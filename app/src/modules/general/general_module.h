#pragma once

#include "framework/module.h"

namespace st::app {

/// Menu "Einstellungen": personal preferences, paths and diagnostics.
class GeneralModule final : public Module {
public:
  std::string_view name() const override { return "general"; }
  void init(AppContext &ctx) override;

private:
  void drawMenu(AppContext &ctx);
};

} // namespace st::app
