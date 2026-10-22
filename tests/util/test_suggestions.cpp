#include "st/util/suggestions.h"
#include <doctest/doctest.h>

using st::util::foldCase;
using st::util::matchSuggestions;

namespace {
const std::vector<std::string> kCustomers{
    "Elektro Maier",
    "Müller GmbH",
    "Schmidt & Söhne",
    "Hans Müller",
    "Maier-Bau",
    "Kellermann AG",
};

/// Helper to convert string_view in string
std::vector<std::string> toStrings(const std::vector<std::string_view> &v) {
  return {v.begin(), v.end()};
}
} // Namespace
TEST_CASE("foldCase: ASCII und Umlaute") {
  CHECK(foldCase("HeLLo") == "hello");
  CHECK(foldCase("ÄÖÜ äöü ß") == "äöü äöü ß");
  CHECK(foldCase("") == "");
}

TEST_CASE("matchSuggestions: leere Eingabe liefert nichts") {
  CHECK(matchSuggestions("", kCustomers, 8).empty());
  CHECK(matchSuggestions("   ", kCustomers, 8).empty());
}

TEST_CASE("matchSuggestions: case-insensitive inkl. Umlaute") {
  CHECK(toStrings(matchSuggestions("MÜLLER", kCustomers, 8)) == std::vector<std::string>{"Müller GmbH", "Hans Müller"});
}

TEST_CASE("matchSuggestions: Rangfolge Präfix > Wortanfang > enthält") {
  // "Maier-Bau" = Präfix, "Elektro Maier" = Wortanfang
  CHECK(toStrings(matchSuggestions("maier", kCustomers, 8)) == std::vector<std::string>{"Maier-Bau", "Elektro Maier"});
  // wortanfang nach "-"
  CHECK(toStrings(matchSuggestions("bau", kCustomers, 8)) == std::vector<std::string>{"Maier-Bau"});
  // ller: nur enthält
  CHECK(toStrings(matchSuggestions("ller", kCustomers, 8)) == std::vector<std::string>{"Müller GmbH", "Hans Müller", "Kellermann AG"});
}

TEST_CASE("matchSuggestions: Wortanfang") {
  CHECK(toStrings(matchSuggestions("söhne", kCustomers, 8)) == std::vector<std::string>{"Schmidt & Söhne"});
}

TEST_CASE("matchSuggestions: Eingabe wird getrimmt") {
  CHECK(toStrings(matchSuggestions("  kellermann ", kCustomers, 8)) == std::vector<std::string>{"Kellermann AG"});
}
TEST_CASE("matchSuggestions: Limit wird eingehalten, Präfix-Treffer zuerst") {
  const auto r = toStrings(matchSuggestions("m", kCustomers, 2));
  REQUIRE(r.size() == 2);
  CHECK(r[0] == "Müller GmbH");
  CHECK(r[1] == "Maier-Bau");
  CHECK(matchSuggestions("m", kCustomers, 0).empty());
}
TEST_CASE("matchSuggestions: kein Treffer") {
  CHECK(matchSuggestions("xyz", kCustomers, 8).empty());
}
