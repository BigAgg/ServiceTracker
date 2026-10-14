#include "st/service/history.h"

#include "st/devices/device.h"
#include "st/util/strings.h"

#include <set>

namespace st::service {

using util::json::Json;

std::string HistoryEntry::servicePath() const {
  if (record.path.empty() || record.activeService.empty())
    return record.path;
  return record.path + "/" + record.activeService + "/";
}

util::Status HistoryStore::ensureExists() const {
  if (util::pathExists(mFile))
    return {};
  return util::json::save(mFile, Json::array());
}

util::Status HistoryStore::append(const ServiceRecord &record, double totalPrice, const std::string &endDate) const {
  Json j = Json::array();
  if (util::pathExists(mFile)) {
    auto loaded = util::json::load(mFile);
    if (!loaded)
      return std::unexpected(loaded.error());
    j = std::move(*loaded);
  }
  if (!j.is_array())
    return std::unexpected(util::Error{"History-Datei hat ein falsches Format: " + util::pathToUtf8(mFile)});

  Json entry = recordToJson(record);
  entry["total_price"] = totalPrice;
  if (!endDate.empty())
    entry["end_date"] = endDate;

  bool found = false;
  for (auto &device : j) {
    if (util::json::str(device, "name") != record.name)
      continue;
    if (!device.contains("services") || !device["services"].is_array())
      device["services"] = Json::array();
    device["services"].push_back(entry);
    found = true;
    break;
  }
  if (!found)
    j.push_back({{"name", record.name}, {"services", Json::array({entry})}});
  return util::json::save(mFile, j);
}

util::Result<std::vector<HistoryEntry>> loadHistoryRows(const util::fs::path &file) {
  auto j = util::json::load(file);
  if (!j)
    return std::unexpected(j.error());
  if (!j->is_array())
    return std::unexpected(util::Error{"Liste hat ein falsches Format: " + util::pathToUtf8(file)});
  std::vector<HistoryEntry> rows;
  for (const auto &device : *j) {
    const std::string name = util::json::str(device, "name");
    if (name.empty())
      continue;
    const auto &services = util::json::array(device, "services");
    const bool historyFormat = !services.empty() && services.front().is_object();
    if (historyFormat) {
      for (const auto &s : services) {
        HistoryEntry e;
        e.device = name;
        e.record = recordFromJson(s);
        if (e.record.activeService.empty())
          continue;
        e.record.name = name;
        e.totalPrice = util::json::number(s, "total_price");
        e.endDate = util::json::str(s, "end_date");
        e.record.extra.erase("total_price");
        e.record.extra.erase("end_date");
        rows.push_back(std::move(e));
      }
    } else if (device.contains("status")) {
      // record list (Sync.json / extern.json)
      HistoryEntry e;
      e.device = name;
      e.record = recordFromJson(device);
      if (e.record.status == status::Done)
        continue;
      rows.push_back(std::move(e));
    }
  }
  return rows;
}

HistoryStatistics computeStatistics(const std::vector<HistoryEntry> &rows, std::string_view project) {
  HistoryStatistics stats;
  long durationSum = 0;
  int durationCount = 0;
  for (const auto &row : rows) {
    if (!project.empty() && devices::projectOf(row.device) != project)
      continue;
    ++stats.services;
    const std::string info = util::toLower(row.record.additionalInfo);
    if (info.find("upgrade") != std::string::npos || info.find("aufrüstung") != std::string::npos)
      ++stats.upgrades;
    if (info.find("repair") != std::string::npos || info.find("reparatur") != std::string::npos)
      ++stats.repairs;
    if (row.totalPrice == 0.0)
      ++stats.goodwill;
    stats.revenue += row.totalPrice;
    if (row.record.duration > 0) {
      durationSum += row.record.duration;
      ++durationCount;
    } else {
      stats.missingDuration.push_back(row.device);
    }
  }
  stats.averageDuration = durationCount > 0 ? static_cast<double>(durationSum) / durationCount : 0.0;
  return stats;
}

std::vector<std::string> projectsOf(const std::vector<HistoryEntry> &rows) {
  std::set<std::string> projects;
  for (const auto &row : rows) {
    std::string p = devices::projectOf(row.device);
    if (!p.empty())
      projects.insert(std::move(p));
  }
  return {projects.begin(), projects.end()};
}

} // namespace st::service
