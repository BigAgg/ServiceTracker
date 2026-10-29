#include "test_helpers.h"

#include "st/exporting/mail.h"
#include "st/exporting/record_export.h"

#include <doctest/doctest.h>

#include <thread>

using namespace st;
using namespace st::exporting;

namespace {

checklist::Checklist recordChecklist() {
  checklist::Checklist c;
  checklist::Section s;
  s.name = "Wareneingang";
  s.finished = true;
  auto task = checklist::Task::make("Bemerkung", checklist::Text{"Kratzer"});
  task.timestamp = "2026-03-15 10:00:00";
  s.items.emplace_back(task);
  c.sections.push_back(s);
  return c;
}

class Dictionary final : public translate::Backend {
public:
  bool fail = false;
  util::Result<std::vector<std::string>> translate(const std::vector<std::string> &texts) override {
    if (fail)
      return std::unexpected(util::Error{"offline"});
    std::vector<std::string> out;
    for (const auto &t : texts)
      out.push_back("EN " + t);
    return out;
  }
};

} // namespace

TEST_CASE("record: browser command line") {
  const auto args = browserPrintArguments("C:/Temp/a b/R.html", "C:/Temp/a b/R.pdf");
  CHECK(std::find(args.begin(), args.end(), "--headless") != args.end());
  CHECK(std::find(args.begin(), args.end(), "--no-pdf-header-footer") != args.end());
  CHECK(args.back() == "file:///C:/Temp/a%20b/R.html");
  CHECK(std::any_of(args.begin(), args.end(), [](const std::string &a) { return a.starts_with("--print-to-pdf="); }));
  CHECK(std::none_of(args.begin(), args.end(), [](const std::string &a) { return a.starts_with("--user-data-dir="); }));

  CHECK(fileUrl(util::pathFromUtf8("/tmp/Ger\xC3\xA4t.html")) == "file:///tmp/Ger%C3%A4t.html"); // "Gerät", bytes explicit for MSVC
  CHECK(fileUrl("C:\\x\\y.html") == "file:///C:/x/y.html");
  CHECK(fileUrl("\\\\server\\share\\y.html") == "file://server/share/y.html");

  CHECK(quoteWindowsArgument("simple") == "simple");
  CHECK(quoteWindowsArgument("") == "\"\"");
  CHECK(quoteWindowsArgument("a b") == "\"a b\"");
  CHECK(quoteWindowsArgument("C:\\a b\\") == "\"C:\\a b\\\\\"");
  CHECK(quoteWindowsArgument("say \"hi\"") == "\"say \\\"hi\\\"\"");

  const auto candidates = browserCandidates({"C:/Program Files (x86)", "", "C:/Program Files"});
  REQUIRE(candidates.size() == 4);
  CHECK(util::pathToUtf8(candidates[0].filename()) == "msedge.exe");
  CHECK(util::pathToUtf8(candidates[1].filename()) == "msedge.exe");
  CHECK(util::pathToUtf8(candidates[2].filename()) == "chrome.exe");
}

TEST_CASE("record: pdf, translation and fallbacks") {
  test::TempDir dir;
  RecordRequest request;
  request.checklist = recordChecklist();
  request.outputBase = dir / "0601-1234_ServiceRecord_EN";

  std::string printedHtml;
  const PdfPrinter printer = [&](const util::fs::path &html, const util::fs::path &pdf) -> util::Status {
    printedHtml = test::readText(html);
    test::writeText(pdf, "%PDF-1.7");
    return {};
  };

  SUBCASE("translated pdf, html is removed") {
    translate::TranslationCache cache(dir / "t.json");
    Dictionary dictionary;
    translate::Translator translator(cache, &dictionary);
    request.translate = true;
    auto result = createRecord(request, &translator, printer);
    REQUIRE(result.has_value());
    CHECK(result->isPdf);
    CHECK(result->warnings.empty());
    CHECK(result->translatedLines == 3);
    CHECK(util::pathToUtf8(result->file.filename()) == "0601-1234_ServiceRecord_EN.pdf");
    CHECK_FALSE(util::pathExists(dir / "0601-1234_ServiceRecord_EN.html"));
    CHECK(printedHtml.find("EN Kratzer") != std::string::npos);
    CHECK(printedHtml.find("counter(page)") != std::string::npos);
    CHECK(printedHtml.find("0601-1234_ServiceRecord_EN.pdf") != std::string::npos);
  }
  SUBCASE("failing translation stops the export") {
    translate::TranslationCache cache(dir / "t.json");
    Dictionary dictionary;
    dictionary.fail = true;
    translate::Translator translator(cache, &dictionary);
    request.translate = true;
    CHECK_FALSE(createRecord(request, &translator, printer).has_value());
    CHECK_FALSE(createRecord(request, nullptr, printer).has_value());
  }
  SUBCASE("failing pdf falls back to html") {
    auto result = createRecord(request, nullptr, [](const auto &, const auto &) -> util::Status {
      return std::unexpected(util::Error{"Edge nicht gefunden"});
    });
    REQUIRE(result.has_value());
    CHECK_FALSE(result->isPdf);
    REQUIRE(result->warnings.size() == 1);
    CHECK(result->warnings[0].find("Edge nicht gefunden") != std::string::npos);
    CHECK(util::pathExists(result->file));
  }
  SUBCASE("printer that writes nothing") {
    auto result = createRecord(request, nullptr, [](const auto &, const auto &) -> util::Status { return {}; });
    REQUIRE(result.has_value());
    CHECK_FALSE(result->isPdf);
    CHECK(result->warnings.size() == 1);
  }
  SUBCASE("html only") {
    request.pdf = false;
    auto result = createRecord(request, nullptr, printer);
    REQUIRE(result.has_value());
    CHECK(util::pathToUtf8(result->file.extension()) == ".html");
    CHECK(result->warnings.empty());
    CHECK(printedHtml.empty());
  }
}

TEST_CASE("mail: template, placeholders and mailto") {
  MailTemplate mail;
  mail.to = " service@example.com ";
  mail.cc = "{user}@example.com";
  mail.subject = "Report {reader} {unknown}";
  mail.body = "Service {service}\nvom {date}";
  const auto draft = buildMailDraft(mail, {{"reader", "0601-1234"}, {"user", "ajahraus"}, {"service", "Service_2026.10.29"}, {"date", "2026.10.29"}});
  CHECK(draft.to == "service@example.com");
  CHECK(draft.cc == "ajahraus@example.com");
  CHECK(draft.subject == "Report 0601-1234 {unknown}");
  CHECK(draft.body == "Service Service_2026.10.29\nvom 2026.10.29");

  const auto url = mailtoUrl(draft);
  CHECK(url.starts_with("mailto:service%40example.com?cc=ajahraus%40example.com&subject=Report%200601-1234%20%7Bunknown%7D&body="));
  CHECK(url.find("%0D%0A") != std::string::npos);
  CHECK(mailtoUrl(MailDraft{}) == "mailto:");

  const auto defaults = defaultMailTemplate();
  CHECK(defaults.subject.find("{reader}") != std::string::npos);
  for (const auto &[key, description] : mailPlaceholders())
    CHECK((key.front() == '{' && key.back() == '}' && !description.empty()));
}

TEST_CASE("mail: reader evaluation in EV-Plate") {
  test::TempDir dir;
  const auto service = dir.path() / "Service_2026.10.29";
  CHECK_FALSE(findReaderEvaluation(service).has_value());
  test::writeText(service / "EV-Plate" / "notes.pdf", "x");
  test::writeText(service / "EV-Plate" / "0601-1234 Reader Evaluation.txt", "x");
  CHECK_FALSE(findReaderEvaluation(service).has_value());
  test::writeText(service / "EV-Plate" / "0601-1234 reader evaluation alt.PDF", "x");
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  test::writeText(service / "EV-Plate" / "0601-1234 Reader Evaluation.pdf", "x");
  const auto found = findReaderEvaluation(service);
  REQUIRE(found.has_value());
  CHECK(util::pathToUtf8(found->filename()) == "0601-1234 Reader Evaluation.pdf");
}

TEST_CASE("mail: template store") {
  test::TempDir dir;
  const auto file = dir / "mailTemplate.json";
  MailTemplateStore store(file);
  REQUIRE(store.load().has_value());
  CHECK(store.mail() == defaultMailTemplate());

  test::writeText(file, R"({"to": "a@b.c", "future": 1})");
  REQUIRE(store.load().has_value());
  CHECK(store.mail().to == "a@b.c");
  CHECK(store.mail().subject == defaultMailTemplate().subject);

  MailTemplate changed = store.mail();
  changed.subject = "S";
  REQUIRE(store.save(changed).has_value());
  CHECK_FALSE(store.changedOnDisk());
  MailTemplateStore other(file);
  REQUIRE(other.load().has_value());
  CHECK(other.mail() == changed);
  CHECK(test::readText(file).find("future") != std::string::npos);

  test::writeText(file, "[]");
  CHECK_FALSE(other.load().has_value());
}
