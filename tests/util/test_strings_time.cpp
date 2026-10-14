#include <doctest/doctest.h>

#include "st/util/strings.h"
#include "st/util/time.h"

using namespace st::util;

TEST_CASE("splitAt splits at the first delimiter like the legacy split_at") {
  CHECK(splitAt("NAME=a=b", "=") == std::pair<std::string, std::string>{"NAME", "a=b"});
  CHECK(splitAt("nodelim", "=") == std::pair<std::string, std::string>{"nodelim", ""});
  CHECK(splitAt("", "=") == std::pair<std::string, std::string>{"", ""});
  CHECK(splitAt("0601-1234", "-").first == "0601");
}

TEST_CASE("split keeps or skips empty parts") {
  CHECK(split("a,,b", ',') == std::vector<std::string>{"a", "", "b"});
  CHECK(split("a,,b", ',', true) == std::vector<std::string>{"a", "b"});
  CHECK(split("", ',', true).empty());
}

TEST_CASE("splitLines handles CRLF and trailing newline") {
  CHECK(splitLines("a\r\nb\nc\n") == std::vector<std::string>{"a", "b", "c"});
  CHECK(splitLines("a\n\nb") == std::vector<std::string>{"a", "", "b"});
  CHECK(splitLines("").empty());
}

TEST_CASE("number parsing is strict and locale independent") {
  CHECK(parseInt(" 42 ") == 42);
  CHECK(parseInt("-7") == -7);
  CHECK_FALSE(parseInt("4x").has_value());
  CHECK_FALSE(parseInt("").has_value());
  CHECK(parseDouble("1.5") == doctest::Approx(1.5));
  CHECK(parseDouble("1,5") == doctest::Approx(1.5));
  CHECK_FALSE(parseDouble("abc").has_value());
  CHECK(parseUInt64("18446744073709551615") == 18446744073709551615ULL);
}

TEST_CASE("misc string helpers") {
  CHECK(toLower("AbC") == "abc");
  CHECK(trim("  x y \t") == "x y");
  CHECK(containsIgnoreCase("Reparatur Gerät", "REPARATUR"));
  CHECK(containsIgnoreCase("abc", ""));
  CHECK(singleLine("a\nb\r\nc") == "a b c");
  CHECK(replaced("{reader}_x_{reader}", "{reader}", "0601-1") == "0601-1_x_0601-1");
  CHECK(htmlEscape("<a & 'b'>") == "&lt;a &amp; &#39;b&#39;&gt;");
  CHECK(formatDoubleLegacy(1.5) == "1.5");
  CHECK(formatDoubleLegacy(0.0) == "0");
}

TEST_CASE("timestamps and dates use the legacy formats") {
  const auto tp = makeLocalTime(2026, 3, 5, 7, 8, 9);
  CHECK(formatTimestamp(tp) == "2026-03-05 07:08:09");
  CHECK(formatDate(tp) == "2026.03.05");
  CHECK(toGermanTimestamp("2026-03-05 07:08:09") == "05.03.2026 07:08:09");
  CHECK(toGermanDate("2026.03.05") == "05.03.2026");
  CHECK(toGermanTimestamp("garbage") == "garbage");
}

TEST_CASE("daysBetween accepts dot and dash dates") {
  CHECK(daysBetween("2026.01.30", "2026.02.02") == 3);
  CHECK(daysBetween("2026-02-02", "2026.01.30") == -3);
  CHECK(daysBetween("2024.02.28", "2024.03.01") == 2); // leap year
  CHECK_FALSE(daysBetween("2026.13.01", "2026.01.01").has_value());
  CHECK_FALSE(daysBetween("", "2026.01.01").has_value());
}
