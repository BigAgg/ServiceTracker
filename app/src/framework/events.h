#pragma once

#include <string>

/// Events exchanged between modules (see EventBus). Add new events here so all
/// modules see the full list of things that can happen.
namespace st::app::events {

/// The active device changed (name may be empty = no device).
struct DeviceSelected {
  std::string device;
};

/// A service folder of the active device was selected (service may be empty).
struct ServiceSelected {
  std::string device;
  std::string service;
};

/// The service list (Sync.json) changed – by this program or by another user.
struct RecordsChanged {};

/// This user changed a record (ServiceManager action applied with ctx.applyRecord).
/// Published before the following RecordsChanged, e.g. to suppress notifications
/// about the user's own changes.
struct RecordApplied {
  std::string device;
};

/// A notification was clicked. `item` is the item id of a per-item notification
/// (e.g. the device), empty for summaries and clicks on the tray icon.
struct AlertActivated {
  std::string channel;
  std::string item;
};

/// Open the editor of the status notifications (menu "Configs").
struct ShowStatusAlertsEditor {};

/// The device list was rescanned.
struct DevicesRescanned {};

/// The open checklist was saved / loaded / closed.
struct ChecklistSaved {
  std::string path;
};
struct ChecklistOpened {
  std::string path;
};
struct ChecklistClosed {};

/// Ask the checklist module to save immediately (e.g. before the service is finished).
struct SaveChecklistRequest {};

/// The user changed personal preferences.
struct PreferencesChanged {};

} // namespace st::app::events
