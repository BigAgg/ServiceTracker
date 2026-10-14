#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

/// File system helpers built for files on a shared network drive that several
/// users access at the same time:
///  - every write is atomic (temp file + rename), so readers never see half-written files
///  - reads and writes retry for a short time when a file is temporarily locked
///  - no function throws; errors are returned as `Error`
namespace st::util {

namespace fs = std::filesystem;

/// Human readable error message (German, shown in the UI).
struct Error {
  std::string message;
};

template <typename T>
using Result = std::expected<T, Error>;
using Status = std::expected<void, Error>;

/// How often and how long an operation on a locked file is retried.
struct RetryPolicy {
  int attempts = 30;
  std::chrono::milliseconds delay{100};
};

/// Converts a UTF-8 string into a path (correct for umlauts on Windows).
fs::path pathFromUtf8(std::string_view utf8);

/// Converts a path into a UTF-8 string with forward slashes kept as they are.
std::string pathToUtf8(const fs::path &path);

/// Reads the whole file in binary mode.
Result<std::string> readFile(const fs::path &path, RetryPolicy retry = {});

/// Atomically replaces `path` with `content` (creates parent directories).
Status writeFileAtomic(const fs::path &path, std::string_view content, RetryPolicy retry = {});

/// Last modification time as an integer (0 when the file does not exist).
/// Used to detect changes made by other users.
std::int64_t lastWriteTime(const fs::path &path) noexcept;

/// Last modification time formatted as "YYYY-MM-DD HH:MM:SS" (empty if missing).
std::string lastWriteTimeString(const fs::path &path) noexcept;

bool pathExists(const fs::path &path) noexcept;
bool isDirectory(const fs::path &path) noexcept;

Status createDirectories(const fs::path &path);

/// Copies a file, overwriting the destination.
Status copyFile(const fs::path &from, const fs::path &to);

/// Copies a directory tree into `to` (merging, existing files are overwritten).
Status copyTree(const fs::path &from, const fs::path &to);

/// Removes a file or a directory tree. Missing paths are not an error.
Status removeAll(const fs::path &path);

/// Lists the direct children of a directory (empty on error), sorted by name.
std::vector<fs::path> listDirectory(const fs::path &path, bool includeDirectories = true, bool includeFiles = true);

} // namespace st::util
