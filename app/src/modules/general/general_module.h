#pragma once

#include "framework/module.h"

namespace st::app {

/// Menu "Einstellungen": personal preferences, paths and diagnostics.
class GeneralModule final : public Module {
public:
  std::string_view name() const override { return "general"; }
  void init(AppContext &ctx) override;

  /// Content of the menu (public for the smoke test, which renders it in a window).
  void drawMenu(AppContext &ctx);

private:
  /// "Benachrichtigungen" section (only for users that receive any channel).
  /// Returns true if a preference changed.
  bool drawNotificationSettings(AppContext &ctx);
};

} // namespace st::app
