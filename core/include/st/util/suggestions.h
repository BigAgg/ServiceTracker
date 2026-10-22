#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

/// Suggestions for input fields with autocompletion (e.g. customers in "Bestellungen").
namespace st::util {

/// Lower case for comparisons: ASCII and the German umlauts (Ä/Ö/Ü -> ä/ö/ü) of a UTF-8 string.
std::string foldCase(std::string_view s);

/// Candidates matching `query` (case-insensitive), best first: prefix matches, then
/// matches at a word start, then matches anywhere. At most `limit` results; the views
/// point into `candidates`.
std::vector<std::string_view> matchSuggestions(std::string_view query, const std::vector<std::string> &candidates, std::size_t limit);

} // namespace st::util
