#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// String helpers shared by all modules. All strings are UTF-8 encoded.
namespace st::util {

/// Splits `input` at the first occurrence of `delimiter`.
/// Returns {input, ""} when the delimiter is not found (same semantics as the
/// legacy `split_at` the old file formats rely on).
std::pair<std::string, std::string> splitAt(std::string_view input, std::string_view delimiter);

/// Splits `input` at every `delimiter`. Empty parts are kept unless `skipEmpty` is set.
std::vector<std::string> split(std::string_view input, char delimiter, bool skipEmpty = false);

/// Splits text into lines. Handles "\n" and "\r\n"; a trailing newline does not
/// produce an empty last line.
std::vector<std::string> splitLines(std::string_view text);

/// Joins `parts` with `separator`.
std::string join(const std::vector<std::string> &parts, std::string_view separator);

/// Replaces every occurrence of `from` with `to` in place.
void replaceAll(std::string &str, std::string_view from, std::string_view to);

/// Returns a copy of `str` with every occurrence of `from` replaced by `to`.
std::string replaced(std::string str, std::string_view from, std::string_view to);

/// ASCII lower case conversion (UTF-8 multibyte sequences are kept as they are).
std::string toLower(std::string_view str);

/// Removes leading and trailing whitespace.
std::string trim(std::string_view str);

/// Case insensitive (ASCII) substring search. An empty needle always matches.
bool containsIgnoreCase(std::string_view haystack, std::string_view needle);

/// Replaces line breaks with a single space so the value fits into a `KEY=value` line.
std::string singleLine(std::string_view str);

/// Locale independent number parsing. Leading/trailing whitespace is ignored,
/// any other trailing garbage makes the parse fail.
std::optional<int> parseInt(std::string_view str);
std::optional<long long> parseInt64(std::string_view str);
std::optional<unsigned long long> parseUInt64(std::string_view str);
std::optional<double> parseDouble(std::string_view str);

/// Formats a double the way `std::ostream << double` does with the classic locale
/// (6 significant digits, no trailing zeros). Used by the legacy text format.
std::string formatDoubleLegacy(double value);

/// Escapes the five HTML special characters.
std::string htmlEscape(std::string_view str);

} // namespace st::util
