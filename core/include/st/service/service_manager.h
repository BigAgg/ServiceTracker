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
/// External:  createExternService copies the device folder to the local working copy,
///            finishExternService copies the service folder back and deletes the copy.
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
  HistoryStore history;
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

  /// Service folders of a device, sorted (oldest first).
  std::vector<std::string> listServices(const devices::Device &device) const;
  util::fs::path syncFolderFor(const devices::Device &device, std::string_view service) const;
  util::fs::path externCopyFor(const devices::Device &device) const;

  /// Record of the device from Sync.json, or a fresh record (not in the list).
  util::Result<ServiceRecord> recordFor(const devices::Device &device) const;

  /// Checklist file of a service: local external copy, service folder, sync folder (in that order).
  std::optional<util::fs::path> findChecklist(const devices::Device &device, std::string_view service, const ServiceRecord &record) const;

  /// Starts an in house service for today.
  util::Result<ServiceRecord> createService(const devices::Device &device, const util::Session &session);

  /// Sets the status (writes the record). An empty status removes the device from the list.
  util::Result<ServiceRecord> setStatus(const devices::Device &device, std::string_view status);
  /// Marks a device as expected ("erwartet") with additional information.
  util::Result<ServiceRecord> expect(const devices::Device &device, const std::string &info);
  util::Result<ServiceRecord> setAdditionalInfo(const devices::Device &device, const std::string &info);
  util::Status removeFromList(const devices::Device &device);

  /// Finishes the running in house service. `checklist` (may be null) is exported as service record.
  util::Result<FinishResult> finishService(const devices::Device &device, const checklist::Checklist *checklist,
                                           const checklist::HtmlOptions &html, const util::Session &session);

  /// Plans an external service on `date` ("YYYY.MM.DD").
  util::Result<ServiceRecord> createExternService(const devices::Device &device, const std::string &date, const std::string &info, const util::Session &session);
  /// Finishes an external service: copies the work back and removes the local copy.
  util::Result<FinishResult> finishExternService(const devices::Device &device, const checklist::Checklist *checklist,
                                                 const checklist::HtmlOptions &html, const util::Session &session);
  /// Removes an external service from both lists. The local copy is kept.
  util::Status cancelExternService(const devices::Device &device);

  /// Price of a service according to the configured price table.
  double priceOf(const checklist::Checklist &checklist) const;

private:
  util::Status createServiceFolders(const devices::Device &device, const util::fs::path &serviceFolder) const;
  util::Status copyTemplates(const devices::Device &device, const std::string &date, const util::fs::path &target) const;
  util::Result<FinishResult> finalize(const devices::Device &device, ServiceRecord record, const util::fs::path &workFolder,
                                      const checklist::Checklist *checklist, const checklist::HtmlOptions &html, const util::Session &session);

  ServiceEnvironment mEnv;
};

/// Logic of the "update_status_rinfo" section action: the status `section.postActionArgs`
/// is only applied if the section comes after the section that set the current status,
/// so finishing sections out of order never moves the status backwards.
bool shouldAdvanceStatus(const checklist::Checklist &checklist, const checklist::Section &section, std::string_view currentStatus);

} // namespace st::service
