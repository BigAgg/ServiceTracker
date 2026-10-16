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

namespace {

/// Relative path of `path` below `root` (empty if it is not below `root`).
util::fs::path relativeBelow(const util::fs::path &path, const util::fs::path &root) {
  const auto rel = path.lexically_normal().lexically_relative(root.lexically_normal());
  if (rel.empty() || rel.native().starts_with(util::fs::path("..").native()))
    return {};
  return rel;
}

/// Copies attachment files of `checklist` that exist below `from` but are missing below `to`.
void copyMissingAttachments(const Checklist &checklist, const util::fs::path &from, const util::fs::path &to) {
  for (const auto &section : checklist.sections) {
    forEachTask(section, [&](const Task &task) {
      for (const auto &attachment : task.attachments) {
        const auto source = util::pathFromUtf8(attachment);
        const auto rel = relativeBelow(source, from);
        if (rel.empty() || !util::pathExists(source) || util::pathExists(to / rel))
          continue;
        if (auto s = util::copyFile(source, to / rel); !s)
          log::warning("Anhang konnte nicht kopiert werden: {}", s.error().message);
      }
    });
  }
}

} // namespace

std::size_t relocateAttachments(Checklist &checklist, const util::fs::path &from, const util::fs::path &to) {
  std::size_t changed = 0;
  for (auto &section : checklist.sections) {
    forEachTask(section, [&](Task &task) {
      for (auto &attachment : task.attachments) {
        const auto rel = relativeBelow(util::pathFromUtf8(attachment), from);
        if (rel.empty())
          continue;
        attachment = util::pathToUtf8(to / rel);
        ++changed;
      }
    });
  }
  return changed;
}

util::Result<std::size_t> ChecklistDocument::syncWithMirror(const util::fs::path &mirrorFile, const util::fs::path &localRoot,
                                                            const util::fs::path &mirrorRoot) {
  if (!isOpen())
    return std::unexpected(util::Error{"Keine Checkliste geöffnet"});
  // Local changes first, so the mirror receives the current state
  if (auto saved = save(); !saved)
    return saved;

  Checklist local = mData;
  relocateAttachments(local, localRoot, mirrorRoot);
  if (!util::pathExists(mirrorFile)) {
    ChecklistDocument created(mirrorFile, std::move(local));
    if (auto s = created.saveAs(mirrorFile); !s)
      return std::unexpected(s.error());
    copyMissingAttachments(mData, localRoot, mirrorRoot);
    log::info("Checkliste gespiegelt (neu): {}", util::pathToUtf8(mirrorFile));
    return 0;
  }

  auto mirror = load(mirrorFile);
  if (!mirror)
    return std::unexpected(mirror.error());
  // mirror -> local
  Checklist remote = mirror->data();
  relocateAttachments(remote, mirrorRoot, localRoot);
  const std::size_t pulled = mergeNewer(mData, remote);
  copyMissingAttachments(mirror->data(), mirrorRoot, localRoot);
  // local -> mirror
  const std::size_t pushed = mergeNewer(mirror->data(), local);
  if (pushed > 0) {
    if (auto s = mirror->save(); !s)
      return std::unexpected(s.error());
    copyMissingAttachments(mData, localRoot, mirrorRoot);
  }
  if (pulled > 0) {
    if (auto s = save(); !s)
      return s;
  }
  if (pulled > 0 || pushed > 0)
    log::info("Checkliste gespiegelt ({} übernommen, {} übertragen): {}", pulled, pushed, util::pathToUtf8(mirrorFile));
  return pulled;
}

} // namespace st::checklist
