#pragma once

#include "framework/module.h"

#include <st/service/device_filter.h>
#include <st/util/fs.h>

#include <string>
#include <vector>

namespace st::app {

/// Device selection and device details.
/// Windows: "Reader Control", "Reader Filewalker", "ReaderInfo", "Konfiguration".
class DevicesModule final : public Module {
public:
  std::string_view name() const override { return "devices"; }
  void init(AppContext &ctx) override;

private:
  void drawControl(AppContext &ctx);
  void drawFilewalker(AppContext &ctx);
  void drawInfo(AppContext &ctx);
  void drawConfiguration(AppContext &ctx);
  void refilter(AppContext &ctx);
  /// Reports all devices "im Wareneingang" to the notification system.
  void reportReceived(AppContext &ctx);
  void openFolder(const util::fs::path &folder);

  service::DeviceFilter mFilter;
  std::vector<std::string> mFiltered;
  bool mDirty = true;

  // Filewalker
  util::fs::path mBrowseRoot;
  util::fs::path mBrowsePath;
  std::vector<util::fs::path> mEntries;
  std::string mFileSearch;
};

} // namespace st::app
