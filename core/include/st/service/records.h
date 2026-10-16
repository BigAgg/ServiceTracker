#pragma once

#include "st/util/fs.h"
#include "st/util/json.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// Service state of devices, stored in shared JSON lists:
///  - Sync.json   : all devices currently "in the list" (in house services)
///  - extern.json : external services until they are finished ("Service fertigmelden")
/// Both files contain an array of records with identical keys.
namespace st::service {

/// Status values used by the program. Section actions may set any other text.
namespace status {
inline constexpr std::string_view Expected = "erwartet";
inline constexpr std::string_view Received = "Wareneingang";
inline constexpr std::string_view Done = "erledigt";
inline constexpr std::string_view External = "extern";
inline constexpr std::string_view PlannedExternal = "geplant extern";
/// External work is done and copied back; the service still has to be finished
/// in house ("Service fertigmelden").
inline constexpr std::string_view ExternFinished = "extern abgeschlossen";
inline constexpr std::string_view DoneExternal = "erledigt extern";
} // namespace status

/// True for "", "erledigt" and "erledigt extern" (device is not being worked on).
bool isClosedStatus(std::string_view value);
/// True while the technician works on the local copy ("geplant extern", "extern").
bool isExternalWorkStatus(std::string_view value);

struct ServiceRecord {
  std::string name;
  /// Device folder (UTF-8).
  std::string path;
  std::string status;
  /// Service folder name "Service_YYYY.MM.DD" of the running service.
  std::string activeService;
  /// Folder on the sync drive where the checklist is edited while the service runs.
  std::string syncFolder;
  std::string additionalInfo;
  std::vector<std::string> services;
  /// Days between start and end of the service.
  long duration = 0;
  /// Keys written by other program versions (kept when saving).
  util::json::Json extra = util::json::Json::object();

  bool operator==(const ServiceRecord &) const = default;
};

ServiceRecord recordFromJson(const util::json::Json &j);
util::json::Json recordToJson(const ServiceRecord &record);

/// A JSON array of service records on the shared drive.
///
/// Every modification is a short read-modify-write of the current file content,
/// so changes of other users to other records are never lost.
class RecordStore {
public:
  RecordStore() = default;
  explicit RecordStore(util::fs::path file) : mFile(std::move(file)) {}

  const util::fs::path &file() const { return mFile; }

  /// Creates an empty list if the file does not exist yet.
  util::Status ensureExists() const;

  util::Result<std::vector<ServiceRecord>> loadAll() const;
  util::Result<std::optional<ServiceRecord>> find(std::string_view name) const;

  /// Inserts or updates the record with the same name.
  util::Status upsert(const ServiceRecord &record) const;
  /// Removes the record. Missing records are not an error.
  util::Status remove(std::string_view name) const;

  /// Modification time of the file (to detect changes of other users).
  std::int64_t lastWriteTime() const { return util::lastWriteTime(mFile); }

private:
  util::Result<util::json::Json> read() const;

  util::fs::path mFile;
};

} // namespace st::service
