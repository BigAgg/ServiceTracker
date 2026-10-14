#pragma once

#include <memory>
#include <string_view>
#include <vector>

namespace st::app {

class AppContext;

/// A feature of the program (e.g. "Bestellungen"). Each module registers its own
/// windows/menus/popups, expressions and event handlers in `init` and keeps its
/// UI state as members. See docs/module.md for a step by step guide.
class Module {
public:
  virtual ~Module() = default;

  /// Short technical name (logging, diagnostics).
  virtual std::string_view name() const = 0;

  /// Whether the module is active for the current user/installation
  /// (permissions, reachable paths). Unavailable modules are not initialised.
  virtual bool available(const AppContext &) const { return true; }

  /// Called once at start-up: register windows, menus, expressions, event handlers.
  virtual void init(AppContext &ctx) = 0;

  /// Called every frame before drawing. No ImGui calls here – use it for polling
  /// files, timers and background work.
  virtual void update(AppContext &) {}

  /// Called every frame inside the ImGui frame after the docked windows. Use it for
  /// floating dialogs that are opened from a main menu entry (menus close
  /// immediately, so dialogs cannot live inside the menu).
  virtual void drawOverlay(AppContext &) {}

  /// Called once before the program exits (save state).
  virtual void shutdown(AppContext &) {}
};

/// Owns all modules and calls them in registration order.
class ModuleRegistry {
public:
  template <typename T, typename... Args>
  T &add(Args &&...args) {
    auto module = std::make_unique<T>(std::forward<Args>(args)...);
    T &ref = *module;
    mModules.push_back(std::move(module));
    return ref;
  }

  void initAll(AppContext &ctx);
  void updateAll(AppContext &ctx);
  void drawOverlays(AppContext &ctx);
  void shutdownAll(AppContext &ctx);

  /// Names of the initialised modules.
  std::vector<std::string_view> activeModules() const;

private:
  std::vector<std::unique_ptr<Module>> mModules;
  std::vector<Module *> mActive;
};

/// Registers all modules of the program (modules/register_modules.cpp).
void registerModules(ModuleRegistry &registry);

} // namespace st::app
