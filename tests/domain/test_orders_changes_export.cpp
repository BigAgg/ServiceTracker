#include <doctest/doctest.h>

#include "st/changes/change_requests.h"
#include "st/exporting/reports.h"
#include "st/orders/order_store.h"
#include "test_helpers.h"

using namespace st;
using namespace st::orders;

TEST_CASE("order state follows the filled dates") {
  Order o;
  CHECK(o.state() == OrderState::Received);
  o.bookedDate = "2026.01.02";
  CHECK(o.state() == OrderState::Booked);
  o.preparedDate = "2026.01.03";
  CHECK(o.state() == OrderState::Prepared);
  o.shippingDate = "2026.01.04";
  CHECK(o.state() == OrderState::Shipped);
  o.finishedDate = "2026.01.05";
  CHECK(o.state() == OrderState::Finished);
  o.cancelledDate = "2026.01.06";
  CHECK(o.state() == OrderState::Cancelled);
  CHECK(stateLabel(OrderState::Prepared) == "Versandbereit");
  o.entryDate = "2026.01.01";
  CHECK(o.processingDays() == 4);
}

TEST_CASE("order validation") {
  Order o;
  CHECK(missingFields(o) == std::vector<std::string>{"Bestellnummer", "Kunde", "Eingangsdatum"});
  o.orderNumber = "A";
  o.customer = "BMG UK";
  o.entryDate = "2026.01.01";
  CHECK(missingFields(o).empty());
}

TEST_CASE("order store: legacy file, concurrent users, unknown keys") {
  test::TempDir dir;
  const auto file = dir / "Bestellungen.json";
  test::writeText(file, R"({"Bestellungen": [
    {"id": 18446744073709551615, "Bestellnummer": "ORD-1", "Kunde": "BMG UK", "Eingangsdatum": "2026.01.01",
     "Letzte Änderung": "2026-01-01 10:00:00", "Preis": 12.5, "Expressversand": true, "Sonderfeld": "x"},
    {"id": 0, "Bestellnummer": "ohne id"}
  ], "meta": 1})");

  OrderStore a(file);
  OrderStore b(file);
  REQUIRE(a.load());
  REQUIRE(b.load());
  REQUIRE(a.orders().size() == 1);
  const Order *legacy = a.find(18446744073709551615ULL);
  REQUIRE(legacy);
  CHECK(legacy->express);
  CHECK(legacy->price == 12.5);
  CHECK(legacy->extra["Sonderfeld"] == "x");

  test::TestSession ts;
  Order created;
  created.orderNumber = "ORD-2";
  created.customer = "BMG USA";
  created.entryDate = "2026.02.01";
  auto stored = a.upsert(created, ts.session);
  REQUIRE(stored.has_value());
  CHECK(stored->id != 0);
  CHECK(stored->changeTime == "2026-03-15 10:00:00");

  // user B edits the legacy order without knowing ORD-2
  ts.advance();
  Order edit = *b.find(18446744073709551615ULL);
  edit.bookedDate = "2026.01.02";
  REQUIRE(b.upsert(edit, ts.session).has_value());
  CHECK(b.orders().size() == 2); // merged ORD-2 before writing

  // user A pulls the change
  auto refreshed = a.refresh();
  REQUIRE(refreshed.has_value());
  CHECK(*refreshed);
  CHECK(a.find(18446744073709551615ULL)->state() == OrderState::Booked);
  CHECK_FALSE(*a.refresh());

  CHECK(a.idsInState(OrderState::Received) == std::vector<std::uint64_t>{stored->id});
  CHECK(a.countInState(OrderState::Booked) == 1);
  CHECK(a.search("usa") == std::vector<std::uint64_t>{stored->id});
  CHECK(a.search("  ").empty());
  CHECK(a.customers() == std::vector<std::string>{"BMG UK", "BMG USA"});

  const auto raw = util::json::load(file);
  REQUIRE(raw.has_value());
  CHECK((*raw)["meta"] == 1);
  CHECK((*raw)["Bestellungen"].size() == 2);
}

TEST_CASE("order report") {
  std::vector<Order> list(3);
  list[0].customer = "BMG UK";
  list[0].price = 10;
  list[0].entryDate = "2026.01.01";
  list[0].finishedDate = "2026.01.05";
  list[0].preparedDate = "2026.01.02";
  list[0].shippingDate = "2026.01.04";
  list[1].customer = "BMG UK";
  list[1].cancelledDate = "2026.01.02";
  list[1].price = 99;
  list[2].customer = "";
  list[2].price = 5;
  const auto sheets = exporting::buildOrderReport(list);
  REQUIRE(sheets.size() == 3);
  CHECK(sheets[0].name == "Bestellungen");
  CHECK(sheets[1].name == "BMG UK");
  CHECK(sheets[2].name == "Ohne Kunde");
  const auto &rows = sheets[0].rows;
  CHECK(std::get<long long>(rows[0][1]) == 3);   // total
  CHECK(std::get<long long>(rows[1][1]) == 1);   // open
  CHECK(std::get<long long>(rows[2][1]) == 1);   // finished
  CHECK(std::get<long long>(rows[3][1]) == 1);   // shipped
  CHECK(std::get<long long>(rows[4][1]) == 1);   // cancelled
  CHECK(std::get<double>(rows[5][1]) == 15.0);   // revenue without cancelled
  CHECK(std::get<double>(rows[6][1]) == 4.0);    // average processing
  CHECK(std::get<double>(rows[8][1]) == 2.0);    // average shipping
  CHECK(rows.size() == 10 + 1 + 1 + 3);
}

TEST_CASE("history report") {
  std::vector<service::HistoryEntry> rows(2);
  rows[0].device = "0601-1";
  rows[0].totalPrice = 10;
  rows[1].device = "0471-1";
  const auto sheets = exporting::buildHistoryReport(rows);
  REQUIRE(sheets.size() == 3);
  CHECK(sheets[0].name == "Jahresinformation");
  CHECK(sheets[1].name == "0471");
  CHECK(std::get<long long>(sheets[2].rows[0][1]) == 1);
}

TEST_CASE("sheet names are sanitised for Excel") {
  CHECK(exporting::sanitizeSheetName("a/b:c", {}) == "a_b_c");
  CHECK(exporting::sanitizeSheetName("", {}) == "Tabelle");
  CHECK(exporting::sanitizeSheetName("X", {"x"}) == "X (2)");
  CHECK(exporting::sanitizeSheetName(std::string(40, 'a'), {}).size() == 31);
  const auto umlauts = exporting::sanitizeSheetName(std::string(30, 'a') + "ä", {});
  CHECK(umlauts.size() == 30); // never cut inside a UTF-8 sequence
}

TEST_CASE("xlsx file is written") {
  if (!exporting::xlsxAvailable())
    return;
  test::TempDir dir;
  const auto file = dir / "report.xlsx";
  std::vector<exporting::Sheet> sheets(1);
  sheets[0].name = "Test";
  sheets[0].rows = {{std::string("a"), 1.5, 2LL, true, std::monostate{}}, {std::string("mehr\nzeilig")}};
  sheets[0].boldRows = {0};
  REQUIRE(exporting::writeXlsx(file, sheets));
  const auto content = test::readText(file);
  CHECK(content.starts_with("PK")); // zip container
}

TEST_CASE("change requests: legacy info.txt and workflow") {
  const auto legacy = changes::parseInfoFile("ID=42\nSTATUS=OPEN\nSUBJECT=Titel\nMESSAGE_START\nZeile 1\nZeile 2\nMESSAGE_END\nATTACHMENTS_START\nY:/a.png\nATTACHMENTS_END\n");
  CHECK(legacy.id == 42);
  CHECK(legacy.open);
  CHECK(legacy.message == "Zeile 1\nZeile 2");
  CHECK(legacy.attachments == std::vector<std::string>{"Y:/a.png"});
  CHECK(changes::parseInfoFile(changes::writeInfoFile(legacy)) == legacy);
  CHECK_FALSE(changes::parseInfoFile("ID=1\nSTATUS=CLOSED\n").open);

  test::TempDir dir;
  REQUIRE(util::createDirectories(dir / "REQUESTS"));
  test::writeText(dir / "screenshot.png", "png");
  changes::ChangeRequestStore store(dir / "REQUESTS");
  CHECK(store.available());
  CHECK_FALSE(store.submit("", "x", {}, "", "").has_value());
  auto created = store.submit("Neuer Filter", "Bitte einbauen", {dir / "screenshot.png"}, "tester", "2026-03-15 10:00:00");
  REQUIRE(created.has_value());
  CHECK(util::pathExists(util::pathFromUtf8(created->attachments.at(0))));
  auto all = store.loadAll();
  REQUIRE(all.has_value());
  REQUIRE(all->size() == 1);
  CHECK((*all)[0].subject == "Neuer Filter");
  CHECK((*all)[0].author == "tester");
  REQUIRE(store.setOpen(created->id, false));
  CHECK_FALSE(store.loadAll()->at(0).open);

  changes::ChangeRequestStore missing(dir / "missing");
  CHECK_FALSE(missing.available());
  CHECK_FALSE(missing.loadAll().has_value());
}
