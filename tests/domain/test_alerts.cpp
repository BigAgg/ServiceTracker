#include <doctest/doctest.h>

#include "st/alerts/alert_center.h"

using namespace st::alerts;

namespace {

Channel summaryChannel(bool notifyOnStart = true) {
  Channel c;
  c.id = "orders.prepared";
  c.label = "Bestellungen versandbereit";
  c.title = "ServiceTracker - Bestellungen";
  c.target = "Bestellungen";
  c.notifyOnStart = notifyOnStart;
  c.summaryText = [](std::size_t n) { return countSentence(n, "Bestellung", "Bestellungen", "versandbereit"); };
  return c;
}

Channel itemChannel() {
  Channel c;
  c.id = "devices.received";
  c.label = "Neue Geräte";
  c.title = "ServiceTracker - Wareneingang";
  c.mode = Mode::PerItem;
  c.maxItemNotifications = 2;
  c.itemText = [](const Item &item) { return "Neuer Reader im Wareneingang " + item.label; };
  c.summaryText = [](std::size_t n) { return std::to_string(n) + " Reader im Wareneingang"; };
  return c;
}

std::vector<Item> items(std::initializer_list<const char *> ids) {
  std::vector<Item> result;
  for (const char *id : ids)
    result.push_back({id, id});
  return result;
}

} // namespace

TEST_CASE("alerts: count sentence") {
  CHECK(countSentence(1, "Bestellung", "Bestellungen", "versandbereit") == "Es ist 1 Bestellung versandbereit");
  CHECK(countSentence(3, "Bestellung", "Bestellungen", "versandbereit") == "Es sind 3 Bestellungen versandbereit");
  CHECK(countSentence(0, "Bestellung", "Bestellungen", "versandbereit") == "Es sind 0 Bestellungen versandbereit");
}

TEST_CASE("alerts: channel registration") {
  AlertCenter center;
  CHECK(center.addChannel(summaryChannel()));
  CHECK_FALSE(center.addChannel(summaryChannel()));
  CHECK_FALSE(center.addChannel(Channel{}));
  CHECK(center.hasChannel("orders.prepared"));
  CHECK(center.channels().size() == 1);
  // Reports for unknown channels are ignored (user not in the audience)
  center.report("unknown", items({"1"}));
  CHECK_FALSE(center.hasPending());
}

TEST_CASE("alerts: summary channel announces start state and new items only") {
  AlertCenter center;
  center.addChannel(summaryChannel());

  SUBCASE("nothing at start") {
    center.report("orders.prepared", {});
    CHECK_FALSE(center.hasPending());
  }
  SUBCASE("existing items at start") {
    center.report("orders.prepared", items({"1", "2"}));
    const auto pending = center.takePending();
    REQUIRE(pending.size() == 1);
    CHECK(pending[0] == Notification{"orders.prepared", "ServiceTracker - Bestellungen", "Es sind 2 Bestellungen versandbereit", "Bestellungen", "orders.prepared"});
    CHECK_FALSE(center.hasPending());
  }

  center.report("orders.prepared", items({"1", "2"}));
  center.takePending();
  center.report("orders.prepared", items({"1", "2"}));
  CHECK_FALSE(center.hasPending()); // unchanged

  center.report("orders.prepared", items({"1"}));
  CHECK_FALSE(center.hasPending()); // shipped orders are no news
  CHECK(center.count("orders.prepared") == 1);

  center.report("orders.prepared", items({"1", "3"}));
  auto pending = center.takePending();
  REQUIRE(pending.size() == 1);
  CHECK(pending[0].body == "Es sind 2 Bestellungen versandbereit");

  // Several reports before the notification is shown: only the newest count
  center.report("orders.prepared", items({"1", "3", "4"}));
  center.report("orders.prepared", items({"1", "3", "4", "5"}));
  pending = center.takePending();
  REQUIRE(pending.size() == 1);
  CHECK(pending[0].body == "Es sind 4 Bestellungen versandbereit");
}

TEST_CASE("alerts: queued summary is dropped when the items are gone") {
  AlertCenter center;
  center.addChannel(summaryChannel());
  center.report("orders.prepared", items({"1"}));
  center.report("orders.prepared", {});
  CHECK_FALSE(center.hasPending());
  CHECK(center.activeCount() == 0);
}

TEST_CASE("alerts: notifyOnStart = false ignores the first report") {
  AlertCenter center;
  center.addChannel(summaryChannel(false));
  center.report("orders.prepared", items({"1"}));
  CHECK_FALSE(center.hasPending());
  CHECK(center.activeCount() == 1);
  center.report("orders.prepared", items({"1", "2"}));
  CHECK(center.takePending().size() == 1);
}

TEST_CASE("alerts: markSeen suppresses own changes") {
  AlertCenter center;
  center.addChannel(summaryChannel());
  center.report("orders.prepared", {});
  center.markSeen("orders.prepared", "7");
  center.report("orders.prepared", items({"7"}));
  CHECK_FALSE(center.hasPending());
  // Another user's order in the same refresh is still announced
  center.markSeen("orders.prepared", "8");
  center.report("orders.prepared", items({"7", "8", "9"}));
  CHECK(center.takePending().size() == 1);
}

TEST_CASE("alerts: an item that comes back is announced again") {
  AlertCenter center;
  center.addChannel(summaryChannel());
  center.report("orders.prepared", items({"1"}));
  center.takePending();
  center.report("orders.prepared", {});
  center.report("orders.prepared", items({"1"}));
  CHECK(center.takePending().size() == 1);
}

TEST_CASE("alerts: per item channel and burst collapsing") {
  AlertCenter center;
  center.addChannel(itemChannel());
  center.report("devices.received", items({"0601-1"}));
  auto pending = center.takePending();
  REQUIRE(pending.size() == 1);
  CHECK(pending[0].body == "Neuer Reader im Wareneingang 0601-1");
  CHECK(pending[0].key == "devices.received/0601-1"); // separate notification per device

  center.report("devices.received", items({"0601-1", "0601-2", "0601-3"}));
  pending = center.takePending();
  REQUIRE(pending.size() == 2);
  CHECK(pending[0].body == "Neuer Reader im Wareneingang 0601-2");
  CHECK(pending[1].body == "Neuer Reader im Wareneingang 0601-3");

  center.report("devices.received", items({"0601-1", "0601-2", "0601-3", "a", "b", "c"}));
  pending = center.takePending();
  REQUIRE(pending.size() == 1);
  CHECK(pending[0].body == "6 Reader im Wareneingang");
  CHECK(pending[0].key == "devices.received");
}

TEST_CASE("alerts: muted channels") {
  AlertCenter center;
  center.addChannel(summaryChannel());
  center.addChannel(itemChannel());
  center.setMutedChannels({"orders.prepared", "later.registered"});
  CHECK(center.isMuted("orders.prepared"));
  CHECK(center.isMuted("later.registered"));

  center.report("orders.prepared", items({"1"}));
  center.report("devices.received", items({"x"}));
  const auto pending = center.takePending();
  REQUIRE(pending.size() == 1);
  CHECK(pending[0].channel == "devices.received");
  CHECK(center.activeCount() == 1);
  CHECK(center.summaryLines() == std::vector<std::string>{"1 Reader im Wareneingang"});
  CHECK(center.firstActiveTarget().empty()); // devices channel has no target

  // Unmuting does not replay old items, but counts again
  center.setMutedChannels({});
  CHECK_FALSE(center.isMuted("orders.prepared"));
  center.report("orders.prepared", items({"1"}));
  CHECK_FALSE(center.hasPending());
  CHECK(center.activeCount() == 2);
  CHECK(center.firstActiveTarget() == "Bestellungen");

  // Muting drops queued notifications
  center.report("orders.prepared", items({"1", "2"}));
  center.setMuted("orders.prepared", true);
  CHECK_FALSE(center.hasPending());
}
