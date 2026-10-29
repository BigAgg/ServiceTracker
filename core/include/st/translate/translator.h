#pragma once

#include "st/util/fs.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// Translation of record texts (German -> English).
///
/// All translations are kept in a shared cache file on the network drive so every user
/// profits from them and the translation service is only asked for unknown lines. The
/// file is a plain JSON object {"German line": "English line", ...} and may be edited
/// by hand: an entry in the file always wins over the translation service.
namespace st::translate {

/// Shared cache of translated lines (see namespace documentation).
class TranslationCache {
public:
  TranslationCache() = default;
  explicit TranslationCache(util::fs::path file) : mFile(std::move(file)) {}

  const util::fs::path &file() const { return mFile; }

  /// Reads the file. A missing file is an empty cache, not an error.
  util::Status load();
  /// Reloads the file if another user changed it since the last load/save.
  util::Status refresh();

  /// Translation of `text` (exact match of the trimmed line).
  std::optional<std::string> find(std::string_view text) const;

  /// Adds translations and writes the file. The file is read again right before
  /// writing; entries already on disk (e.g. corrected by hand) are kept unchanged.
  util::Status add(const std::map<std::string, std::string> &entries);

  std::size_t size() const { return mEntries.size(); }

private:
  util::Status readFrom(std::map<std::string, std::string, std::less<>> &target) const;

  util::fs::path mFile;
  std::map<std::string, std::string, std::less<>> mEntries;
  std::int64_t mLastWrite = 0;
};

/// A translation service (LibreTranslate, fakes in tests).
class Backend {
public:
  virtual ~Backend() = default;
  /// Translates every text; the result has the same size and order.
  virtual util::Result<std::vector<std::string>> translate(const std::vector<std::string> &texts) = 0;
};

/// True if the line contains something to translate (at least one letter).
/// Numbers, dates, symbols and serial numbers like "12-34/5" are kept as they are.
bool needsTranslation(std::string_view line);

/// Translates texts line by line: lines found in the cache are taken from there, all
/// other lines are sent to the backend in batches and added to the cache afterwards.
/// Leading indentation and empty lines are preserved.
class Translator {
public:
  /// `backend` may be null: then only cached lines can be translated.
  Translator(TranslationCache &cache, Backend *backend, std::size_t batchSize = 50)
      : mCache(cache), mBackend(backend), mBatchSize(batchSize == 0 ? 1 : batchSize) {}

  /// Translations of `texts` in the same order.
  util::Result<std::vector<std::string>> translate(const std::vector<std::string> &texts);

  /// Number of lines the backend translated in the last call (statistics for the UI).
  std::size_t lastBackendLines() const { return mLastBackendLines; }

private:
  TranslationCache &mCache;
  Backend *mBackend;
  std::size_t mBatchSize;
  std::size_t mLastBackendLines = 0;
};

} // namespace st::translate
