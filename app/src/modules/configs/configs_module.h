#pragma once

#include "framework/module.h"

#include <st/devices/device_config.h>

#include <string>

namespace st::app {

/// Device configurations (configs.json). Menu "Configs" (power users) with an editor dialog.
class ConfigsModule final : public Module {
public:
  std::string_view name() const override { return "configs"; }
  bool available(const AppContext &ctx) const override;
  void init(AppContext &ctx) override;
  void drawOverlay(AppContext &ctx) override;
  /// Opens the editor dialog (optionally with a project selected).
  void show(std::string project = {}) {
    mOpen = true;
    mPendingSelect = std::move(project);
  }

private:
  void drawEditor(AppContext &ctx);
  void select(AppContext &ctx, const std::string &project);

  bool mOpen = false;
  std::string mPendingSelect;
  std::string mSelected;
  devices::DeviceConfig mDraft;
  bool mDirty = false;
  std::string mNewProject;
  std::string mNewFolder;
  devices::AddressRule mNewRule;
  std::string mNewRuleFolder;
};

} // namespace st::app
