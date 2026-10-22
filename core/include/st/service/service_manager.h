#pragma once

#include "st/checklist/export.h"
#include "st/checklist/model.h"
#include "st/devices/device.h"
#include "st/devices/device_config.h"
#include "st/service/history.h"
#include "st/service/records.h"
#include "st/util/session.h"

#include <optional>
#include <string>
#include <vector>

/// Service workflows (see Programminfo.md):
///
/// In house:  createService -> (checklist is edited in the sync folder) -> finishService
///            finishService copies the sync folder into the device folder, writes the
///            service record (HTML) and appends the service to the history.
/// External:  createExternService creates the service on the server (extern templates)
///            and copies the device folder to the local working copy. While working in
///            house the checklist of the copy is mirrored with the server
///            (ChecklistDocument::syncWithMirror, see mirrorOf). finishExternService
///            ("Extern abschließen") syncs a last time, copies the local device folder over
///            the original, deletes the copy and sets "extern abgeschlossen".
///            finishService ("Service fertigmelden") then writes the service record and
///            the entry in History_extern.json.
namespace st::service {

/// Section action that sets the device status to the section's postActionArgs.
inline constexpr std::string_view kUpdateStatusAction = "update_status_rinfo";

struct ServiceEnvironment {
  /// SYNC/temp – per device and service a folder with the live checklist.
  util::fs::path syncTempDir;
  /// Local working copies of external services.
  util::fs::path externLocalDir;
  RecordStore syncStore;
  RecordStore externStore;
  /// External services with a local copy on this PC (externLocalDir/extern_local.json).
  /// Used when the network drive is not reachable (working at a customer).
  RecordStore localExternStore;
  HistoryStore history;
  /// History of external services (History_extern.json).
  HistoryStore externHistory;
  /// Optional; without configs no extra folders/templates are created.
  const devices::DeviceConfigStore *configs = nullptr;

  /// Where the price of a service is read from (table column total).
  std::string priceSection = "Preisliste";
  std::string priceTable = "Preisliste (Rabatt bereits berechnet)";
  std::string priceColumn = "Preis";
};

struct FinishResult {
  ServiceRecord record;
  /// Service record (HTML) inside the service folder.
  util::fs::path reportFile;
};

class ServiceManager {
public:
  explicit ServiceManager(ServiceEnvironment env);

  ServiceEnvironment &environment() { return mEnv; }
  const ServiceEnvironment &environment() const { return mEnv; }

  /// "Service_YYYY.MM.DD"
  static std::string serviceFolderName(std::string_view date);
  /// "Service_YYYY.MM.DD" -> "YYYY.MM.DD"
  static std::string serviceDate(std::string_view folderName);

  /// True if the shared lists (Sync.json) are reachable. Offline the records of external
  /// services come from the local list and the services from the local copy.
  bool online() const;

  /// Service folders of a device, sorted (oldest first). Offline: from the local copy.
  std::vector<std::string> listServices(const devices::Device &device) const;
  util::fs::path syncFolderFor(const devices::Device &device, std::string_view service) const;
  util::fs::path externCopyFor(const devices::Device &device) const;

  /// Record of the device from Sync.json, or a fresh record (not in the list).
  /// Offline: from the local list of external services.
  util::Result<ServiceRecord> recordFor(const devices::Device &device) const;

  /// Checklist file of a service: local external copy, service folder, sync folder (in that order).
  std::optional<util::fs::path> findChecklist(const devices::Device &device, std::string_view service, const ServiceRecord &record) const;

  /// Counterpart on the server of a file inside the local external copy (same relative
  /// path below the device folder). Empty if `file` is not inside the local copy.
  std::optional<util::fs::path> mirrorOf(const devices::Device &device, const util::fs::path &file) const;

  /// Starts an in house service for today.
  util::Result<ServiceRecord> createService(const devices::Device &device, const util::Session &session);

  /// Sets the status (writes the record). An empty status removes the device from the list.
  util::Result<ServiceRecord> setStatus(const devices::Device &device, std::string_view status);
  /// Marks a device as expected ("erwartet") with additional information.
  util::Result<ServiceRecord> expect(const devices::Device &device, const std::string &info);
  util::Result<ServiceRecord> setAdditionalInfo(const devices::Device &device, const std::string &info);
  util::Status removeFromList(const devices::Device &device);

  /// Finishes the running service ("Service fertigmelden"). `checklist` (may be null) is
  /// exported as service record. In house: entry in History.json. External services
  /// (status "extern abgeschlossen"): status "erledigt extern", entry in
  /// History_extern.json, removed from extern.json.
  util::Result<FinishResult> finishService(const devices::Device &device, const checklist::Checklist *checklist,
                                           const checklist::HtmlOptions &html, const util::Session &session);

  /// Plans an external service on `date` ("YYYY.MM.DD").
  util::Result<ServiceRecord> createExternService(const devices::Device &device, const std::string &date, const std::string &info, const util::Session &session);
  /// "Extern abschließen": merges the checklists of the local copy with the server a
  /// last time, copies the local device folder over the original, deletes the local
  /// copy and sets "extern abgeschlossen". Requires access to the device folder.
  util::Result<ServiceRecord> finishExternService(const devices::Device &device);
  /// One record per local copy (folder in externLocalDir) on this PC. Known services
  /// come from extern_local.json; copies without an entry (created by an older version
  /// or copied by hand) get status "geplant extern" and their newest service folder.
  /// Used offline and to show local copies in "Externe Services".
  util::Result<std::vector<ServiceRecord>> localExternRecords() const;
  /// True if a local copy of the device exists on this PC.
  bool hasLocalCopy(const devices::Device &device) const;
  /// "Ordner lokal kopieren": copies the device folder of a running external service
  /// (e.g. created by a colleague) to this PC. Refused if a local copy already exists.
  util::Status copyExternToLocal(const devices::Device &device);
  /// Takes the current state of the shared extern.json over into the local list for
  /// devices with a local copy (status changes of colleagues are known offline).
  void refreshLocalExtern(const std::vector<ServiceRecord> &externRecords) const;
  /// Removes an external service from both lists. The local copy is kept.
  util::Status cancelExternService(const devices::Device &device);

  /// Price of a service according to the configured price table.
  double priceOf(const checklist::Checklist &checklist) const;

private:
  /// Keeps the local list in sync: external work states are stored, others removed.
  void updateLocalExtern(const ServiceRecord &record) const;
  util::Status createServiceFolders(const devices::Device &device, const util::fs::path &serviceFolder) const;
  util::Status copyTemplates(const devices::Device &device, const std::string &date, const util::fs::path &target, bool external) const;
  util::Result<FinishResult> finalize(const devices::Device &device, ServiceRecord record, const util::fs::path &workFolder,
                                      const checklist::Checklist *checklist, const checklist::HtmlOptions &html, const HistoryStore &history,
                                      const util::Session &session);

  ServiceEnvironment mEnv;
};

/// Logic of the "update_status_rinfo" section action: the status `section.postActionArgs`
/// is only applied if the section comes after the section that set the current status,
/// so finishing sections out of order never moves the status backwards. The status of
/// an external service ("geplant extern", "extern", "extern abgeschlossen") is never
/// replaced – it controls the external workflow.
bool shouldAdvanceStatus(const checklist::Checklist &checklist, const checklist::Section &section, std::string_view currentStatus);

} // namespace st::service
