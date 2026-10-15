#include "framework/app_context.h"
#include "framework/desktop_notifier.h"
#include "framework/module.h"
#include "framework/platform.h"

#include <st/config/settings.h>
#include <st/util/log.h>
#include <st/util/session.h>

#include <app/app.h>
#include <app/updater.h>
#include <utils/logging.h>

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

/// `ServiceTracker.exe --version` prints the version into the calling console.
bool printVersionIfRequested(int argc, char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) != "--version")
      continue;
#ifdef _WIN32
    const bool attached = AttachConsole(ATTACH_PARENT_PROCESS) != 0;
    if (attached) {
      FILE *tmp = nullptr;
      freopen_s(&tmp, "CONOUT$", "w", stdout);
    }
#endif
    std::cout << '\n'
              << APP_VERSION << std::endl;
#ifdef _WIN32
    if (attached)
      FreeConsole();
#endif
    return true;
  }
  return false;
}

/// Routes the core log into the cpp_app log file (run.log).
void connectLogging() {
  st::log::setSink([](st::log::Level level, std::string_view message) {
    const char *type = level == st::log::Level::Error ? "[ERROR]" : level == st::log::Level::Warning ? "[WARNING]"
                                                                                                       : "[INFO]";
    logging::log(type, std::string(message));
  });
}

} // namespace

int main(int argc, char *argv[]) {
  if (printVersionIfRequested(argc, argv))
    return 0;
  if (app::SetSingleInstance(true, "ServiceTracker"))
    return 1;

  namespace fs = std::filesystem;
  const fs::path workingDir = fs::current_path();
  logging::startlogging(workingDir.string(), "run.log");
  connectLogging();

  // Settings: next to the executable, optional
  const fs::path settingsFile = st::app::platform::executableDir() / "ServiceTracker.json";
  auto settings = st::config::loadSettings(settingsFile);
  std::string settingsError;
  if (!settings) {
    settingsError = settings.error().message;
    settings = st::config::Settings{};
  }

  try {
    st::log::info("ServiceTracker v{}", APP_VERSION);
    updater::InitGit(settings->updateUrl, settings->updateInstaller, APP_VERSION);
    app::Init(1080, 720, "ServiceTracker");
    if (!app::Running())
      return 1; // silent update in progress
    app::SetFont("fonts/JetBrainsMonoNerdFont-Bold.ttf");
    app::SetIcon("icon.png");
    app::AddProgrammInformation("ServiceTracker\tv" + std::string(APP_VERSION) +
                                "\n\nEin Tool zur Nachverfolgung\nund Aufnahme von Servicegeräten\n\nAdrian Jahraus - TechSupport");
    app::DrawStartup("startup.png");

    st::app::AppContext ctx(*settings, st::util::currentUserName(), workingDir);
    if (!settingsError.empty())
      ctx.notify.error("ServiceTracker.json fehlerhaft, Standardwerte aktiv: " + settingsError);
    ctx.alerts.setNotifier(st::app::DesktopNotifier::create());
    ctx.initialize();

    st::app::ModuleRegistry modules;
    st::app::registerModules(modules);
    modules.initAll(ctx);

    while (app::Running()) {
      ctx.update();
      modules.updateAll(ctx);
      app::BeginDrawing();
      modules.drawOverlays(ctx);
      ctx.drawOverlay();
      app::EndDrawing();
    }

    modules.shutdownAll(ctx);
    app::Close();
  } catch (const std::exception &e) {
    LOG_ERROR(e.what());
    logging::stoplogging();
    logging::backuplog(settings->paths.crashDir, true);
    st::app::platform::openPath(workingDir / "run.log");
    return 2;
  }
  logging::stoplogging();
  return EXIT_SUCCESS;
}
