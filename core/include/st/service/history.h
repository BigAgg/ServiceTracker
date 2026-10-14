#pragma once

#include "st/service/records.h"

#include <map>
#include <string>
#include <vector>

/// Service history (History.json): one entry per device with all finished services.
///
/// Format: [{"name": "0601-1234", "services": [<record> + "total_price" + "end_date"]}]
namespace st::service {

struct HistoryEntry {
  /// Device name.
  std::string device;
  ServiceRecord record;
  double totalPrice = 0.0;
  std::string endDate;

  /// Folder of the service (device path + service folder).
  std::string servicePath() const;
};

/// Appends finished services to History.json.
class HistoryStore {
public:
  HistoryStore() = default;
  explicit HistoryStore(util::fs::path file) : mFile(std::move(file)) {}

  const util::fs::path &file() const { return mFile; }
  util::Status ensureExists() const;
  util::Status append(const ServiceRecord &record, double totalPrice, const std::string &endDate) const;

private:
  util::fs::path mFile;
};

/// Loads history rows from any supported list:
///  - history format (History.json, oldhistory/*.json)
///  - record lists (Sync.json, extern.json); finished entries are skipped there
util::Result<std::vector<HistoryEntry>> loadHistoryRows(const util::fs::path &file);

/// Key figures for the yearly export.
struct HistoryStatistics {
  int services = 0;
  int upgrades = 0;
  int repairs = 0;
  /// Services without price (warranty / goodwill).
  int goodwill = 0;
  double averageDuration = 0.0;
  double revenue = 0.0;
  /// Device names without a recorded duration.
  std::vector<std::string> missingDuration;
};

/// Statistics over all rows (project empty) or over one project.
HistoryStatistics computeStatistics(const std::vector<HistoryEntry> &rows, std::string_view project = {});

/// Project numbers that occur in `rows`, sorted.
std::vector<std::string> projectsOf(const std::vector<HistoryEntry> &rows);

} // namespace st::service
