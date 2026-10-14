#include "st/devices/device.h"

#include "st/util/strings.h"

#include <cctype>

namespace st::devices {

std::string_view flagLabel(ConfigFlags flag) {
  for (const auto &info : kFlags)
    if (static_cast<ConfigFlags>(info.flag) == flag)
      return info.label;
  return "None";
}

std::optional<ConfigFlag> flagFromKey(std::string_view key) {
  const std::string lower = util::toLower(key);
  for (const auto &info : kFlags)
    if (info.key == lower)
      return info.flag;
  return std::nullopt;
}

std::string Device::project() const { return projectOf(name); }
std::string Device::serial() const { return serialOf(name); }

bool isValidDeviceName(std::string_view name) {
  if (name.empty() || name.front() != '0' || name.find('-') == std::string_view::npos)
    return false;
  const auto [project, serial] = util::splitAt(name, "-");
  return !project.empty() && !serial.empty() && project.size() <= 4;
}

std::string projectOf(std::string_view deviceName) { return util::splitAt(deviceName, "-").first; }
std::string serialOf(std::string_view deviceName) { return util::splitAt(deviceName, "-").second; }

std::string projectOfFolder(std::string_view folderName) {
  if (folderName.empty() || folderName.front() != '0')
    return {};
  return util::splitAt(folderName, " ").first;
}

DeviceInfo parseDeviceInfo(std::string_view content) {
  DeviceInfo info;
  for (std::string line : util::splitLines(content)) {
    // Same normalisation as the legacy parser: dots are thousands separators
    util::replaceAll(line, ".", "");
    std::erase_if(line, [](unsigned char c) { return c != ' ' && std::isspace(c); });
    auto [key, value] = util::splitAt(line, ":");
    key = util::trim(key);
    value = util::trim(value);
    const auto number = [&] { return util::parseInt(value).value_or(0); };
    if (key == "Reader active time")
      info.readerActiveTime = value;
    else if (key == "Executed testruns")
      info.executedTestruns = number();
    else if (key == "Processed wells")
      info.processedWells = number();
    else if (key == "Well movements")
      info.wellMovements = number();
    else if (key == "Shaking active time")
      info.shakingTime = value;
    else if (key == "Performed flashes (flash lamp)")
      info.performedFlashesLamp = number();
    else if (key == "Performed flashes (TRF laser)")
      info.performedFlashesLaser = number();
    else if (key == "AlphaScreen laser active time")
      info.alphascreenLaserActiveTime = value;
    else if (key == "Pump 1 strokes")
      info.pump1Strokes = number();
    else if (key == "Pump 2 strokes")
      info.pump2Strokes = number();
  }
  return info;
}

} // namespace st::devices
