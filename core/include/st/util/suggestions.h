#pragma once

#include <string_view>
#include <string>
#include <vector>

namespace st::text {
/// Converting UTF-8-String in relative lowercase
/// Handles ASCII aswell as the german characters (Ä/Ö/Ü -> ä/ö/ü)
std::string foldCase(std::string_view s);

// core: matches filter - prefix-matches first then "contains", case-insensitive, max. 'limit'
std::vector<std::string_view> matchSuggestions(std::string_view query, const std::vector<std::string> &candidates, size_t limit);


};
