#pragma once

#include "framework/app_context.h"
#include "framework/module.h"

#include <st/service/history.h>

#include <string>
#include <vector>

namespace st::app {

/// Service history and yearly Excel export. Window: "Service Historie".
class HistoryModule final : public Module {
public:
  std::string_view name() const override { return "history"; }
  bool available(const AppContext &ctx) const override;
  void init(AppContext &ctx) override;
  void update(AppContext &ctx) override;

private:
  void reload(AppContext &ctx);
  void applyFilter();
  void sortRows(int column, bool ascending);
  void drawWindow(AppContext &ctx);
  void drawExportPopup(AppContext &ctx);
  std::vector<util::fs::path> sources(const AppContext &ctx) const;

  util::fs::path mSource;
  std::int64_t mSourceTime = 0;
  PollTimer mPoll;
  std::vector<service::HistoryEntry> mRows;
  std::vector<std::size_t> mVisible;
  std::string mSearch;
  std::string mYear;
  std::string mExportFile;
  int mSortColumn = 0;
  bool mSortAscending = true;
};

} // namespace st::app
