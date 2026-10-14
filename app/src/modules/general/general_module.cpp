#include "modules/general/general_module.h"

#include "framework/app_context.h"
#include "framework/events.h"
#include "framework/platform.h"
#include "framework/widgets.h"

#include <imgui.h>

namespace st::app {

void GeneralModule::init(AppContext &ctx) {
  ctx.ui.addMenu("Einstellungen", [this, &ctx] { drawMenu(ctx); });
}

void GeneralModule::drawMenu(AppContext &ctx) {
  auto &prefs = ctx.prefs();
  bool changed = false;
  ImGui::SeparatorText("Checkliste");
  changed |= ImGui::Checkbox("Nur Arbeitsgänge mit Berechtigungen anzeigen", &prefs.onlyPermittedSections);
  changed |= ImGui::Checkbox("Deutsches Zeitformat", &prefs.germanDates);
  if (ctx.isPoweruser()) {
    changed |= ImGui::Checkbox("Fertiggemeldet ignorieren", &prefs.ignoreFinished);
    ui::help("Fertig gemeldete Arbeitsgänge bleiben bearbeitbar.");
    changed |= ImGui::Checkbox("Berechtigungen ignorieren", &prefs.ignorePermissions);
    ui::help("Alle Arbeitsgänge sind bearbeitbar, unabhängig von der Abteilung.");
  }
  ImGui::SeparatorText("Listen");
  changed |= ImGui::Checkbox("Erwartete und erledigte Geräte anzeigen", &prefs.showClosedDevices);
  ui::help("Reader Control zeigt ohne Filter sonst nur Geräte, die gerade bearbeitet werden.");
  changed |= ImGui::Checkbox("Bestellungen als Popup öffnen", &prefs.ordersAsPopup);
  if (!prefs.recentDevices.empty() && ImGui::Button("Zuletzt verwendete Geräte leeren")) {
    prefs.recentDevices.clear();
    changed = true;
  }
  if (changed) {
    prefs.restrictTo(ctx.isPoweruser());
    ctx.savePrefs();
    ctx.events.publish(events::PreferencesChanged{});
  }

  ImGui::SeparatorText("Info");
  ImGui::Text("Benutzer: %s%s", ctx.user().c_str(), ctx.isPoweruser() ? " (Poweruser)" : "");
  ImGui::Text("Version: %s", APP_VERSION);
  if (ImGui::BeginMenu("Pfade")) {
    const auto &p = ctx.paths();
    const auto row = [](const char *label, const std::string &path) {
      ImGui::Text("%s:", label);
      ImGui::SameLine();
      if (ImGui::SmallButton(path.c_str()))
        platform::openPath(util::pathFromUtf8(path));
      ImGui::SetItemTooltip("Öffnen");
    };
    row("Geräte", p.devicesRoot);
    row("Sync", p.syncDir);
    row("Configs", p.configsDir);
    row("Benutzer", p.usersFile);
    row("Bestellungen", p.ordersFile);
    row("Extern (lokal)", p.externLocalDir);
    ImGui::Separator();
    ui::hintText("Pfade werden in ServiceTracker.json neben der .exe konfiguriert (siehe Doku).");
    ImGui::EndMenu();
  }
  if (ImGui::MenuItem("Log-Datei öffnen"))
    platform::openPath(ctx.workingDir() / "run.log");
  if (ImGui::MenuItem("Anleitung öffnen"))
    platform::openPath(platform::executableDir() / "Anleitung.pdf");
}

} // namespace st::app
