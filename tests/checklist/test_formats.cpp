#include <doctest/doctest.h>

#include "st/checklist/formats.h"
#include "st/util/strings.h"
#include "test_helpers.h"

using namespace st;
using namespace st::checklist;

namespace {
Checklist loadFixture() {
  return parseLegacyText(test::readText(test::dataFile("legacy_checklist.txt")));
}
} // namespace

TEST_CASE("legacy text: fixture is parsed completely") {
  const Checklist c = loadFixture();
  CHECK(c.version == "1");
  CHECK(c.user == "mmueller");
  CHECK(c.timestamp == "2026-02-01 12:00:00");
  CHECK_FALSE(c.finished);
  REQUIRE(c.sections.size() == 2);

  const Section &s = c.sections[0];
  CHECK(s.name == "Wareneingang");
  CHECK(s.time == doctest::Approx(1.3));
  CHECK(s.finished);
  CHECK(s.user == "lager");
  CHECK(s.postAction == "update_status_rinfo");
  CHECK(s.postActionArgs == "Technik");
  CHECK(s.permissions == std::vector<std::string>{"LAGER", "TECHSUPPORT"});
  CHECK(s.postPopup.title == "Hinweis");
  CHECK(s.postPopup.text == "Gerät an Technik übergeben\nDanke");
  REQUIRE(s.items.size() == 3);

  const auto &task = std::get<Task>(s.items[0]);
  CHECK(std::get<bool>(task.value));
  CHECK(task.name == "Verpackung ok");
  CHECK(task.comment == "Karton beschädigt\nzweite Zeile");
  CHECK(task.attachments == std::vector<std::string>{"Y:/attachments/foto.jpg"});
  CHECK(task.tooltip == "Sichtprüfung");
  CHECK(task.isDone());

  const auto &group = std::get<TaskGroup>(s.items[1]);
  CHECK(group.name == "Zubehör");
  REQUIRE(group.tasks.size() == 2);
  CHECK(std::get<int>(group.tasks[0].value) == 2);
  const auto &combo = std::get<Combo>(group.tasks[1].value);
  CHECK(combo.selected == "Ja");
  CHECK(combo.options == std::vector<std::string>{"Ja", "Nein"});
  CHECK_FALSE(group.tasks[1].isDone());

  const auto &table = std::get<TaskTable>(s.items[2]);
  CHECK(table.name == "Preisliste (Rabatt bereits berechnet)");
  REQUIRE(table.templateRow.tasks.size() == 2);
  CHECK(table.templateRow.tasks[1].fmt == "%.2f€");
  REQUIRE(table.rows.size() == 2);
  CHECK(std::get<Text>(table.rows[0].tasks[0].value).value == "Lampe");
  CHECK(std::get<double>(table.rows[1].tasks[1].value) == doctest::Approx(79.25));

  const auto &befund = std::get<Task>(c.sections[1].items[0]);
  CHECK(std::get<TextBox>(befund.value).value == "Zeile 1\nZeile 2");
  CHECK(befund.expression == "@rinfo.serial");
}

TEST_CASE("legacy text: writing reproduces the legacy file byte by byte") {
  const std::string original = test::readText(test::dataFile("legacy_checklist.txt"));
  CHECK(writeLegacyText(parseLegacyText(original)) == original);
}

TEST_CASE("legacy text: CRLF files and unknown lines are accepted") {
  std::string crlf = test::readText(test::dataFile("legacy_checklist.txt"));
  st::util::replaceAll(crlf, "\nSECTION_START", "\nUNKNOWN_KEY=1\nSECTION_START");
  st::util::replaceAll(crlf, "\nCHECKINFO_END", "\nUNKNOWN_TASK_KEY=1\nCHECKINFO_END");
  st::util::replaceAll(crlf, "\n", "\r\n");
  CHECK(parseLegacyText(crlf) == loadFixture());
}

TEST_CASE("legacy text: legacy quirks") {
  SUBCASE("resolve_rinfo becomes auto_finish") {
    const auto c = parseLegacyText("SECTION_START\nNAME=A\nPOST_ACTION=resolve_rinfo\nSECTION_END\n");
    CHECK(c.sections.at(0).postAction == "auto_finish");
  }
  SUBCASE("date values are stored in an extra line") {
    Checklist c;
    Section s;
    s.name = "A";
    Task t;
    t.name = "Datum";
    t.value = Date{"01.02.2026"};
    s.items.emplace_back(t);
    c.sections.push_back(s);
    const auto text = writeLegacyText(c);
    CHECK(text.find("VARIABLE_DATE=01.02.2026") != std::string::npos);
    CHECK(std::get<Date>(std::get<Task>(parseLegacyText(text).sections[0].items[0]).value).value == "01.02.2026");
  }
  SUBCASE("checklist version survives although the legacy header is VERSION=1") {
    Checklist c;
    c.version = "3.2";
    const auto text = writeLegacyText(c);
    CHECK(text.starts_with("VERSION=1\n"));
    CHECK(parseLegacyText(text).version == "3.2");
  }
  SUBCASE("newlines in single line fields do not break the file") {
    Checklist c;
    Section s;
    s.name = "Name\nmit Umbruch";
    c.sections.push_back(s);
    CHECK(parseLegacyText(writeLegacyText(c)).sections.at(0).name == "Name mit Umbruch");
  }
  SUBCASE("garbage numbers fall back to defaults") {
    const auto c = parseLegacyText("SECTION_START\nTIME=abc\nCHECKINFO_START\nVARIABLE_INT=x\nCHECKINFO_END\nSECTION_END\n");
    CHECK(c.sections.at(0).time == 0.0);
    CHECK(std::get<int>(std::get<Task>(c.sections.at(0).items.at(0)).value) == 0);
  }
  SUBCASE("truncated file does not crash") {
    const std::string full = test::readText(test::dataFile("legacy_checklist.txt"));
    for (std::size_t n : {10ul, 200ul, 900ul, 1500ul})
      CHECK_NOTHROW(parseLegacyText(std::string_view(full).substr(0, n)));
  }
}

TEST_CASE("json: round trip keeps everything") {
  Checklist c = loadFixture();
  c.version = "2.0";
  c.permissions = {"TECHSUPPORT"};
  auto &s = c.sections[1];
  Task date;
  date.name = "Datum";
  date.value = Date{"03.04.2026"};
  s.items.emplace_back(date);
  auto parsed = parseJson(writeJson(c, "Y:/x/attachments/", 123));
  REQUIRE(parsed.has_value());
  CHECK(*parsed == c);
}

TEST_CASE("json: files of the legacy program are read") {
  // Shape as written by the legacy SaveChecklistJSON (sorted keys, missing arrays)
  const char *legacy = R"({
    "attachmentsDir": "Y:/x/attachments/",
    "finished": false,
    "lastedit": 133,
    "permissions": [],
    "sections": [
      {
        "finished": true,
        "items": [
          {"data": {"attachments": [], "comment": "", "expression": "", "fmt": "", "name": "Seriennummer",
                    "timestamp": "2026-01-01 10:00:00", "tooltip": "", "type": "string", "user": "a", "value": "123"},
           "kind": "info"},
          {"data": {"name": "Leer", "singleTick": false}, "kind": "line"},
          {"data": {"cfocus": -1, "enteradd": false, "name": "T", "rfocus": -1, "timestamp": "",
                    "templateLine": {"items": [{"name": "c", "type": "int", "value": 0}], "name": "", "singleTick": false}},
           "kind": "table"},
          {"kind": "unknown", "data": {}},
          {"kind": "info"}
        ],
        "name": "Wareneingang",
        "permissions": ["LAGER"],
        "postAction": "resolve_rinfo",
        "postActionArgs": "",
        "postPopup": {"header": "", "text": "", "title": ""},
        "time": 0.5,
        "timestamp": "2026-01-01 10:00:00",
        "user": "a"
      }
    ],
    "timestamp": "",
    "user": "",
    "version": "1.0"
  })";
  auto c = parseJson(legacy);
  REQUIRE(c.has_value());
  CHECK(c->version == "1.0");
  REQUIRE(c->sections.size() == 1);
  const auto &s = c->sections[0];
  CHECK(s.postAction == "auto_finish");
  REQUIRE(s.items.size() == 3);
  CHECK(std::get<Text>(std::get<Task>(s.items[0]).value).value == "123");
  CHECK(std::get<TaskGroup>(s.items[1]).tasks.empty());
  CHECK(std::get<TaskTable>(s.items[2]).templateRow.tasks.size() == 1);
}

TEST_CASE("json: legacy multiline strings become text boxes") {
  auto c = parseJson(R"({"sections":[{"name":"A","items":[{"kind":"info","data":{"type":"string","multiline":true,"value":"a\nb"}}]}]})");
  REQUIRE(c.has_value());
  CHECK(std::get<TextBox>(std::get<Task>(c->sections[0].items[0]).value).value == "a\nb");
}

TEST_CASE("json: invalid content is an error, not an exception") {
  CHECK_FALSE(parseJson("{").has_value());
  CHECK_FALSE(parseJson("[]").has_value());
}

TEST_CASE("format detection by extension") {
  CHECK(formatForPath("a/Checklist.json") == Format::Json);
  CHECK(formatForPath("a/Checklist.JSON") == Format::Json);
  CHECK(formatForPath("a/Checklist.txt") == Format::LegacyText);
}
