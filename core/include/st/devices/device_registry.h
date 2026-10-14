#pragma once

#include "st/devices/device.h"
#include "st/devices/device_config.h"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace st::devices {

/// Index of all devices below the devices root folder.
class DeviceRegistry {
public:
  /// Scans `root` (one thread per project folder). Returns false if the root is not
  /// accessible; in that case the registry stays empty and `hasAccess()` is false.
  bool scan(const util::fs::path &root);

  /// False if the device folder could not be read (e.g. sales users without access).
  bool hasAccess() const { return mAccess; }

  /// Adds a device that is not on disk (e.g. known only from the sync file). Returns false if it exists.
  bool add(Device device);

  const Device *find(std::string_view name) const;
  Device *find(std::string_view name);

  std::size_t size() const { return mDevices.size(); }
  /// Project numbers, sorted.
  std::vector<std::string> projects() const;
  /// Device names of a project, sorted.
  std::vector<std::string> devicesOf(std::string_view project) const;
  /// Case insensitive substring search over device names (optionally limited to a project). Sorted.
  std::vector<std::string> search(std::string_view text, std::string_view project = {}) const;

private:
  std::map<std::string, Device, std::less<>> mDevices;
  bool mAccess = false;
};

/// Newest file below `root` whose path (relative to root, lower case) ends with
/// `extension` and contains `marker`.
std::optional<util::fs::path> findNewestFile(const util::fs::path &root, std::string_view extension, std::string_view marker);

/// Reads flags (EEPROM, base flags) and counters (info file) of a device from disk.
/// Without a config the flags are left unchanged (legacy behaviour).
void populateDevice(Device &device, const DeviceConfig *config);

} // namespace st::devices
