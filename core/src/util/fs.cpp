#include "st/util/fs.h"

#include "st/util/log.h"
#include "st/util/time.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include <fstream>
#include <optional>
#include <random>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace st::util {

namespace {
std::string uniqueSuffix() {
  static std::atomic<unsigned> counter{0};
  static const unsigned seed = std::random_device{}();
  return std::format(".st-tmp-{:x}-{}", seed, counter.fetch_add(1));
}

Error makeError(std::string_view what, const fs::path &path, const std::error_code &ec = {}) {
  std::string msg = std::format("{}: \"{}\"", what, pathToUtf8(path));
  if (ec)
    msg += std::format(" ({})", ec.message());
  return Error{std::move(msg)};
}
} // namespace

fs::path pathFromUtf8(std::string_view utf8) {
  return fs::path(std::u8string(utf8.begin(), utf8.end()));
}

std::string pathToUtf8(const fs::path &path) {
  const std::u8string u8 = path.u8string();
  return std::string(u8.begin(), u8.end());
}

Result<std::string> readFile(const fs::path &path, RetryPolicy retry) {
  std::error_code ec;
  if (!fs::exists(path, ec))
    return std::unexpected(makeError("Datei existiert nicht", path));
  for (int attempt = 0; attempt < std::max(1, retry.attempts); ++attempt) {
    std::ifstream in(path, std::ios::binary);
    if (in) {
      std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
      if (!in.bad())
        return content;
    }
    std::this_thread::sleep_for(retry.delay);
  }
  return std::unexpected(makeError("Datei kann nicht gelesen werden", path));
}

namespace {

/// Other users detect changes by the modification time (lastWriteTime). Two saves
/// within the timestamp resolution of the file system (Windows ~ms, network shares up
/// to 2 s) would leave the time unchanged and the second change unnoticed. The new
/// file therefore always gets a time later than the file it replaces.
void ensureNewerWriteTime(const fs::path &tmp, const std::optional<fs::file_time_type> &previous) {
  if (!previous)
    return;
  std::error_code ec;
  if (fs::last_write_time(tmp, ec) > *previous || ec)
    return;
  using namespace std::chrono_literals;
  for (const auto step : {std::chrono::nanoseconds(1ms), std::chrono::nanoseconds(10ms), std::chrono::nanoseconds(1s), std::chrono::nanoseconds(2s)}) {
    fs::last_write_time(tmp, *previous + std::chrono::duration_cast<fs::file_time_type::duration>(step), ec);
    if (!ec && fs::last_write_time(tmp, ec) > *previous && !ec)
      return;
  }
  log::warning("Änderungszeit von {} konnte nicht erhöht werden", pathToUtf8(tmp));
}

} // namespace

Status writeFileAtomic(const fs::path &path, std::string_view content, RetryPolicy retry) {
  std::error_code ec;
  if (path.has_parent_path() && !fs::exists(path.parent_path(), ec)) {
    fs::create_directories(path.parent_path(), ec);
    if (ec)
      return std::unexpected(makeError("Ordner kann nicht erstellt werden", path.parent_path(), ec));
  }
  std::optional<fs::file_time_type> previous;
  if (const auto time = fs::last_write_time(path, ec); !ec)
    previous = time;
  ec.clear();
  fs::path tmp = path;
  tmp += uniqueSuffix();
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out)
      return std::unexpected(makeError("Temporäre Datei kann nicht geschrieben werden", tmp));
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.flush();
    if (!out) {
      out.close();
      fs::remove(tmp, ec);
      return std::unexpected(makeError("Schreiben fehlgeschlagen", tmp));
    }
  }
  ensureNewerWriteTime(tmp, previous); // rename keeps the time of the temp file
  for (int attempt = 0; attempt < std::max(1, retry.attempts); ++attempt) {
    ec.clear();
#ifdef _WIN32
    // Explicitly replace the target; fails while another process holds the file open
    // without FILE_SHARE_DELETE, which is handled by the retry loop.
    if (MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
      return {};
    ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
#else
    fs::rename(tmp, path, ec);
    if (!ec)
      return {};
#endif
    std::this_thread::sleep_for(retry.delay);
  }
  std::error_code ignored;
  fs::remove(tmp, ignored);
  return std::unexpected(makeError("Datei ist gesperrt und kann nicht ersetzt werden", path, ec));
}

std::int64_t lastWriteTime(const fs::path &path) noexcept {
  std::error_code ec;
  const auto time = fs::last_write_time(path, ec);
  if (ec)
    return 0;
  return static_cast<std::int64_t>(time.time_since_epoch().count());
}

std::string lastWriteTimeString(const fs::path &path) noexcept {
  try {
    std::error_code ec;
    const auto time = fs::last_write_time(path, ec);
    if (ec)
      return {};
    const auto sys = std::chrono::clock_cast<std::chrono::system_clock>(time);
    return formatTimestamp(std::chrono::time_point_cast<std::chrono::system_clock::duration>(sys));
  } catch (...) {
    return {};
  }
}

bool pathExists(const fs::path &path) noexcept {
  std::error_code ec;
  return !path.empty() && fs::exists(path, ec);
}

bool isDirectory(const fs::path &path) noexcept {
  std::error_code ec;
  return fs::is_directory(path, ec);
}

Status createDirectories(const fs::path &path) {
  std::error_code ec;
  fs::create_directories(path, ec);
  if (ec)
    return std::unexpected(makeError("Ordner kann nicht erstellt werden", path, ec));
  return {};
}

Status copyFile(const fs::path &from, const fs::path &to) {
  std::error_code ec;
  if (to.has_parent_path())
    fs::create_directories(to.parent_path(), ec);
  ec.clear();
  fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
  if (ec)
    return std::unexpected(makeError("Datei kann nicht kopiert werden", from, ec));
  return {};
}

Status copyTree(const fs::path &from, const fs::path &to) {
  std::error_code ec;
  if (!fs::exists(from, ec))
    return std::unexpected(makeError("Quelle existiert nicht", from));
  fs::create_directories(to, ec);
  ec.clear();
  fs::copy(from, to, fs::copy_options::overwrite_existing | fs::copy_options::recursive, ec);
  if (ec)
    return std::unexpected(makeError("Ordner kann nicht kopiert werden", from, ec));
  return {};
}

Status removeAll(const fs::path &path) {
  std::error_code ec;
  fs::remove_all(path, ec);
  if (ec)
    return std::unexpected(makeError("Löschen fehlgeschlagen", path, ec));
  return {};
}

std::vector<fs::path> listDirectory(const fs::path &path, bool includeDirectories, bool includeFiles) {
  std::vector<fs::path> out;
  std::error_code ec;
  for (fs::directory_iterator it(path, ec), end; !ec && it != end; it.increment(ec)) {
    const bool dir = it->is_directory(ec);
    if ((dir && includeDirectories) || (!dir && includeFiles))
      out.push_back(it->path());
  }
  std::sort(out.begin(), out.end());
  return out;
}

} // namespace st::util
