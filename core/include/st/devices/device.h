#pragma once

#include "st/util/fs.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// Devices ("Reader") and their hardware configuration.
///
/// A device lives in `<devicesRoot>/<project folder>/<PROJECT>-<SERIAL>/`, e.g.
/// `Y:/Gerätenachweise/0601 FLUOstar/0601-1234/`. The project folder must start
/// with '0'; its first word is the project number.
namespace st::devices {

/// Hardware options detected from the EEPROM dump. Values are stored in configs.json
/// and must never change.
enum class ConfigFlag : std::uint32_t {
  None = 0,
  FI = 0x01,
  ABS = 0x02,
  LUM = 0x04,
  TRF = 0x08,
  ALPHA = 0x10,
  EXT_INC = 0x20,
  FP = 0x40,
  TRF_LASER = 0x80,
  LTS = 0x100,
};

using ConfigFlags = std::uint32_t;

struct FlagInfo {
  ConfigFlag flag;
  /// Key used in expressions ("@rinfo.config.<key>") and configs.json.
  std::string_view key;
  /// Label shown in the UI.
  std::string_view label;
};

/// All known flags. Add new hardware options here (and nowhere else).
inline constexpr FlagInfo kFlags[] = {
    {ConfigFlag::FI, "fi", "FI"},
    {ConfigFlag::TRF, "trf", "TRF"},
    {ConfigFlag::TRF_LASER, "trf_laser", "TRF Laser"},
    {ConfigFlag::FP, "fp", "FP"},
    {ConfigFlag::LUM, "lum", "LUM"},
    {ConfigFlag::ABS, "abs", "ABS"},
    {ConfigFlag::ALPHA, "alpha", "Alphascreen"},
    {ConfigFlag::EXT_INC, "ext_inc", "65°"},
    {ConfigFlag::LTS, "lts", "LTS"},
};

constexpr bool hasFlag(ConfigFlags flags, ConfigFlag flag) {
  return (flags & static_cast<ConfigFlags>(flag)) != 0;
}

/// Label of a single flag ("None" for 0 or unknown values).
std::string_view flagLabel(ConfigFlags flag);
/// Flag by key ("abs" -> ABS).
std::optional<ConfigFlag> flagFromKey(std::string_view key);

/// Counters read from the newest "*info*.txt" file of a device.
struct DeviceInfo {
  std::string readerActiveTime;
  int executedTestruns = 0;
  int processedWells = 0;
  int wellMovements = 0;
  std::string shakingTime;
  int performedFlashesLamp = 0;
  int performedFlashesLaser = 0;
  std::string alphascreenLaserActiveTime;
  int pump1Strokes = 0;
  int pump2Strokes = 0;
  /// Modification time of the info file.
  std::string fileDate;

  bool operator==(const DeviceInfo &) const = default;
};

struct Device {
  /// "<PROJECT>-<SERIAL>", e.g. "0601-1234"
  std::string name;
  util::fs::path path;
  ConfigFlags flags = 0;
  DeviceInfo info;

  std::string project() const;
  std::string serial() const;
  explicit operator bool() const { return !name.empty(); }
};

/// "0601-1234" is valid; the project part must start with '0' and have at most 4 characters.
bool isValidDeviceName(std::string_view name);
std::string projectOf(std::string_view deviceName);
std::string serialOf(std::string_view deviceName);

/// Project number of a project folder ("0601 FLUOstar" -> "0601"), empty if invalid.
std::string projectOfFolder(std::string_view folderName);

/// Parses the content of an info file (lines "Key: value", dots in numbers are thousands separators).
DeviceInfo parseDeviceInfo(std::string_view content);

} // namespace st::devices
