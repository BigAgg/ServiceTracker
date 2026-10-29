#include <doctest/doctest.h>

#include "st/checklist/actions.h"
#include "st/checklist/export.h"
#include "st/checklist/formats.h"
#include "test_helpers.h"

using namespace st;
using namespace st::checklist;

namespace {
Checklist fixture() { return parseLegacyText(test::readText(test::dataFile("legacy_checklist.txt"))); }
} // namespace

TEST_CASE("expressions fill tasks and stamp them") {
  test::TestSession ts;
  ExpressionRegistry expressions;
  expressions.add("@rinfo.serial", [](Task &t) { t.value = Text{"1234"}; }, "Seriennummer");
  CHECK(expressions.contains("@rinfo.serial"));
  CHECK(expressions.description("@rinfo.serial") == "Seriennummer");
  CHECK(expressions.names() == std::vector<std::string>{"@rinfo.serial"});

  Checklist c = fixture();
  Section &technik = c.sections[1];
  CHECK(resolveExpressions(technik, expressions, ts.session) == 1);
  const Task &befund = std::get<Task>(technik.items[0]);
  CHECK(std::get<Text>(befund.value).value == "1234");
  CHECK(befund.user == "tester");
  CHECK(befund.timestamp == "2026-03-15 10:00:00");

  Task unknown;
  unknown.expression = "@nope";
  CHECK_FALSE(expressions.resolve("@nope", unknown));
}

TEST_CASE("finishing a section reports open tasks and runs the post action") {
  test::TestSession ts;
  ExpressionRegistry expressions;
  SectionActionRegistry actions;
  registerBuiltinActions(actions, expressions);
  std::string ranWith;
  actions.add("update_status_rinfo", [&](SectionActionContext &ctx) { ranWith = ctx.section.postActionArgs; });

  Checklist c = fixture();
  Section &s = c.sections[0];
  s.finished = false;
  const auto open = prepareSectionFinish(s, expressions, ts.session);
  CHECK(open == std::vector<std::string>{"Handbuch"});

  finishSection(c, s, actions, ts.session);
  CHECK(s.finished);
  CHECK(s.user == "tester");
  CHECK(ranWith == "Technik");

  reopenSection(s, ts.session);
  CHECK_FALSE(s.finished);
  CHECK_FALSE(s.timestamp.empty());
}

TEST_CASE("finishing the checklist finishes auto_finish sections") {
  test::TestSession ts;
  ExpressionRegistry expressions;
  expressions.add("@rinfo.serial", [](Task &t) { t.value = TextBox{"auto"}; });
  SectionActionRegistry actions;
  registerBuiltinActions(actions, expressions);

  Checklist c = fixture();
  finishChecklist(c, expressions, actions, ts.session);
  CHECK(c.finished);
  CHECK(c.user == "tester");
  CHECK(c.sections[1].finished);
  CHECK(std::get<TextBox>(std::get<Task>(c.sections[1].items[0]).value).value == "auto");
  // sections without auto_finish are not touched
  CHECK(c.sections[0].user == "lager");
}

TEST_CASE("number formatting uses German notation") {
  CHECK(formatNumber(1234.5, "%.2f€") == "1.234,50€");
  CHECK(formatNumber(0.0, "") == "0,00");
  CHECK(formatNumber(-1.005, "%.1f") == "-1,0");
  CHECK(formatNumber(-0.001, "%.2f") == "0,00");
  CHECK(formatNumber(1234567.0, "%.0f h") == "1.234.567 h");
  CHECK(formatInteger(1234567) == "1.234.567");
  CHECK(formatInteger(-1000) == "-1.000");
  CHECK(formatInteger(999) == "999");
}

TEST_CASE("value access") {
  const Checklist c = fixture();
  CHECK(retrieveValue(c, "Wareneingang", "Kabel") == "2");
  CHECK(retrieveValue(c, "Wareneingang", "Handbuch") == "Ja");
  CHECK(retrieveValue(c, "Wareneingang", "Artikel") == "Lampe\nFilter");
  CHECK(retrieveValue(c, "Wareneingang", "Verpackung ok") == "1");
  CHECK(retrieveValue(c, "Gibt es nicht", "Kabel").empty());
  CHECK(tableColumnTotal(c, "Wareneingang", "Preisliste (Rabatt bereits berechnet)", "Preis") == doctest::Approx(199.75));
  CHECK(tableColumnTotal(c, "Wareneingang", "Andere", "Preis") == 0.0);
}

TEST_CASE("hours are summed in h.mm notation") {
  CHECK(normalizeHours(1.75) == doctest::Approx(2.15));
  CHECK(normalizeHours(0.3) == doctest::Approx(0.3));
  Checklist c;
  c.sections.resize(2);
  c.sections[0].time = 1.45;
  c.sections[1].time = 0.30;
  CHECK(totalHours(c) == doctest::Approx(2.15));
}

TEST_CASE("html export contains finished sections only and escapes content") {
  Checklist c = fixture();
  std::get<Task>(c.sections[0].items[0]).name = "<b>Verpackung</b>";
  HtmlOptions options;
  options.documentName = "0601-1234_SR.html";
  options.appVersion = "2.0.0";
  const std::string html = exportHtml(c, options);
  CHECK(html.find("<!DOCTYPE html>") == 0);
  CHECK(html.find("Wareneingang") != std::string::npos);
  CHECK(html.find("Technik</div>") == std::string::npos); // not finished
  CHECK(html.find("&lt;b&gt;Verpackung&lt;/b&gt;") != std::string::npos);
  CHECK(html.find("120,50€") != std::string::npos);
  CHECK(html.find("Handbuch") == std::string::npos); // task without timestamp
  CHECK(html.find("0601-1234_SR.html") != std::string::npos);

  options.customHead = "<html><body><h1>EIGEN</h1>";
  options.includeUnfinished = true;
  const std::string custom = exportHtml(c, options);
  CHECK(custom.starts_with("<html><body><h1>EIGEN</h1>"));
  CHECK(custom.find("Technik</div>") != std::string::npos);
}

TEST_CASE("csv export") {
  const std::string csv = exportCsv(fixture(), "Y:/x/Checklist.txt");
  CHECK(csv.starts_with("\xEF\xBB\xBFPath:;Y:/x/Checklist.txt;"));
  CHECK(csv.find("Section:;Wareneingang;") != std::string::npos);
  CHECK(csv.find("Lampe;120.5;") != std::string::npos);
}

TEST_CASE("record subset keeps the chosen order") {
  const Checklist c = fixture();
  const Checklist r = recordSubset(c, {"Technik", "Wareneingang", "Fehlt"});
  REQUIRE(r.sections.size() == 2);
  CHECK(r.sections[0].name == "Technik");
  CHECK(r.sections[0].finished);
}

TEST_CASE("table rows follow template changes") {
  TaskTable t;
  t.templateRow.tasks = {Task::make("Teil", Text{}), Task::make("Preis", 0.0)};
  TaskGroup row = t.templateRow;
  std::get<Text>(row.tasks[0].value).value = "Lampe";
  row.tasks[1].value = 5.0;
  t.rows = {row};

  // insert a column at the front and rename nothing: values stay with their column
  t.templateRow.tasks.insert(t.templateRow.tasks.begin(), Task::make("Menge", 0));
  CHECK(alignTableRows(t));
  REQUIRE(t.rows[0].tasks.size() == 3);
  CHECK(std::get<int>(t.rows[0].tasks[0].value) == 0);
  CHECK(std::get<Text>(t.rows[0].tasks[1].value).value == "Lampe");
  CHECK(std::get<double>(t.rows[0].tasks[2].value) == 5.0);
  CHECK_FALSE(alignTableRows(t));

  // changing the type of a column resets the cell
  t.templateRow.tasks[2].value = Text{};
  CHECK(alignTableRows(t));
  CHECK(std::holds_alternative<Text>(t.rows[0].tasks[2].value));

  // combo options are taken from the template, invalid selections cleared
  t.templateRow.tasks.push_back(Task::make("Wahl", Combo{{"a", "b"}, ""}));
  alignTableRows(t);
  std::get<Combo>(t.rows[0].tasks[3].value).selected = "b";
  t.templateRow.tasks[3].value = Combo{{"a"}, ""};
  alignTableRows(t);
  CHECK(std::get<Combo>(t.rows[0].tasks[3].value).options == std::vector<std::string>{"a"});
  CHECK(std::get<Combo>(t.rows[0].tasks[3].value).selected.empty());
}

TEST_CASE("html export for customers hides working times and names") {
  Checklist c = fixture();
  c.user = "ajahraus";
  c.timestamp = "2026-03-15 10:00:00";
  c.sections[0].user = "mmuster";
  c.sections[0].timestamp = "2026-03-14 09:30:00";
  HtmlOptions options;
  options.documentName = "R.pdf";
  const std::string normal = exportHtml(c, options);
  CHECK(normal.find("mmuster") != std::string::npos);
  CHECK(normal.find("Total Time") != std::string::npos);
  CHECK(normal.find("@bottom-right") == std::string::npos);

  options.hideWorkInfo = true;
  options.pageHeaderFooter = true;
  const std::string html = exportHtml(c, options);
  CHECK(html.find("mmuster") == std::string::npos);
  CHECK(html.find("ajahraus") == std::string::npos);
  CHECK(html.find("Time:") == std::string::npos);
  CHECK(html.find("Total Time") == std::string::npos);
  CHECK(html.find("09:30") == std::string::npos);
  CHECK(html.find("2026-03-14") != std::string::npos); // the date stays
  CHECK(html.find("counter(pages)") != std::string::npos);
  CHECK(html.find("@top-left { content: \"R.pdf\"") != std::string::npos);
}

TEST_CASE("css strings are escaped") {
  CHECK(cssString("a\"b\\c") == "\"a\\\"b\\\\c\"");
  CHECK(cssString("x\ny") == "\"x\\A y\"");
  CHECK(cssString("</style>") == "\"\\3C /style>\"");
}

TEST_CASE("e-mail subset: finished sections that are allowed, in checklist order") {
  Checklist c = fixture();
  for (auto &s : c.sections)
    s.finished = true;
  CHECK(emailSubset(c).sections.size() == c.sections.size());
  c.sections[0].emailExport = false;
  const Checklist mail = emailSubset(c);
  REQUIRE(mail.sections.size() == c.sections.size() - 1);
  CHECK(mail.sections[0].name == c.sections[1].name);
  c.sections[1].finished = false;
  CHECK(emailSubset(c).sections.size() == c.sections.size() - 2);
}
