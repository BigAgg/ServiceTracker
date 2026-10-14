#include "st/service/records.h"

#include <algorithm>
#include <iterator>

namespace st::service {

using util::json::Json;

namespace {
constexpr const char *kKnownKeys[] = {"name", "path", "status", "activeservice", "syncfolder", "additionalinfo", "services", "duration"};
}

bool isClosedStatus(std::string_view value) {
  return value.empty() || value == status::Done || value == status::DoneExternal;
}

ServiceRecord recordFromJson(const Json &j) {
  ServiceRecord r;
  r.name = util::json::str(j, "name");
  r.path = util::json::str(j, "path");
  r.status = util::json::str(j, "status");
  r.activeService = util::json::str(j, "activeservice");
  r.syncFolder = util::json::str(j, "syncfolder");
  r.additionalInfo = util::json::str(j, "additionalinfo");
  r.services = util::json::strings(j, "services");
  r.duration = static_cast<long>(util::json::integer(j, "duration", 0));
  if (j.is_object()) {
    for (auto it = j.begin(); it != j.end(); ++it) {
      if (std::find(std::begin(kKnownKeys), std::end(kKnownKeys), it.key()) == std::end(kKnownKeys))
        r.extra[it.key()] = it.value();
    }
  }
  return r;
}

Json recordToJson(const ServiceRecord &r) {
  Json j = r.extra.is_object() ? r.extra : Json::object();
  j["name"] = r.name;
  j["path"] = r.path;
  j["status"] = r.status;
  j["activeservice"] = r.activeService;
  j["syncfolder"] = r.syncFolder;
  j["additionalinfo"] = r.additionalInfo;
  j["services"] = r.services;
  j["duration"] = r.duration;
  return j;
}

util::Status RecordStore::ensureExists() const {
  if (util::pathExists(mFile))
    return {};
  return util::json::save(mFile, Json::array());
}

util::Result<Json> RecordStore::read() const {
  if (!util::pathExists(mFile))
    return Json::array();
  auto j = util::json::load(mFile);
  if (!j)
    return j;
  if (!j->is_array())
    return std::unexpected(util::Error{"Falsches Format (Liste erwartet): " + util::pathToUtf8(mFile)});
  return j;
}

util::Result<std::vector<ServiceRecord>> RecordStore::loadAll() const {
  auto j = read();
  if (!j)
    return std::unexpected(j.error());
  std::vector<ServiceRecord> out;
  for (const auto &entry : *j) {
    ServiceRecord r = recordFromJson(entry);
    if (!r.name.empty())
      out.push_back(std::move(r));
  }
  return out;
}

util::Result<std::optional<ServiceRecord>> RecordStore::find(std::string_view name) const {
  auto all = loadAll();
  if (!all)
    return std::unexpected(all.error());
  for (auto &r : *all)
    if (r.name == name)
      return std::optional<ServiceRecord>(std::move(r));
  return std::optional<ServiceRecord>();
}

util::Status RecordStore::upsert(const ServiceRecord &record) const {
  auto j = read();
  if (!j)
    return std::unexpected(j.error());
  const Json original = *j;
  bool found = false;
  for (auto &entry : *j) {
    if (util::json::str(entry, "name") == record.name) {
      entry.merge_patch(recordToJson(record));
      found = true;
      break;
    }
  }
  if (!found)
    j->push_back(recordToJson(record));
  if (*j == original)
    return {};
  return util::json::save(mFile, *j);
}

util::Status RecordStore::remove(std::string_view name) const {
  auto j = read();
  if (!j)
    return std::unexpected(j.error());
  const auto before = j->size();
  Json filtered = Json::array();
  for (auto &entry : *j)
    if (util::json::str(entry, "name") != name)
      filtered.push_back(std::move(entry));
  if (filtered.size() == before)
    return {};
  return util::json::save(mFile, filtered);
}

} // namespace st::service
