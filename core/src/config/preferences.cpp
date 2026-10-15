#include "st/config/preferences.h"

#include "st/util/json.h"

#include <algorithm>

namespace st::config {

using util::json::Json;

void Preferences::rememberDevice(const std::string &device, std::size_t limit) {
  if (device.empty())
    return;
  std::erase(recentDevices, device);
  recentDevices.insert(recentDevices.begin(), device);
  if (recentDevices.size() > limit)
    recentDevices.resize(limit);
}

void Preferences::restrictTo(bool poweruser) {
  if (!poweruser) {
    ignoreFinished = false;
    ignorePermissions = false;
  }
}

Preferences loadPreferences(const util::fs::path &file, const util::fs::path &legacyChecklistBin) {
  Preferences prefs;
  if (util::pathExists(file)) {
    if (auto j = util::json::load(file); j && j->is_object()) {
      prefs.onlyPermittedSections = util::json::boolean(*j, "onlyPermittedSections", prefs.onlyPermittedSections);
      prefs.germanDates = util::json::boolean(*j, "germanDates", prefs.germanDates);
      prefs.ignoreFinished = util::json::boolean(*j, "ignoreFinished", prefs.ignoreFinished);
      prefs.ignorePermissions = util::json::boolean(*j, "ignorePermissions", prefs.ignorePermissions);
      prefs.showClosedDevices = util::json::boolean(*j, "showClosedDevices", prefs.showClosedDevices);
      prefs.ordersAsPopup = util::json::boolean(*j, "ordersAsPopup", prefs.ordersAsPopup);
      prefs.desktopNotifications = util::json::boolean(*j, "desktopNotifications", prefs.desktopNotifications);
      prefs.taskbarBadge = util::json::boolean(*j, "taskbarBadge", prefs.taskbarBadge);
      prefs.mutedAlerts = util::json::strings(*j, "mutedAlerts");
      prefs.recentDevices = util::json::strings(*j, "recentDevices");
    }
    return prefs;
  }
  // Legacy checklist.bin: four raw bools
  if (!legacyChecklistBin.empty() && util::pathExists(legacyChecklistBin)) {
    if (auto bytes = util::readFile(legacyChecklistBin); bytes && bytes->size() >= 4) {
      prefs.onlyPermittedSections = (*bytes)[0] != 0;
      prefs.germanDates = (*bytes)[1] != 0;
      prefs.ignoreFinished = (*bytes)[2] != 0;
      prefs.ignorePermissions = (*bytes)[3] != 0;
    }
  }
  return prefs;
}

util::Status savePreferences(const util::fs::path &file, const Preferences &prefs) {
  Json j;
  j["onlyPermittedSections"] = prefs.onlyPermittedSections;
  j["germanDates"] = prefs.germanDates;
  j["ignoreFinished"] = prefs.ignoreFinished;
  j["ignorePermissions"] = prefs.ignorePermissions;
  j["showClosedDevices"] = prefs.showClosedDevices;
  j["ordersAsPopup"] = prefs.ordersAsPopup;
  j["desktopNotifications"] = prefs.desktopNotifications;
  j["taskbarBadge"] = prefs.taskbarBadge;
  j["mutedAlerts"] = prefs.mutedAlerts;
  j["recentDevices"] = prefs.recentDevices;
  return util::json::save(file, j);
}

} // namespace st::config
