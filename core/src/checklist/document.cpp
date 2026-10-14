#include "st/checklist/document.h"

#include "st/checklist/merge.h"
#include "st/util/log.h"

namespace st::checklist {

ChecklistDocument::ChecklistDocument(util::fs::path path, Checklist data)
    : mPath(std::move(path)), mData(std::move(data)) {}

util::Result<ChecklistDocument> ChecklistDocument::load(const util::fs::path &path) {
  auto content = util::readFile(path);
  if (!content)
    return std::unexpected(content.error());
  auto parsed = parse(*content, formatForPath(path));
  if (!parsed)
    return std::unexpected(util::Error{parsed.error().message + ": \"" + util::pathToUtf8(path) + "\""});
  ChecklistDocument doc(path, std::move(*parsed));
  doc.mLastWrite = util::lastWriteTime(path);
  log::info("Checkliste geladen: {}", util::pathToUtf8(path));
  return doc;
}

util::fs::path ChecklistDocument::attachmentsDir() const { return mPath.parent_path() / "attachments"; }

bool ChecklistDocument::changedOnDisk() const {
  return isOpen() && util::lastWriteTime(mPath) != mLastWrite;
}

util::Result<std::size_t> ChecklistDocument::mergeFromDisk(std::string *diskContent) {
  if (!util::pathExists(mPath))
    return 0;
  const std::int64_t writeTime = util::lastWriteTime(mPath);
  auto content = util::readFile(mPath);
  if (!content)
    return std::unexpected(content.error());
  auto remote = parse(*content, format());
  if (!remote)
    return std::unexpected(remote.error());
  mLastWrite = writeTime;
  if (diskContent)
    *diskContent = std::move(*content);
  return mergeNewer(mData, *remote);
}

util::Result<std::size_t> ChecklistDocument::refresh() {
  if (!changedOnDisk())
    return 0;
  auto merged = mergeFromDisk(nullptr);
  if (merged && *merged > 0)
    log::info("Checkliste synchronisiert ({} Änderungen): {}", *merged, util::pathToUtf8(mPath));
  return merged;
}

util::Result<std::size_t> ChecklistDocument::save() {
  if (!isOpen())
    return std::unexpected(util::Error{"Keine Checkliste geöffnet"});
  std::string diskContent;
  auto merged = mergeFromDisk(&diskContent);
  if (!merged)
    return merged;
  std::string content;
  if (format() == Format::Json)
    content = writeJson(mData, util::pathToUtf8(attachmentsDir()) + "/", mLastWrite);
  else
    content = writeLegacyText(mData);
  if (format() == Format::Json) {
    // lastedit changes on every write; compare without it to avoid needless writes
    if (auto disk = parseJson(diskContent); disk && *disk == mData)
      return merged;
  } else if (content == diskContent) {
    return merged;
  }
  if (auto status = util::writeFileAtomic(mPath, content); !status)
    return std::unexpected(status.error());
  mLastWrite = util::lastWriteTime(mPath);
  return merged;
}

util::Status ChecklistDocument::saveAs(const util::fs::path &path) {
  const std::string content = formatForPath(path) == Format::Json
                                  ? writeJson(mData, util::pathToUtf8(path.parent_path() / "attachments") + "/")
                                  : writeLegacyText(mData);
  if (auto status = util::writeFileAtomic(path, content); !status)
    return status;
  mPath = path;
  mLastWrite = util::lastWriteTime(mPath);
  return {};
}

} // namespace st::checklist
