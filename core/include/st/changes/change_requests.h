#pragma once

#include "st/util/fs.h"

#include <cstdint>
#include <string>
#include <vector>

/// Change requests ("Änderungswünsche") users send to the developers.
///
/// Every request is a folder `<requestsDir>/<id>/` with `info.txt` and an
/// `attachments/` folder (legacy format, see docs/dateiformate.md).
namespace st::changes {

struct ChangeRequest {
  std::uint64_t id = 0;
  std::string subject;
  std::string message;
  /// Absolute paths of the copied attachments.
  std::vector<std::string> attachments;
  bool open = true;
  /// Optional: user that created the request (not written by the legacy program).
  std::string author;
  std::string created;

  bool operator==(const ChangeRequest &) const = default;
};

std::string writeInfoFile(const ChangeRequest &request);
ChangeRequest parseInfoFile(std::string_view content);

class ChangeRequestStore {
public:
  ChangeRequestStore() = default;
  explicit ChangeRequestStore(util::fs::path dir) : mDir(std::move(dir)) {}

  bool available() const { return util::isDirectory(mDir); }

  /// All requests, newest first.
  util::Result<std::vector<ChangeRequest>> loadAll() const;

  /// Creates a request and copies the attachment files into its folder.
  util::Result<ChangeRequest> submit(std::string subject, std::string message, const std::vector<util::fs::path> &attachments,
                                     std::string author, std::string created) const;

  /// Opens or closes a request.
  util::Status setOpen(std::uint64_t id, bool open) const;

private:
  util::fs::path mDir;
};

} // namespace st::changes
