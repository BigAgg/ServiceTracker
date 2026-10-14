#pragma once

#include "framework/module.h"

#include <st/checklist/export.h>

#include <string>

namespace st::app {

/// In house service workflow. Window: "Service Control".
/// Registers the "@rinfo.*" expressions and the "update_status_rinfo" section action.
class ServiceModule final : public Module {
public:
  std::string_view name() const override { return "service"; }
  void init(AppContext &ctx) override;

  /// HTML options for service records (title, footer, optional style.txt).
  static checklist::HtmlOptions htmlOptions(const AppContext &ctx);

private:
  void registerExpressions(AppContext &ctx);
  void drawWindow(AppContext &ctx);
  void drawExpectPopup(AppContext &ctx);
  void drawFinishPopup(AppContext &ctx);
  void finishService(AppContext &ctx);

  std::string mInfoBuffer;
  std::string mInfoBufferDevice;
  std::string mExpectInfo;
  std::string mManualStatus;
};

/// Template for the additional information of a new service.
inline constexpr const char *kExpectTemplate = "Kunde: \nAnsprechpartner: \nGrund: \nAngebot: \nAuftrag: \nDekontaminationsbericht: ";

} // namespace st::app
