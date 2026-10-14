#include "st/changes/change_requests.h"

#include "st/util/strings.h"

#include <algorithm>
#include <random>
#include <sstream>

namespace st::changes {

std::string writeInfoFile(const ChangeRequest &r) {
  std::ostringstream out;
  out << "ID=" << r.id << "\n";
  out << "STATUS=" << (r.open ? "OPEN" : "CLOSED") << "\n";
  out << "SUBJECT=" << util::singleLine(r.subject) << "\n";
  if (!r.author.empty())
    out << "AUTHOR=" << util::singleLine(r.author) << "\n";
  if (!r.created.empty())
    out << "CREATED=" << util::singleLine(r.created) << "\n";
  out << "MESSAGE_START\n" << r.message << "\nMESSAGE_END\n";
  out << "ATTACHMENTS_START\n";
  for (const auto &a : r.attachments)
    out << util::singleLine(a) << "\n";
  out << "ATTACHMENTS_END\n";
  return out.str();
}

ChangeRequest parseInfoFile(std::string_view content) {
  ChangeRequest r;
  r.open = false;
  const auto lines = util::splitLines(content);
  for (std::size_t i = 0; i < lines.size(); ++i) {
    const std::string &line = lines[i];
    const auto value = util::splitAt(line, "=").second;
    if (line.starts_with("ID=")) {
      r.id = util::parseUInt64(value).value_or(0);
    } else if (line.starts_with("SUBJECT=")) {
      r.subject = value;
    } else if (line.starts_with("AUTHOR=")) {
      r.author = value;
    } else if (line.starts_with("CREATED=")) {
      r.created = value;
    } else if (line == "STATUS=OPEN") {
      r.open = true;
    } else if (line == "MESSAGE_START") {
      std::vector<std::string> message;
      while (++i < lines.size() && lines[i] != "MESSAGE_END")
        message.push_back(lines[i]);
      r.message = util::join(message, "\n");
    } else if (line == "ATTACHMENTS_START") {
      while (++i < lines.size() && lines[i] != "ATTACHMENTS_END")
        if (!lines[i].empty())
          r.attachments.push_back(lines[i]);
    }
  }
  return r;
}

util::Result<std::vector<ChangeRequest>> ChangeRequestStore::loadAll() const {
  if (!available())
    return std::unexpected(util::Error{"Ordner für Änderungswünsche nicht erreichbar: " + util::pathToUtf8(mDir)});
  std::vector<ChangeRequest> out;
  for (const auto &folder : util::listDirectory(mDir, true, false)) {
    const auto info = folder / "info.txt";
    if (!util::pathExists(info))
      continue;
    auto content = util::readFile(info);
    if (!content)
      continue;
    ChangeRequest r = parseInfoFile(*content);
    if (r.id == 0)
      r.id = util::parseUInt64(util::pathToUtf8(folder.filename())).value_or(0);
    out.push_back(std::move(r));
  }
  std::sort(out.begin(), out.end(), [](const ChangeRequest &a, const ChangeRequest &b) {
    if (a.created != b.created)
      return a.created > b.created;
    return a.id > b.id;
  });
  return out;
}

util::Result<ChangeRequest> ChangeRequestStore::submit(std::string subject, std::string message, const std::vector<util::fs::path> &attachments,
                                                       std::string author, std::string created) const {
  if (util::trim(subject).empty() || util::trim(message).empty())
    return std::unexpected(util::Error{"Titel und Beschreibung müssen ausgefüllt sein"});
  static std::mt19937_64 gen{std::random_device{}()};
  ChangeRequest r;
  do {
    r.id = gen();
  } while (r.id == 0 || util::pathExists(mDir / std::to_string(r.id)));
  r.subject = std::move(subject);
  r.message = std::move(message);
  r.author = std::move(author);
  r.created = std::move(created);
  r.open = true;
  const auto folder = mDir / std::to_string(r.id);
  const auto attachmentsDir = folder / "attachments";
  if (auto s = util::createDirectories(attachmentsDir); !s)
    return std::unexpected(s.error());
  for (const auto &file : attachments) {
    const auto target = attachmentsDir / file.filename();
    if (auto s = util::copyFile(file, target); !s)
      return std::unexpected(s.error());
    r.attachments.push_back(util::pathToUtf8(target));
  }
  if (auto s = util::writeFileAtomic(folder / "info.txt", writeInfoFile(r)); !s)
    return std::unexpected(s.error());
  return r;
}

util::Status ChangeRequestStore::setOpen(std::uint64_t id, bool open) const {
  const auto info = mDir / std::to_string(id) / "info.txt";
  auto content = util::readFile(info);
  if (!content)
    return std::unexpected(content.error());
  ChangeRequest r = parseInfoFile(*content);
  r.open = open;
  return util::writeFileAtomic(info, writeInfoFile(r));
}

} // namespace st::changes
