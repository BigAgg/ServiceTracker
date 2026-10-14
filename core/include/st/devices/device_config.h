#pragma once

#include "st/devices/device.h"
#include "st/util/fs.h"
#include "st/util/json.h"

#include <optional>
#include <string>
#include <vector>

/// Per project configuration (configs.json):
///  - which EEPROM address/value enables which hardware flag
///  - which folders are created for a new service (always / depending on a flag)
///  - which template files (e.g. the checklist) are copied into a new service
namespace st::devices {

struct AddressRule {
  int address = 0;
  ConfigFlags flag = 0;
  int validationValue = -1;
  /// Folders created for a new service if the device has `flag`.
  std::vector<std::string> folders;

  bool valid() const { return address > 0; }
  bool operator==(const AddressRule &) const = default;
};

struct TemplateFile {
  /// Source file (UTF-8 path).
  std::string path;
  /// Target name; placeholders {reader} and {service} are replaced.
  std::string rename;

  bool valid() const { return !path.empty() && !rename.empty(); }
  bool operator==(const TemplateFile &) const = default;
};

struct DeviceConfig {
  std::string project;
  std::vector<AddressRule> addresses;
  std::vector<std::string> folders;
  std::vector<TemplateFile> files;
  /// Flags every device of this project has. If not set in configs.json the
  /// legacy defaults apply (0601 -> ABS, 0471/0472 -> FP).
  std::optional<ConfigFlags> baseFlags;

  bool valid() const { return !project.empty(); }
  ConfigFlags effectiveBaseFlags() const;
  /// Template target name with placeholders replaced.
  static std::string expandName(std::string_view pattern, std::string_view deviceName, std::string_view serviceDate);
  bool operator==(const DeviceConfig &) const = default;
};

/// Flags detected from an EEPROM dump ("*.btc") using the address rules of `config`.
ConfigFlags parseEepromFlags(std::string_view content, const DeviceConfig &config);

/// Loads and saves configs.json. Unknown keys of existing entries are preserved.
class DeviceConfigStore {
public:
  DeviceConfigStore() = default;
  explicit DeviceConfigStore(util::fs::path file) : mFile(std::move(file)) {}

  util::Status load();
  util::Status save() const;

  const std::vector<DeviceConfig> &configs() const { return mConfigs; }
  const DeviceConfig *find(std::string_view project) const;
  std::vector<std::string> projects() const;

  /// Adds or replaces the config of `config.project` and saves.
  util::Status upsert(DeviceConfig config);
  util::Status remove(std::string_view project);

  static DeviceConfig fromJson(const util::json::Json &j);
  static util::json::Json toJson(const DeviceConfig &config, util::json::Json base = {});

private:
  util::fs::path mFile;
  std::vector<DeviceConfig> mConfigs;
  util::json::Json mRaw = util::json::Json::array();
};

} // namespace st::devices
