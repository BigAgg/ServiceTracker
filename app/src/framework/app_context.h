#pragma once

#include "framework/alerts.h"
#include "framework/event_bus.h"
#include "framework/notifications.h"
#include "framework/poll_timer.h"
#include "framework/ui_host.h"

#include <st/checklist/actions.h>
#include <st/checklist/document.h>
#include <st/config/preferences.h>
#include <st/config/settings.h>
#include <st/devices/device_config.h>
#include <st/devices/device_registry.h>
#include <st/service/device_filter.h>
#include <st/service/service_manager.h>
#include <st/users/user_store.h>
#include <st/util/session.h>

#include <chrono>
#include <optional>
#include <string>

namespace st::app {

/// The checklist currently opened for the selected service. Shared by the
/// checklist, service and extern modules.
struct ChecklistSession {
  /// External service: the checklist on the server the local copy is mirrored with.
  struct Mirror {
    util::fs::path file;
    util::fs::path localRoot;
    util::fs::path serverRoot;
  };

  std::optional<checklist::ChecklistDocument> doc;
  std::optional<Mirror> mirror;
  /// State of the mirror for the checklist window ("Synchronisiert 10:21:03" ...).
  std::string mirrorState;
  /// Local changes that are not written yet.
  bool dirty = false;
  std::chrono::steady_clock::time_point lastEdit{};
  std::string lastError;

  bool isOpen() const { return doc.has_value(); }
  void markDirty() {
    dirty = true;
    lastEdit = std::chrono::steady_clock::now();
  }
};

/// Everything modules share: configuration, the logged in user, data stores,
/// the current selection and the UI infrastructure. Created once in main().
class AppContext {
public:
  AppContext(config::Settings settings, std::string user, util::fs::path workingDir);

  /// Loads users, configs, devices and the service list. Problems are reported as
  /// notifications instead of aborting the program.
  void initialize();
  /// Polls shared files (throttled) and delivers notifications. Call once per frame.
  void update();
  /// Framework parts drawn inside the ImGui frame after the modules (toasts, status
  /// line, focus requests of clicked notifications). Call once per frame.
  void drawOverlay();

  // --- configuration & identity -----------------------------------------------
  const config::Settings &settings() const { return mSettings; }
  const config::Paths &paths() const { return mSettings.paths; }
  const util::Session &session() const { return mSession; }
  const std::string &user() const { return mSession.user(); }
  const util::fs::path &workingDir() const { return mWorkingDir; }

  bool isPoweruser() const { return userStore.isPoweruser(user()); }
  bool hasAccess(std::string_view group) const { return userStore.hasAccess(user(), group); }
  bool hasAnyAccess(const std::vector<std::string> &groups) const { return userStore.hasAnyAccess(user(), groups); }

  config::Preferences &prefs() { return mPrefs; }
  const config::Preferences &prefs() const { return mPrefs; }
  void savePrefs();
  /// Timestamp for display, honours the "Deutsches Zeitformat" preference.
  std::string displayTime(std::string_view timestamp) const;

  // --- infrastructure ---------------------------------------------------------
  UiHost ui;
  EventBus events;
  Notifications notify;
  /// Desktop notifications and taskbar badge (see docs/benachrichtigungen.md).
  Alerts alerts;

  // --- data -------------------------------------------------------------------
  st::users::UserStore userStore;
  st::devices::DeviceRegistry deviceRegistry;
  st::devices::DeviceConfigStore configStore;
  checklist::ExpressionRegistry expressions;
  checklist::SectionActionRegistry sectionActions;
  service::ServiceManager &services() { return *mServices; }
  const service::ServiceManager &services() const { return *mServices; }
  /// False when the network drive was not reachable at start (offline mode).
  bool syncAvailable() const { return mSyncAvailable; }

  // --- selection --------------------------------------------------------------
  /// Status of all devices in the service list.
  const service::StatusIndex &statusIndex() const { return mStatusIndex; }
  /// Re-reads Sync.json and publishes RecordsChanged.
  void reloadRecords();

  st::devices::Device *activeDevice();
  const std::string &activeDeviceName() const { return mActiveDevice; }
  /// Record of the active device (empty record if none).
  const service::ServiceRecord &activeRecord() const { return mActiveRecord; }
  const std::string &selectedService() const { return mSelectedService; }

  /// Makes `name` the active device (reads its flags/info, selects the running service).
  void selectDevice(const std::string &name);
  void selectService(const std::string &service);
  /// Re-reads flags and counters of the active device from disk.
  void refreshActiveDevice();
  /// Applies the result of a ServiceManager operation (updates lists and selection).
  void applyRecord(const service::ServiceRecord &record);

  ChecklistSession &checklist() { return mChecklist; }

private:
  config::Settings mSettings;
  util::SystemClock mClock;
  util::Session mSession;
  util::fs::path mWorkingDir;
  config::Preferences mPrefs;

  std::unique_ptr<service::ServiceManager> mServices;
  bool mSyncAvailable = false;
  service::StatusIndex mStatusIndex;
  std::int64_t mSyncWriteTime = 0;
  PollTimer mSyncPoll;
  PollTimer mOnlineCheck{std::chrono::seconds(10)};
  bool mReconnectNotified = false;

  std::string mActiveDevice;
  service::ServiceRecord mActiveRecord;
  std::string mSelectedService;
  ChecklistSession mChecklist;
};

} // namespace st::app
