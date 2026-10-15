#pragma once

#include "st/util/fs.h"

#include <string>
#include <vector>

/// Personal settings of the local user (stored next to the executable, not on the
/// shared drive).
namespace st::config {

struct Preferences {
  /// Hide checklist sections the user has no permission for.
  bool onlyPermittedSections = true;
  /// Show timestamps as "DD.MM.YYYY HH:MM:SS".
  bool germanDates = false;
  /// Power users only: allow editing finished sections.
  bool ignoreFinished = false;
  /// Power users only: allow editing sections without permission.
  bool ignorePermissions = false;
  /// Show the device list including finished/expected devices when no filter is set.
  bool showClosedDevices = false;
  /// Orders are opened as popup instead of inline tree nodes.
  bool ordersAsPopup = false;
  /// Show Windows notifications (otherwise only messages inside the program).
  bool desktopNotifications = true;
  /// Red dot on the taskbar icon while notifications are pending.
  bool taskbarBadge = true;
  /// Ids of notification channels the user switched off (alerts::Channel::id).
  std::vector<std::string> mutedAlerts;
  /// Recently selected devices (newest first).
  std::vector<std::string> recentDevices;

  /// Moves `device` to the front of the recent list (max `limit` entries).
  void rememberDevice(const std::string &device, std::size_t limit = 10);
  /// Resets options a non power user must not use.
  void restrictTo(bool poweruser);

  bool operator==(const Preferences &) const = default;
};

/// Reads the preferences. Falls back to the legacy `checklist.bin` (4 bools) next to
/// `file` and to defaults when nothing exists.
Preferences loadPreferences(const util::fs::path &file, const util::fs::path &legacyChecklistBin = {});
util::Status savePreferences(const util::fs::path &file, const Preferences &prefs);

} // namespace st::config
