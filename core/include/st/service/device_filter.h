#pragma once

#include "st/devices/device_registry.h"
#include "st/service/records.h"

#include <map>
#include <string>
#include <vector>

namespace st::service {

/// Status of every device that is in the service list (Sync.json).
class StatusIndex {
public:
  static StatusIndex fromRecords(const std::vector<ServiceRecord> &records);

  /// Status of a device ("" if not in the list).
  std::string statusOf(std::string_view device) const;
  /// All non empty status values, sorted.
  std::vector<std::string> statuses() const;
  /// Devices with a status, sorted.
  std::vector<std::string> devices() const;
  std::size_t count(std::string_view status) const;
  /// Devices with exactly `status`, sorted.
  std::vector<std::string> devicesWithStatus(std::string_view status) const;

private:
  std::map<std::string, std::string, std::less<>> mStatus;
};

struct DeviceFilter {
  std::string text;
  std::string project;
  std::string status;
  /// Without any filter only devices currently worked on are listed. With this flag
  /// expected and finished devices are listed as well.
  bool showClosed = false;

  bool empty() const { return text.empty() && project.empty() && status.empty(); }
};

/// Device list of "Reader Control":
///  - no filter:    devices in the service list (closed/expected ones hidden unless showClosed)
///  - text/project: search in all devices
///  - status:       devices with that status (combined with text/project)
std::vector<std::string> filterDevices(const devices::DeviceRegistry &registry, const StatusIndex &index, const DeviceFilter &filter);

/// Adds devices known from the service list but missing in the registry (users without
/// access to the devices folder still see the list). Returns the number of added devices.
std::size_t addMissingDevices(devices::DeviceRegistry &registry, const std::vector<ServiceRecord> &records);

} // namespace st::service
