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
