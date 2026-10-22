#pragma once

#include "framework/app_context.h"
#include "framework/module.h"
#include "framework/widgets.h"

#include <st/alerts/status_rules.h>

#include <string>
#include <vector>

namespace st::app {

/// Status notifications: Windows notification for the configured departments when a
/// device appears in the service list with a certain status (e.g. "versendet" ->
/// TECHSUPPORT). Rules live in configs/statusAlerts.json; power users edit them in the
/// dialog "Status-Benachrichtigungen" (menu "Configs"). See docs/module/statusalerts.md.
class StatusAlertsModule final : public Module {
public:
  std::string_view name() const override { return "statusalerts"; }
  void init(AppContext &ctx) override;
  void update(AppContext &ctx) override;
  void drawOverlay(AppContext &ctx) override;

  /// Opens the editor dialog.
  void show(const AppContext &ctx);

private:
  /// Registers one channel per rule the user receives; removes channels of deleted rules.
  void registerChannels(AppContext &ctx);
  /// Reports the devices with the status of each rule.
  void report(AppContext &ctx);
  void drawEditor(AppContext &ctx);

  alerts::StatusRuleStore mStore;
  /// Channel ids currently registered.
  std::vector<std::string> mChannels;
  PollTimer mPoll;

  // editor
  bool mOpen = false;
  bool mDirty = false;
  std::vector<alerts::StatusRule> mDraft;
  std::vector<ui::AutocompleteState> mStatusInput;
};

} // namespace st::app
