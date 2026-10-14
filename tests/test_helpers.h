#pragma once

#include "st/util/fs.h"
#include "st/util/session.h"

#include <atomic>
#include <filesystem>
#include <random>
#include <string>

namespace st::test {

/// Temporary directory that is removed when the object goes out of scope.
class TempDir {
public:
  TempDir() {
    static std::atomic<int> counter{0};
    mPath = std::filesystem::temp_directory_path() /
            ("st_test_" + std::to_string(std::random_device{}()) + "_" + std::to_string(counter++));
    std::filesystem::create_directories(mPath);
  }
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(mPath, ec);
  }
  TempDir(const TempDir &) = delete;
  TempDir &operator=(const TempDir &) = delete;

  const std::filesystem::path &path() const { return mPath; }
  std::filesystem::path operator/(const std::string &name) const { return mPath / util::pathFromUtf8(name); }

private:
  std::filesystem::path mPath;
};

/// Session with a fixed user and a controllable clock.
struct TestSession {
  util::FixedClock clock{util::makeLocalTime(2026, 3, 15, 10, 0, 0)};
  util::Session session{"tester", clock};

  void advance(int seconds = 1) { clock.advance(std::chrono::seconds(seconds)); }
};

inline std::string readText(const std::filesystem::path &path) {
  auto r = util::readFile(path);
  return r ? *r : std::string();
}

inline void writeText(const std::filesystem::path &path, const std::string &content) {
  (void)util::writeFileAtomic(path, content);
}

inline std::filesystem::path dataFile(const std::string &name) {
  return std::filesystem::path(ST_TEST_DATA_DIR) / name;
}

} // namespace st::test
