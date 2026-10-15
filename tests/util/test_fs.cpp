#include <doctest/doctest.h>

#include "st/util/fs.h"
#include "st/util/json.h"
#include "test_helpers.h"

#include <chrono>
#include <filesystem>
#include <string>

using namespace st;
using namespace st::util;

TEST_CASE("writeFileAtomic creates parent folders and replaces content") {
  test::TempDir dir;
  const auto file = dir / "a/b/c.txt";
  REQUIRE(writeFileAtomic(file, "eins"));
  CHECK(test::readText(file) == "eins");
  REQUIRE(writeFileAtomic(file, "zwei"));
  CHECK(test::readText(file) == "zwei");
  // no temporary files are left behind
  CHECK(listDirectory(file.parent_path()).size() == 1);
}

TEST_CASE("readFile reports missing files") {
  test::TempDir dir;
  auto r = readFile(dir / "missing.txt", RetryPolicy{1, std::chrono::milliseconds(1)});
  CHECK_FALSE(r.has_value());
  CHECK(r.error().message.find("missing.txt") != std::string::npos);
}

TEST_CASE("utf8 paths with umlauts round trip") {
  test::TempDir dir;
  const auto file = dir.path() / pathFromUtf8("Gerätenachweise/Prüfung.txt");
  REQUIRE(writeFileAtomic(file, "x"));
  CHECK(pathExists(file));
  CHECK(pathToUtf8(file.filename()) == "Prüfung.txt");
}

TEST_CASE("lastWriteTime is zero for missing files and changes on write") {
  test::TempDir dir;
  const auto file = dir / "t.txt";
  CHECK(lastWriteTime(file) == 0);
  REQUIRE(writeFileAtomic(file, "1"));
  CHECK(lastWriteTime(file) != 0);
  CHECK_FALSE(lastWriteTimeString(file).empty());
}

TEST_CASE("every write gets a newer modification time (coarse timestamps, clock skew)") {
  test::TempDir dir;
  const auto file = dir / "list.json";
  REQUIRE(writeFileAtomic(file, "0"));

  // Many writes in a row: each one must be detectable by other users
  std::int64_t previous = lastWriteTime(file);
  for (int i = 1; i <= 20; ++i) {
    REQUIRE(writeFileAtomic(file, std::to_string(i % 2))); // same size every time
    const std::int64_t current = lastWriteTime(file);
    CHECK(current > previous);
    previous = current;
  }

  // File written by a PC whose clock is ahead (or a share with coarse time stamps)
  const auto future = std::filesystem::last_write_time(file) + std::chrono::hours(1);
  std::filesystem::last_write_time(file, future);
  REQUIRE(writeFileAtomic(file, "x"));
  CHECK(std::filesystem::last_write_time(file) > future);
  CHECK(test::readText(file) == "x");
}

TEST_CASE("copyTree merges directories") {
  test::TempDir dir;
  REQUIRE(writeFileAtomic(dir / "src/x/1.txt", "1"));
  REQUIRE(writeFileAtomic(dir / "dst/x/2.txt", "2"));
  REQUIRE(copyTree(dir / "src", dir / "dst"));
  CHECK(pathExists(dir / "dst/x/1.txt"));
  CHECK(pathExists(dir / "dst/x/2.txt"));
  REQUIRE(removeAll(dir / "src"));
  CHECK_FALSE(pathExists(dir / "src"));
  CHECK(removeAll(dir / "does-not-exist"));
}

TEST_CASE("json helpers are tolerant") {
  const auto j = json::Json::parse(R"({"s":"x","n":"1.5","i":3,"b":1,"arr":["a",1,"b"]})");
  CHECK(json::str(j, "s") == "x");
  CHECK(json::str(j, "missing", "fb") == "fb");
  CHECK(json::number(j, "n") == doctest::Approx(1.5));
  CHECK(json::integer(j, "i") == 3);
  CHECK(json::boolean(j, "b"));
  CHECK(json::strings(j, "arr") == std::vector<std::string>{"a", "b"});
  CHECK(json::array(j, "s").empty());
  CHECK_FALSE(json::parse("{broken").has_value());
}
