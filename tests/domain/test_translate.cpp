#include "test_helpers.h"

#include "st/checklist/translation.h"
#include "st/translate/libretranslate.h"
#include "st/translate/translator.h"
#include "st/util/json.h"

#include <doctest/doctest.h>

#include <map>

using namespace st;
using namespace st::translate;

namespace {

/// Backend that "translates" by a fixed dictionary and records every request.
class FakeBackend final : public Backend {
public:
  std::map<std::string, std::string> dictionary;
  std::vector<std::vector<std::string>> requests;
  bool fail = false;

  util::Result<std::vector<std::string>> translate(const std::vector<std::string> &texts) override {
    requests.push_back(texts);
    if (fail)
      return std::unexpected(util::Error{"offline"});
    std::vector<std::string> out;
    for (const auto &t : texts)
      out.push_back(dictionary.contains(t) ? dictionary.at(t) : "EN(" + t + ")");
    return out;
  }
};

} // namespace

TEST_CASE("translator: lines to translate") {
  CHECK(needsTranslation("Gerät gereinigt"));
  CHECK(needsTranslation("Ölwechsel"));
  CHECK_FALSE(needsTranslation(""));
  CHECK_FALSE(needsTranslation("12.03.2026"));
  CHECK_FALSE(needsTranslation("1.234,50 €"));
  CHECK_FALSE(needsTranslation("✅"));
}

TEST_CASE("translator: cache first, backend for the rest, results are cached") {
  test::TempDir dir;
  const auto file = dir / "translations.json";
  test::writeText(file, R"js({"Gerät gereinigt": "Device cleaned (manuell)"})js");
  TranslationCache cache(file);
  REQUIRE(cache.load().has_value());
  FakeBackend backend;
  backend.dictionary = {{"Lampe getauscht", "Lamp replaced"}, {"Gerät gereinigt", "WRONG"}};
  Translator translator(cache, &backend);

  auto result = translator.translate({"Gerät gereinigt", "  Lampe getauscht", "4711", "Lampe getauscht"});
  REQUIRE(result.has_value());
  CHECK(*result == std::vector<std::string>{"Device cleaned (manuell)", "  Lamp replaced", "4711", "Lamp replaced"});
  // only the unknown line is sent, once
  REQUIRE(backend.requests.size() == 1);
  CHECK(backend.requests[0] == std::vector<std::string>{"Lampe getauscht"});
  CHECK(translator.lastBackendLines() == 1);

  // the new line is in the shared file, the manual entry unchanged
  TranslationCache other(file);
  REQUIRE(other.load().has_value());
  CHECK(other.find("Lampe getauscht") == "Lamp replaced");
  CHECK(other.find("Gerät gereinigt") == "Device cleaned (manuell)");

  // second run needs no backend at all
  Translator offline(other, nullptr);
  auto again = offline.translate({"Lampe getauscht"});
  REQUIRE(again.has_value());
  CHECK(again->at(0) == "Lamp replaced");
}

TEST_CASE("translator: multi line texts are translated line by line") {
  test::TempDir dir;
  TranslationCache cache(dir / "t.json");
  FakeBackend backend;
  Translator translator(cache, &backend);
  auto result = translator.translate({"Erste Zeile\n\n  - Punkt\n123"});
  REQUIRE(result.has_value());
  CHECK(result->at(0) == "EN(Erste Zeile)\n\n  EN(- Punkt)\n123");
}

TEST_CASE("translator: batches and errors") {
  test::TempDir dir;
  TranslationCache cache(dir / "t.json");
  FakeBackend backend;
  SUBCASE("large requests are split") {
    Translator translator(cache, &backend, 2);
    REQUIRE(translator.translate({"a", "b", "c", "d", "e"}).has_value());
    CHECK(backend.requests.size() == 3);
  }
  SUBCASE("backend error is reported, nothing is cached") {
    backend.fail = true;
    Translator translator(cache, &backend);
    CHECK_FALSE(translator.translate({"Hallo"}).has_value());
    CHECK(cache.size() == 0);
  }
  SUBCASE("without backend unknown lines are an error") {
    Translator translator(cache, nullptr);
    CHECK_FALSE(translator.translate({"Hallo"}).has_value());
    CHECK(translator.translate({"123"}).has_value());
  }
}

TEST_CASE("translation cache: entries of other users are kept") {
  test::TempDir dir;
  const auto file = dir / "t.json";
  TranslationCache a(file), b(file);
  REQUIRE(a.load().has_value());
  REQUIRE(b.load().has_value());
  REQUIRE(a.add({{"Eins", "One"}}).has_value());
  REQUIRE(b.add({{"Zwei", "Two"}, {"Eins", "Uno"}}).has_value());
  TranslationCache c(file);
  REQUIRE(c.load().has_value());
  CHECK(c.find("Eins") == "One");
  CHECK(c.find("Zwei") == "Two");
  CHECK(c.size() == 2);
  SUBCASE("invalid file is an error") {
    test::writeText(file, "[1]");
    CHECK_FALSE(c.load().has_value());
  }
}

TEST_CASE("libretranslate: protocol") {
  LibreTranslateConfig config;
  config.url = "http://srv:5000/";
  CHECK(translateEndpoint(config.url) == "http://srv:5000/translate");

  auto body = util::json::parse(buildTranslateRequest({"Hallo", "Welt"}, config));
  REQUIRE(body.has_value());
  CHECK((*body)["q"].size() == 2);
  CHECK((*body)["source"] == "de");
  CHECK((*body)["target"] == "en");
  CHECK_FALSE(body->contains("api_key"));
  config.apiKey = "k";
  CHECK(util::json::parse(buildTranslateRequest({"x"}, config))->at("api_key") == "k");

  auto ok = parseTranslateResponse({200, R"({"translatedText": ["Hello", "World"]})"}, 2);
  REQUIRE(ok.has_value());
  CHECK(ok->at(1) == "World");
  CHECK(parseTranslateResponse({200, R"({"translatedText": "Hello"})"}, 1).has_value());
  CHECK_FALSE(parseTranslateResponse({200, R"({"translatedText": ["Hello"]})"}, 2).has_value());
  auto error = parseTranslateResponse({400, R"({"error": "de is not supported"})"}, 1);
  REQUIRE_FALSE(error.has_value());
  CHECK(error.error().message.find("de is not supported") != std::string::npos);
  CHECK_FALSE(parseTranslateResponse({200, "<html>"}, 1).has_value());

  std::string sentTo;
  LibreTranslateBackend backend(config, [&](const std::string &url, const std::string &) -> util::Result<HttpResponse> {
    sentTo = url;
    return HttpResponse{200, R"({"translatedText": ["A"]})"};
  });
  auto translated = backend.translate({"x"});
  REQUIRE(translated.has_value());
  CHECK(sentTo == "http://srv:5000/translate");
  LibreTranslateBackend unreachable(config, [](const std::string &, const std::string &) -> util::Result<HttpResponse> {
    return std::unexpected(util::Error{"timeout"});
  });
  CHECK_FALSE(unreachable.translate({"x"}).has_value());
}

TEST_CASE("checklist translation covers names and text values only") {
  using namespace st::checklist;
  Checklist c;
  c.user = "ajahraus";
  Section s;
  s.name = "Wareneingang";
  s.user = "ajahraus";
  s.items.emplace_back(Task::make("Bemerkung", Text{"Kratzer"}));
  s.items.emplace_back(Task::make("Anzahl", 3));
  s.items.emplace_back(Task::make("Zustand", Combo{{"gut", "schlecht"}, "gut"}));
  TaskTable table;
  table.name = "Teile";
  table.templateRow.tasks = {Task::make("Teil", Text{})};
  table.rows.push_back(TaskGroup{false, "", {Task::make("Teil", Text{"Lampe"})}});
  s.items.emplace_back(table);
  c.sections.push_back(s);

  test::TempDir dir;
  TranslationCache cache(dir / "t.json");
  FakeBackend backend;
  Translator translator(cache, &backend);
  REQUIRE(translateChecklist(c, translator).has_value());
  const auto &out = c.sections[0];
  CHECK(out.name == "EN(Wareneingang)");
  CHECK(out.user == "ajahraus");
  CHECK(std::get<Text>(std::get<Task>(out.items[0]).value).value == "EN(Kratzer)");
  CHECK(std::get<int>(std::get<Task>(out.items[1]).value) == 3);
  CHECK(std::get<Combo>(std::get<Task>(out.items[2]).value).selected == "EN(gut)");
  const auto &t = std::get<TaskTable>(out.items[3]);
  CHECK(t.name == "EN(Teile)");
  CHECK(t.templateRow.tasks[0].name == "EN(Teil)");
  CHECK(t.rows[0].tasks[0].name == "EN(Teil)");
  CHECK(std::get<Text>(t.rows[0].tasks[0].value).value == "EN(Lampe)");

  backend.fail = true;
  Checklist copy = c;
  copy.sections[0].name = "Neu";
  const Checklist before = copy;
  CHECK_FALSE(translateChecklist(copy, translator).has_value());
  CHECK(copy == before);
}
