#include <doctest/doctest.h>

#include "st/service/history.h"
#include "st/service/legacy_import.h"
#include "test_helpers.h"

using namespace st;
using namespace st::service;

TEST_CASE("csv parser handles quotes, separators and line breaks") {
  const auto rows = parseCsv("\xEF\xBB\xBF" "a;b;c\r\n\"x;y\";\"zwei\nZeilen\";\"sagt \"\"hallo\"\"\"\nletzte;;");
  REQUIRE(rows.size() == 3);
  CHECK(rows[0] == std::vector<std::string>{"a", "b", "c"});
  CHECK(rows[1] == std::vector<std::string>{"x;y", "zwei\nZeilen", "sagt \"hallo\""});
  CHECK(rows[2] == std::vector<std::string>{"letzte", "", ""});
}

TEST_CASE("legacy service list is converted into the history format") {
  test::TempDir dir;
  REQUIRE(util::createDirectories(dir / "0601 FLUOstar/0601-100/Service_2024.03.01"));
  REQUIRE(util::createDirectories(dir / "0601 FLUOstar/0601-100/Service_2024.06.10"));
  devices::DeviceRegistry registry;
  REQUIRE(registry.scan(dir.path()));

  const std::string csv =
      "Anlieferung;Gerät;SN;Kunde;Ansprechpartner;Grund;Angebot;Betrag;Auftrag;Freigabe;Versand;DLZ;DLZ Versand;Status;Notizen\n"
      "2024-06-08;601;100;Uni;Dr. X;Reparatur;A-1;1.234,50 €;;;2024-06-20;;12;erledigt;\n"
      "2024-03-01;0601;100;Uni;;Wartung;;;;;2024-03-05;;4;erledigt;\n"
      "2024-03-01;0601;999;Unbekannt;;;;;;;;;;erledigt;\n"
      "2024-03-01;0601;100;Offen;;;;;;;;;;in Arbeit;\n";
  test::writeText(dir / "History.json", util::json::dump(convertLegacyServiceList(csv, registry)));

  auto rows = loadHistoryRows(dir / "History.json");
  REQUIRE(rows.has_value());
  REQUIRE(rows->size() == 2);
  const auto &first = (*rows)[0];
  CHECK(first.device == "0601-100");
  CHECK(first.record.activeService == "Service_2024.06.10");
  CHECK(first.totalPrice == doctest::Approx(1234.5));
  CHECK(first.endDate == "2024.06.20");
  CHECK(first.record.duration == 12);
  CHECK(first.record.additionalInfo == "Kunde: Uni\nAnsprechpartner: Dr. X\nGrund: Reparatur\nAngebot: A-1\nAuftrag: n/a");
  CHECK((*rows)[1].record.activeService == "Service_2024.03.01");
}
