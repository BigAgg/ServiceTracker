#include "framework/app_context.h"

#include "framework/events.h"

#include <st/util/log.h>
#include <st/util/time.h>

namespace st::app {

namespace {
util::fs::path resolve(const util::fs::path &base, const std::string &utf8) {
  const auto p = util::pathFromUtf8(utf8);
  return p.is_absolute() || p.has_root_name() ? p : base / p;
}
} // namespace

AppContext::AppContext(config::Settings settings, std::string user, util::fs::path workingDir)
    : mSettings(std::move(settings)),
      mSession(std::move(user), mClock),
      mWorkingDir(std::move(workingDir)),
      mSyncPoll(std::chrono::milliseconds(std::max(200, mSettings.refreshIntervalMs))) {
  const auto &p = mSettings.paths;
  userStore = st::users::UserStore(util::pathFromUtf8(p.usersFile), mSettings.bootstrapAdmin);
  configStore = st::devices::DeviceConfigStore(p.configsFile());

  service::ServiceEnvironment env;
  env.syncTempDir = p.syncTempDir();
  env.externLocalDir = resolve(mWorkingDir, p.externLocalDir);
  env.localExternStore = service::RecordStore(env.externLocalDir / "extern_local.json");
  env.syncStore = service::RecordStore(p.syncFile());
  env.externStore = service::RecordStore(util::pathFromUtf8(p.externFile));
  env.history = service::HistoryStore(p.historyFile());
  env.externHistory = service::HistoryStore(p.externHistoryFile());
  env.configs = &configStore;
  mServices = std::make_unique<service::ServiceManager>(std::move(env));
}

void AppContext::initialize() {
  const auto &p = mSettings.paths;
  log::info("Benutzer: {}", user());

  switch (userStore.loadWithCache(mWorkingDir / "users_cache.bin", user())) {
  case users::UserStore::Source::Server:
    break;
  case users::UserStore::Source::Cache:
    notify.warning("Offline: Berechtigungen aus dem zuletzt gespeicherten Stand");
    break;
  case users::UserStore::Source::None:
    notify.error("Benutzerdatei nicht erreichbar und kein lokaler Stand – eingeschränkte Rechte.");
    break;
  }

  if (util::pathExists(p.configsFile())) {
    notify.check(configStore.load(), "Gerätekonfigurationen");
  } else {
    log::warning("configs.json nicht gefunden: {}", util::pathToUtf8(p.configsFile()));
  }

  deviceRegistry.scan(util::pathFromUtf8(p.devicesRoot));

  mSyncAvailable = util::isDirectory(util::pathFromUtf8(p.syncDir));
  if (mSyncAvailable) {
    notify.check(services().environment().syncStore.ensureExists(), "Sync.json");
    notify.check(services().environment().history.ensureExists(), "History.json");
    notify.check(services().environment().externHistory.ensureExists(), "History_extern.json");
  } else {
    notify.warning("Offline: Netzlaufwerk nicht erreichbar – nur externe Services mit lokaler Kopie verfügbar");
    log::warning("Sync-Ordner nicht erreichbar: {}", p.syncDir);
  }

  mPrefs = config::loadPreferences(mWorkingDir / "preferences.json", mWorkingDir / "checklist.bin");
  mPrefs.restrictTo(isPoweruser());

  checklist::registerBuiltinActions(sectionActions, expressions);
  reloadRecords();
}

void AppContext::update() {
  alerts.update(*this);
  if (!mSyncAvailable) {
    notify.setStatusLine(user() + "  |  Offline");
    if (!mReconnectNotified && mOnlineCheck.due() && services().online()) {
      mReconnectNotified = true;
      notify.info("Netzlaufwerk wieder erreichbar – ServiceTracker neu starten, um alle Funktionen zu nutzen.");
    }
    return;
  }
  if (!mSyncPoll.due())
    return;
  const auto writeTime = services().environment().syncStore.lastWriteTime();
  if (writeTime != mSyncWriteTime)
    reloadRecords();
  notify.setStatusLine(user() + (isPoweruser() ? " (Poweruser)" : "") + "  |  Sync " + util::formatTimestamp(mSession.now()).substr(11));
}

void AppContext::drawOverlay() {
  alerts.drawFrame(*this);
  notify.draw();
}

void AppContext::savePrefs() {
  if (auto s = config::savePreferences(mWorkingDir / "preferences.json", mPrefs); !s)
    log::warning("Einstellungen konnten nicht gespeichert werden: {}", s.error().message);
}

std::string AppContext::displayTime(std::string_view timestamp) const {
  return mPrefs.germanDates ? util::toGermanTimestamp(timestamp) : std::string(timestamp);
}

void AppContext::reloadRecords() {
  auto &store = services().environment().syncStore;
  if (mSyncAvailable)
    mSyncWriteTime = store.lastWriteTime();
  // Offline: the external services with a local copy on this PC
  auto records = mSyncAvailable ? store.loadAll() : services().localExternRecords();
  if (!records) {
    notify.error(records.error().message);
    return;
  }
  mStatusIndex = service::StatusIndex::fromRecords(*records);
  if (!deviceRegistry.hasAccess())
    service::addMissingDevices(deviceRegistry, *records);
  if (!mActiveDevice.empty()) {
    const auto previousService = mActiveRecord.activeService;
    bool found = false;
    for (const auto &r : *records) {
      if (r.name == mActiveDevice) {
        auto services = mActiveRecord.services;
        mActiveRecord = r;
        mActiveRecord.services = services;
        found = true;
        break;
      }
    }
    if (!found) {
      service::ServiceRecord empty;
      empty.name = mActiveDevice;
      empty.path = mActiveRecord.path;
      empty.services = mActiveRecord.services;
      mActiveRecord = std::move(empty);
    }
  }
  events.publish(events::RecordsChanged{});
}

st::devices::Device *AppContext::activeDevice() {
  return mActiveDevice.empty() ? nullptr : deviceRegistry.find(mActiveDevice);
}

void AppContext::refreshActiveDevice() {
  if (auto *device = activeDevice(); device && deviceRegistry.hasAccess())
    st::devices::populateDevice(*device, configStore.find(device->project()));
}

void AppContext::selectDevice(const std::string &name) {
  if (name == mActiveDevice)
    return;
  mActiveDevice = name;
  mActiveRecord = {};
  mSelectedService.clear();
  if (auto *device = activeDevice()) {
    refreshActiveDevice();
    if (auto record = services().recordFor(*device)) {
      mActiveRecord = std::move(*record);
      mSelectedService = mActiveRecord.activeService;
    } else {
      notify.error(record.error().message);
    }
    mPrefs.rememberDevice(name);
    savePrefs();
  }
  events.publish(events::DeviceSelected{mActiveDevice});
  events.publish(events::ServiceSelected{mActiveDevice, mSelectedService});
}

void AppContext::selectService(const std::string &service) {
  if (service == mSelectedService)
    return;
  mSelectedService = service;
  events.publish(events::ServiceSelected{mActiveDevice, mSelectedService});
}

void AppContext::applyRecord(const service::ServiceRecord &record) {
  events.publish(events::RecordApplied{record.name});
  if (record.name == mActiveDevice) {
    mActiveRecord = record;
    if (auto *device = activeDevice())
      mActiveRecord.services = services().listServices(*device);
  }
  reloadRecords();
  if (record.name == mActiveDevice && record.activeService != mSelectedService && !record.activeService.empty())
    selectService(record.activeService);
}

} // namespace st::app
