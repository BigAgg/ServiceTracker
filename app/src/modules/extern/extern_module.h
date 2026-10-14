#pragma once

#include "framework/app_context.h"
#include "framework/module.h"

#include <st/service/records.h>

#include <string>
#include <vector>

namespace st::app {

/// External services (service at the customer). Window: "Externe Services".
class ExternModule final : public Module {
public:
  std::string_view name() const override { return "extern"; }
  bool available(const AppContext &ctx) const override;
  void init(AppContext &ctx) override;
  void update(AppContext &ctx) override;

private:
  void reload(AppContext &ctx);
  void drawWindow(AppContext &ctx);
  void drawPlanning(AppContext &ctx);
  void finish(AppContext &ctx);

  std::vector<service::ServiceRecord> mList;
  std::int64_t mListTime = -1;
  PollTimer mPoll;
  std::string mDate;
  std::string mInfo;
};

inline constexpr const char *kExternTemplate = "Kunde: \nVertrag: \nAuftragsnummer: \nGrund: \nNotizen: \n";

} // namespace st::app
