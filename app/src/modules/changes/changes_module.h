#pragma once

#include "framework/module.h"

#include <st/changes/change_requests.h>

#include <string>
#include <vector>

namespace st::app {

/// Change requests ("Änderungswünsche"). Menu "Änderungen" with two dialogs.
class ChangesModule final : public Module {
public:
  std::string_view name() const override { return "changes"; }
  bool available(const AppContext &ctx) const override;
  void init(AppContext &ctx) override;
  void drawOverlay(AppContext &ctx) override;
  /// Opens the dialogs.
  void showSubmit() { mSubmitOpen = true; }
  void showOverview() { mOverviewOpen = true; }

private:
  void reload(AppContext &ctx);
  void drawSubmit(AppContext &ctx);
  void drawOverview(AppContext &ctx);

  changes::ChangeRequestStore mStore;
  std::vector<changes::ChangeRequest> mRequests;
  bool mSubmitOpen = false;
  bool mOverviewOpen = false;
  std::string mSubject;
  std::string mMessage;
  std::vector<std::string> mAttachments;
  std::uint64_t mSelected = 0;
};

} // namespace st::app
