#include <doctest/doctest.h>

#include "st/config/preferences.h"
#include "test_helpers.h"

using namespace st;
using namespace st::config;

TEST_CASE("preferences: defaults, legacy checklist.bin and round trip") {
  test::TempDir dir;
  CHECK(loadPreferences(dir / "prefs.json") == Preferences{});
  CHECK(Preferences{}.desktopNotifications);
  CHECK(Preferences{}.taskbarBadge);

  // Files of older versions without the notification keys keep the defaults
  test::writeText(dir / "old.json", R"({"germanDates":true})");
  const auto old = loadPreferences(dir / "old.json");
  CHECK(old.germanDates);
  CHECK(old.desktopNotifications);
  CHECK(old.mutedAlerts.empty());

  test::writeText(dir / "checklist.bin", std::string("\0\1\1\0", 4));
  const auto legacy = loadPreferences(dir / "prefs.json", dir / "checklist.bin");
  CHECK_FALSE(legacy.onlyPermittedSections);
  CHECK(legacy.germanDates);
  CHECK(legacy.ignoreFinished);
  CHECK_FALSE(legacy.ignorePermissions);

  Preferences p;
  p.ordersAsPopup = true;
  p.desktopNotifications = false;
  p.taskbarBadge = false;
  p.mutedAlerts = {"orders.prepared"};
  p.rememberDevice("a");
  p.rememberDevice("b");
  p.rememberDevice("a");
  CHECK(p.recentDevices == std::vector<std::string>{"a", "b"});
  REQUIRE(savePreferences(dir / "prefs.json", p));
  CHECK(loadPreferences(dir / "prefs.json", dir / "checklist.bin") == p); // json wins

  for (int i = 0; i < 20; ++i)
    p.rememberDevice(std::to_string(i), 5);
  CHECK(p.recentDevices.size() == 5);
  CHECK(p.recentDevices.front() == "19");

  p.ignoreFinished = p.ignorePermissions = true;
  p.restrictTo(true);
  CHECK(p.ignoreFinished);
  p.restrictTo(false);
  CHECK_FALSE(p.ignoreFinished);
  CHECK_FALSE(p.ignorePermissions);
}
