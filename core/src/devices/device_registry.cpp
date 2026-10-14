#include "st/devices/device_registry.h"

#include "st/util/log.h"
#include "st/util/strings.h"

#include <future>

namespace st::devices {

namespace {

struct ProjectScan {
  std::string project;
  std::vector<Device> devices;
};

ProjectScan scanProject(const util::fs::path &folder) {
  ProjectScan result;
  result.project = projectOfFolder(util::pathToUtf8(folder.filename()));
  if (result.project.empty())
    return result;
  for (const auto &entry : util::listDirectory(folder, true, false)) {
    const std::string name = util::pathToUtf8(entry.filename());
    if (!isValidDeviceName(name))
      continue;
    Device device;
    device.name = name;
    device.path = entry;
    result.devices.push_back(std::move(device));
  }
  return result;
}

} // namespace

bool DeviceRegistry::scan(const util::fs::path &root) {
  mDevices.clear();
  mAccess = util::isDirectory(root);
  if (!mAccess) {
    log::warning("Kein Zugriff auf den Geräteordner \"{}\"", util::pathToUtf8(root));
    return false;
  }
  std::vector<std::future<ProjectScan>> tasks;
  for (const auto &folder : util::listDirectory(root, true, false))
    tasks.push_back(std::async(std::launch::async, scanProject, folder));
  for (auto &task : tasks) {
    try {
      for (auto &device : task.get().devices)
        mDevices.emplace(device.name, std::move(device));
    } catch (const std::exception &e) {
      log::warning("Projektordner konnte nicht gelesen werden: {}", e.what());
    }
  }
  log::info("{} Geräte gefunden", mDevices.size());
  return true;
}

bool DeviceRegistry::add(Device device) {
  if (device.name.empty() || mDevices.contains(device.name))
    return false;
  std::string name = device.name;
  mDevices.emplace(std::move(name), std::move(device));
  return true;
}

const Device *DeviceRegistry::find(std::string_view name) const {
  const auto it = mDevices.find(name);
  return it == mDevices.end() ? nullptr : &it->second;
}

Device *DeviceRegistry::find(std::string_view name) {
  const auto it = mDevices.find(name);
  return it == mDevices.end() ? nullptr : &it->second;
}

std::vector<std::string> DeviceRegistry::projects() const {
  std::vector<std::string> out;
  for (const auto &[name, device] : mDevices) {
    std::string project = projectOf(name);
    if (out.empty() || out.back() != project)
      out.push_back(std::move(project));
  }
  return out;
}

std::vector<std::string> DeviceRegistry::devicesOf(std::string_view project) const {
  return search({}, project);
}

std::vector<std::string> DeviceRegistry::search(std::string_view text, std::string_view project) const {
  std::vector<std::string> out;
  for (const auto &[name, device] : mDevices) {
    if (!project.empty() && projectOf(name) != project)
      continue;
    if (util::containsIgnoreCase(name, text))
      out.push_back(name);
  }
  return out;
}

std::optional<util::fs::path> findNewestFile(const util::fs::path &root, std::string_view extension, std::string_view marker) {
  std::optional<util::fs::path> newest;
  std::int64_t newestTime = 0;
  std::error_code ec;
  for (util::fs::recursive_directory_iterator it(root, util::fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
    if (!it->is_regular_file(ec))
      continue;
    const std::string relative = util::toLower(util::pathToUtf8(util::fs::relative(it->path(), root, ec)));
    if (!relative.ends_with(extension) || relative.find(marker) == std::string::npos)
      continue;
    const std::int64_t time = util::lastWriteTime(it->path());
    if (!newest || time > newestTime) {
      newest = it->path();
      newestTime = time;
    }
  }
  return newest;
}

void populateDevice(Device &device, const DeviceConfig *config) {
  if (config) {
    device.flags = config->effectiveBaseFlags();
    if (auto eeprom = findNewestFile(device.path, ".btc", "eeprom")) {
      if (auto content = util::readFile(*eeprom))
        device.flags |= parseEepromFlags(*content, *config);
    }
  }
  device.info = {};
  if (auto infoFile = findNewestFile(device.path, ".txt", "info")) {
    if (auto content = util::readFile(*infoFile)) {
      device.info = parseDeviceInfo(*content);
      device.info.fileDate = util::lastWriteTimeString(*infoFile);
    }
  }
}

} // namespace st::devices
