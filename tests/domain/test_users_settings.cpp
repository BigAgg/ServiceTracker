#include <doctest/doctest.h>

#include "st/config/settings.h"
#include "st/users/user_store.h"
#include "test_helpers.h"

#include <cstring>

using namespace st;
using namespace st::users;

namespace {
/// Builds a record exactly like the legacy UserControl::SaveTable.
std::string legacyRecord(const std::string &plain) {
  std::string data = plain;
  for (char &c : data)
    c = static_cast<char>(c ^ 0x5A);
  const auto size = static_cast<std::uint32_t>(data.size());
  std::string out(sizeof(size), '\0');
  std::memcpy(out.data(), &size, sizeof(size));
  return out + data;
}
} // namespace

TEST_CASE("user table: legacy binary format") {
  const std::string bytes = legacyRecord("ajahraus[1]=") + legacyRecord("mmueller[0]=TECHSUPPORT,LAGER") + legacyRecord("sales[0]=SALES");
  const auto users = decodeUserTable(bytes);
  REQUIRE(users.size() == 3);
  CHECK(users[0].name == "ajahraus");
  CHECK(users[0].poweruser);
  CHECK(users[0].groups.empty());
  CHECK(users[1].groups == std::vector<std::string>{"TECHSUPPORT", "LAGER"});
  CHECK(encodeUserTable(users) == bytes);
}

TEST_CASE("user table: truncated data is ignored") {
  std::string bytes = legacyRecord("a[0]=X") + legacyRecord("b[1]=Y");
  bytes.resize(bytes.size() - 2);
  const auto users = decodeUserTable(bytes);
  REQUIRE(users.size() == 1);
  CHECK(users[0].name == "a");
}

TEST_CASE("user store: permissions") {
  test::TempDir dir;
  const auto file = dir / "users.bin";
  test::writeText(file, legacyRecord("MMueller[0]=TECHSUPPORT") + legacyRecord("lager[0]=LAGER"));
  UserStore store(file, "admin");
  REQUIRE(store.load());

  CHECK(store.isPoweruser("admin")); // bootstrap admin always exists
  CHECK(store.hasAccess("mmueller", "TECHSUPPORT")); // case insensitive names
  CHECK_FALSE(store.hasAccess("mmueller", "LAGER"));
  CHECK_FALSE(store.hasAccess("unbekannt", "LAGER"));
  CHECK(store.hasAnyAccess("lager", {"TECHSUPPORT", "LAGER"}));
  CHECK(store.hasAnyAccess("unbekannt", {})); // no restriction
  CHECK(store.knownGroups() == std::vector<std::string>{"LAGER", "TECHSUPPORT"});

  REQUIRE(store.upsert(User{"Neu", {"SALES", "SALES", ""}, false}));
  UserStore reloaded(file, "admin");
  REQUIRE(reloaded.load());
  REQUIRE(reloaded.find("neu"));
  CHECK(reloaded.find("neu")->groups == std::vector<std::string>{"SALES"});

  REQUIRE(reloaded.remove("neu"));
  CHECK_FALSE(reloaded.find("neu"));
  CHECK_FALSE(reloaded.remove("admin").has_value());
  CHECK_FALSE(reloaded.upsert(User{" ", {}, false}).has_value());
}

TEST_CASE("user store: missing file means empty table") {
  test::TempDir dir;
  UserStore store(dir / "users.bin", "admin");
  REQUIRE(store.load());
  CHECK(store.names() == std::vector<std::string>{"admin"});
}

TEST_CASE("user store: own permissions are cached for offline work") {
  test::TempDir dir;
  const auto server = dir / "server/users.bin";
  const auto cache = dir / "local/users_cache.bin";
  test::writeText(server, encodeUserTable({{"mmueller", {"TECHSUPPORT"}, false}, {"other", {"SALES"}, true}}));

  UserStore online(server, "admin");
  CHECK(online.loadWithCache(cache, "MMueller") == UserStore::Source::Server);
  CHECK(online.hasAccess("mmueller", "TECHSUPPORT"));
  REQUIRE(st::util::pathExists(cache));
  CHECK(decodeUserTable(test::readText(cache)).size() == 1); // only the own entry

  // offline: server folder not reachable
  std::filesystem::remove_all(dir / "server");
  UserStore offline(server, "admin");
  CHECK(offline.loadWithCache(cache, "mmueller") == UserStore::Source::Cache);
  CHECK(offline.hasAccess("mmueller", "TECHSUPPORT"));
  CHECK_FALSE(offline.hasAccess("other", "SALES"));
  CHECK(offline.isPoweruser("admin")); // bootstrap admin stays

  UserStore nothing(server, "admin");
  CHECK(nothing.loadWithCache(dir / "local/missing.bin", "mmueller") == UserStore::Source::None);
  CHECK_FALSE(nothing.hasAccess("mmueller", "TECHSUPPORT"));
}

TEST_CASE("settings: defaults, partial files and round trip") {
  test::TempDir dir;
  auto defaults = config::loadSettings(dir / "missing.json");
  REQUIRE(defaults.has_value());
  CHECK(defaults->paths.devicesRoot == "Y:/Gerätenachweise");
  CHECK(defaults->paths.syncFile() == util::pathFromUtf8("Y:/CrossDept/Automation/ServiceCreatorApp/SYNC") / "Sync.json");

  test::writeText(dir / "partial.json", R"({"paths": {"syncDir": "D:/sync"}, "autosaveDelayMs": 0})");
  auto partial = config::loadSettings(dir / "partial.json");
  REQUIRE(partial.has_value());
  CHECK(partial->paths.syncDir == "D:/sync");
  CHECK(partial->paths.usersFile == defaults->paths.usersFile);
  CHECK(partial->autosaveDelayMs == 0);

  config::Settings custom;
  custom.paths.ordersFile = "X:/orders.json";
  custom.knownCustomers = {"A"};
  REQUIRE(config::saveSettings(dir / "s.json", custom));
  auto loaded = config::loadSettings(dir / "s.json");
  REQUIRE(loaded.has_value());
  CHECK(*loaded == custom);

  test::writeText(dir / "broken.json", "[1,2]");
  CHECK_FALSE(config::loadSettings(dir / "broken.json").has_value());
}

TEST_CASE("settings: the shipped ServiceTracker.json matches the defaults") {
  const auto example = std::filesystem::path(ST_TEST_DATA_DIR) / ".." / ".." / "deploy" / "ServiceTracker.json";
  auto loaded = config::loadSettings(example);
  REQUIRE(loaded.has_value());
  CHECK(*loaded == config::Settings{});
}
