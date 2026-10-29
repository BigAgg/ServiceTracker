#include "st/translate/translator.h"

#include "st/util/json.h"
#include "st/util/log.h"
#include "st/util/strings.h"

#include <set>

namespace st::translate {

namespace {

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r'; }

/// One line of a text split into indentation, translatable core and trailing space.
struct Line {
  std::string lead;
  std::string core;
};

Line splitLine(std::string_view line) {
  std::size_t begin = 0;
  while (begin < line.size() && isSpace(line[begin]))
    ++begin;
  std::size_t end = line.size();
  while (end > begin && isSpace(line[end - 1]))
    --end;
  return {std::string(line.substr(0, begin)), std::string(line.substr(begin, end - begin))};
}

std::vector<std::string> splitNewlines(std::string_view text) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (true) {
    const auto pos = text.find('\n', start);
    lines.emplace_back(text.substr(start, pos == std::string_view::npos ? std::string_view::npos : pos - start));
    if (pos == std::string_view::npos)
      break;
    start = pos + 1;
  }
  return lines;
}

} // namespace

util::Status TranslationCache::readFrom(std::map<std::string, std::string, std::less<>> &target) const {
  target.clear();
  if (mFile.empty() || !util::pathExists(mFile))
    return {};
  auto j = util::json::load(mFile);
  if (!j)
    return std::unexpected(j.error());
  if (!j->is_object())
    return std::unexpected(util::Error{"Übersetzungsdatei muss ein JSON-Objekt sein: " + util::pathToUtf8(mFile)});
  for (const auto &[key, value] : j->items())
    if (value.is_string() && !util::trim(key).empty())
      target.emplace(util::trim(key), value.get<std::string>());
  return {};
}

util::Status TranslationCache::load() {
  std::map<std::string, std::string, std::less<>> entries;
  if (auto s = readFrom(entries); !s)
    return s;
  mEntries = std::move(entries);
  mLastWrite = util::lastWriteTime(mFile);
  return {};
}

util::Status TranslationCache::refresh() {
  if (util::lastWriteTime(mFile) == mLastWrite)
    return {};
  return load();
}

std::optional<std::string> TranslationCache::find(std::string_view text) const {
  const auto it = mEntries.find(util::trim(text));
  if (it == mEntries.end())
    return std::nullopt;
  return it->second;
}

util::Status TranslationCache::add(const std::map<std::string, std::string> &entries) {
  if (mFile.empty())
    return std::unexpected(util::Error{"Keine Übersetzungsdatei konfiguriert."});
  // read-modify-write: entries of other users and manual corrections stay untouched
  std::map<std::string, std::string, std::less<>> merged;
  if (auto s = readFrom(merged); !s)
    return s;
  bool changed = false;
  for (const auto &[source, target] : entries) {
    const std::string key = util::trim(source);
    if (key.empty())
      continue;
    changed |= merged.emplace(key, target).second;
  }
  if (changed) {
    util::json::Json j = util::json::Json::object();
    for (const auto &[source, target] : merged)
      j[source] = target;
    if (auto s = util::json::save(mFile, j); !s)
      return s;
  }
  mEntries = std::move(merged);
  mLastWrite = util::lastWriteTime(mFile);
  return {};
}

bool needsTranslation(std::string_view line) {
  for (std::size_t i = 0; i < line.size(); ++i) {
    const auto c = static_cast<unsigned char>(line[i]);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
      return true;
    // UTF-8 lead bytes of Latin-1 supplement / Latin extended-A (ä, ö, ü, ß, é, ...)
    if ((c == 0xC3 || c == 0xC4 || c == 0xC5) && i + 1 < line.size())
      return true;
  }
  return false;
}

util::Result<std::vector<std::string>> Translator::translate(const std::vector<std::string> &texts) {
  mLastBackendLines = 0;
  if (auto s = mCache.refresh(); !s)
    log::warning("Übersetzungsdatei konnte nicht gelesen werden: {}", s.error().message);

  // collect the distinct lines the cache does not know
  std::vector<std::string> missing;
  std::set<std::string, std::less<>> seen;
  std::vector<std::vector<Line>> split;
  split.reserve(texts.size());
  for (const auto &text : texts) {
    auto &lines = split.emplace_back();
    for (const auto &raw : splitNewlines(text)) {
      Line line = splitLine(raw);
      if (needsTranslation(line.core) && !mCache.find(line.core) && seen.insert(line.core).second)
        missing.push_back(line.core);
      lines.push_back(std::move(line));
    }
  }

  std::map<std::string, std::string> fresh;
  if (!missing.empty()) {
    if (!mBackend)
      return std::unexpected(util::Error{"Kein Übersetzungsdienst verfügbar und " + std::to_string(missing.size()) +
                                         " Zeile(n) sind noch nicht in der Übersetzungsdatei."});
    for (std::size_t start = 0; start < missing.size(); start += mBatchSize) {
      const auto end = std::min(missing.size(), start + mBatchSize);
      const std::vector<std::string> batch(missing.begin() + static_cast<std::ptrdiff_t>(start), missing.begin() + static_cast<std::ptrdiff_t>(end));
      auto translated = mBackend->translate(batch);
      if (!translated)
        return std::unexpected(translated.error());
      if (translated->size() != batch.size())
        return std::unexpected(util::Error{"Übersetzungsdienst lieferte eine falsche Anzahl an Zeilen."});
      for (std::size_t i = 0; i < batch.size(); ++i)
        fresh.emplace(batch[i], util::trim((*translated)[i]));
    }
    mLastBackendLines = fresh.size();
    // a failing cache write must not cost the user his translation
    if (auto s = mCache.add(fresh); !s)
      log::warning("Übersetzungen konnten nicht gespeichert werden: {}", s.error().message);
  }

  std::vector<std::string> out;
  out.reserve(texts.size());
  for (const auto &lines : split) {
    std::string text;
    for (std::size_t i = 0; i < lines.size(); ++i) {
      if (i > 0)
        text += '\n';
      const auto &line = lines[i];
      std::string translated = line.core;
      if (needsTranslation(line.core)) {
        if (const auto cached = mCache.find(line.core))
          translated = *cached;
        else if (const auto it = fresh.find(line.core); it != fresh.end())
          translated = it->second;
      }
      text += line.lead + translated;
    }
    out.push_back(std::move(text));
  }
  return out;
}

} // namespace st::translate
