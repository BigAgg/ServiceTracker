#include "st/service/device_filter.h"

#include "st/util/strings.h"

#include <algorithm>
#include <set>

namespace st::service {

StatusIndex StatusIndex::fromRecords(const std::vector<ServiceRecord> &records) {
  StatusIndex index;
  for (const auto &r : records)
    if (!r.name.empty() && !r.status.empty())
      index.mStatus[r.name] = r.status;
  return index;
}

std::string StatusIndex::statusOf(std::string_view device) const {
  const auto it = mStatus.find(device);
  return it == mStatus.end() ? std::string() : it->second;
}

std::vector<std::string> StatusIndex::statuses() const {
  std::set<std::string> set;
  for (const auto &[device, s] : mStatus)
    set.insert(s);
  return {set.begin(), set.end()};
}

std::vector<std::string> StatusIndex::devices() const {
  std::vector<std::string> out;
  for (const auto &[device, s] : mStatus)
    out.push_back(device);
  return out;
}

std::size_t StatusIndex::count(std::string_view value) const {
  return static_cast<std::size_t>(std::count_if(mStatus.begin(), mStatus.end(), [&](const auto &p) { return p.second == value; }));
}

std::vector<std::string> StatusIndex::devicesWithStatus(std::string_view value) const {
  std::vector<std::string> out;
  for (const auto &[device, s] : mStatus)
    if (s == value)
      out.push_back(device);
  return out;
}

std::vector<std::string> filterDevices(const devices::DeviceRegistry &registry, const StatusIndex &index, const DeviceFilter &filter) {
  std::vector<std::string> out;
  if (filter.empty()) {
    for (const auto &device : index.devices()) {
      const std::string s = index.statusOf(device);
      const bool hidden = isClosedStatus(s) || s == status::Expected || s == status::PlannedExternal;
      if (!hidden || filter.showClosed)
        out.push_back(device);
    }
    return out;
  }
  if (filter.status.empty())
    return registry.search(filter.text, filter.project);
  for (const auto &device : index.devices()) {
    if (index.statusOf(device) != filter.status)
      continue;
    if (!filter.project.empty() && devices::projectOf(device) != filter.project)
      continue;
    if (!util::containsIgnoreCase(device, filter.text))
      continue;
    out.push_back(device);
  }
  return out;
}

std::size_t addMissingDevices(devices::DeviceRegistry &registry, const std::vector<ServiceRecord> &records) {
  std::size_t added = 0;
  for (const auto &r : records) {
    if (r.name.empty() || registry.find(r.name))
      continue;
    devices::Device device;
    device.name = r.name;
    device.path = util::pathFromUtf8(r.path);
    if (registry.add(std::move(device)))
      ++added;
  }
  return added;
}

} // namespace st::service
