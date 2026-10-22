#pragma once

#include "st/util/fs.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/// Status notifications ("Status-Benachrichtigungen"): which departments get a Windows
/// notification when a device appears in the service list with a certain status.
///
/// Stored in configs/statusAlerts.json (shared, edited by power users):
///   [{"status": "versendet", "groups": ["TECHSUPPORT"]}, ...]
/// Without the file the default rules apply.
namespace st::alerts {

struct StatusRule {
  /// Status text as set in Sync.json (compared case-insensitively).
  std::string status;
  /// Departments (user groups) that are notified.
  std::vector<std::string> groups;

  bool operator==(const StatusRule &) const = default;
};

/// Rules used while no statusAlerts.json exists.
std::vector<StatusRule> defaultStatusRules();

/// Notification channel id of a rule ("status.<status>"). Stored in the preferences
/// (muted channels) – changes when the status text changes.
std::string statusChannelId(std::string_view status);

/// Removes rules without status or groups, merges rules with the same status
/// (case-insensitive) and sorts the groups.
std::vector<StatusRule> normalizeStatusRules(std::vector<StatusRule> rules);

class StatusRuleStore {
public:
  StatusRuleStore() = default;
  explicit StatusRuleStore(util::fs::path file) : mFile(std::move(file)) {}

  const util::fs::path &file() const { return mFile; }

  /// Reads the rules; a missing file results in the default rules.
  util::Status load();
  /// Writes the rules (normalized) and keeps them.
  util::Status save(std::vector<StatusRule> rules);

  const std::vector<StatusRule> &rules() const { return mRules; }
  /// True if the file changed on disk since the last load/save.
  bool changedOnDisk() const { return util::lastWriteTime(mFile) != mLastWrite; }

private:
  util::fs::path mFile;
  std::vector<StatusRule> mRules = defaultStatusRules();
  std::int64_t mLastWrite = 0;
};

} // namespace st::alerts
