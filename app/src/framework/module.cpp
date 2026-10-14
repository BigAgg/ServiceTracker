#include "framework/module.h"

#include "framework/app_context.h"

#include <st/util/log.h>

namespace st::app {

void ModuleRegistry::initAll(AppContext &ctx) {
  mActive.clear();
  for (auto &module : mModules) {
    if (!module->available(ctx)) {
      log::info("Modul \"{}\" ist für diesen Benutzer nicht aktiv", module->name());
      continue;
    }
    try {
      module->init(ctx);
      mActive.push_back(module.get());
      log::info("Modul \"{}\" initialisiert", module->name());
    } catch (const std::exception &e) {
      ctx.notify.error("Modul \"" + std::string(module->name()) + "\" konnte nicht gestartet werden: " + e.what());
    }
  }
}

void ModuleRegistry::updateAll(AppContext &ctx) {
  for (auto *module : mActive)
    module->update(ctx);
}

void ModuleRegistry::drawOverlays(AppContext &ctx) {
  for (auto *module : mActive)
    module->drawOverlay(ctx);
}

void ModuleRegistry::shutdownAll(AppContext &ctx) {
  for (auto it = mActive.rbegin(); it != mActive.rend(); ++it) {
    try {
      (*it)->shutdown(ctx);
    } catch (const std::exception &e) {
      log::error("Fehler beim Beenden von Modul \"{}\": {}", (*it)->name(), e.what());
    }
  }
}

std::vector<std::string_view> ModuleRegistry::activeModules() const {
  std::vector<std::string_view> out;
  for (const auto *m : mActive)
    out.push_back(m->name());
  return out;
}

} // namespace st::app
