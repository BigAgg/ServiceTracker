#pragma once

#include "st/checklist/formats.h"
#include "st/checklist/model.h"
#include "st/util/fs.h"

#include <cstdint>

namespace st::checklist {

/// A checklist file on disk that is edited by several users at the same time.
///
/// Typical use:
/// \code
///   auto doc = ChecklistDocument::load(path);
///   doc->refresh();          // call regularly: pulls changes of other users
///   ... edit doc->data() ...
///   doc->save();             // merges remote changes, then writes atomically
/// \endcode
class ChecklistDocument {
public:
  ChecklistDocument() = default;
  ChecklistDocument(util::fs::path path, Checklist data);

  /// Loads a checklist (format chosen by file extension).
  static util::Result<ChecklistDocument> load(const util::fs::path &path);

  const util::fs::path &path() const { return mPath; }
  Format format() const { return formatForPath(mPath); }
  bool isOpen() const { return !mPath.empty(); }

  Checklist &data() { return mData; }
  const Checklist &data() const { return mData; }

  /// Folder for attachments next to the checklist file.
  util::fs::path attachmentsDir() const;

  /// True if another user changed the file since it was last read or written.
  bool changedOnDisk() const;

  /// Merges newer changes from disk into the local data (if the file changed).
  /// Returns the number of merged elements.
  util::Result<std::size_t> refresh();

  /// Merges remote changes and writes the file if the content differs.
  /// Returns the number of merged remote elements.
  util::Result<std::size_t> save();

  /// Writes to a different path (format by extension) and continues with that path.
  util::Status saveAs(const util::fs::path &path);

  /// Two-way sync with a mirror of this checklist in another folder tree (external
  /// service: local copy <-> device folder on the server). Newer elements win in both
  /// directions (see merge.h); attachment paths are translated between `localRoot` and
  /// `mirrorRoot` and missing attachment files are copied. A missing mirror file is
  /// created. Returns the number of elements taken over from the mirror.
  util::Result<std::size_t> syncWithMirror(const util::fs::path &mirrorFile, const util::fs::path &localRoot,
                                           const util::fs::path &mirrorRoot);

private:
  util::Result<std::size_t> mergeFromDisk(std::string *diskContent);

  util::fs::path mPath;
  Checklist mData;
  std::int64_t mLastWrite = 0;
};

/// Rewrites attachment paths below `from` to the same relative path below `to`
/// (e.g. after copying a folder tree). Returns the number of changed paths.
std::size_t relocateAttachments(Checklist &checklist, const util::fs::path &from, const util::fs::path &to);

} // namespace st::checklist
