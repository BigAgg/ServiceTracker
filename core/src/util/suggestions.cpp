#include "st/util/suggestions.h"

#include <array>
#include <vector>
#include <string>
#include <string_view>

namespace st::text {
/// Removes empty/tabs at the start and end of a string
static std::string_view trim(std::string_view s) {
  const auto first = s.find_first_not_of(" \t");
  if (first == std::string_view::npos) return {};
  const auto last = s.find_last_not_of(" \t");
  return s.substr(first, last - first + 1);
}

/// Check if position 'pos' a new word starts after space or typical spacer
static bool isWordStart(std::string_view s, size_t pos) {
  if (pos == 0) return true;
  const char prev = s[pos - 1];
  return prev == ' ' || prev == '\t' || prev == '-' || prev == '_' || prev == '/' || prev == '.' ||
         prev == ',' || prev == '(' || prev == '&';
}

/// Rank the candidates
static int rank(std::string_view hay, std::string_view needle) {
  size_t pos = hay.find(needle);
  if (pos == std::string_view::npos) return -1;
  if (pos == 0) return 0;
  for (; pos != std::string_view::npos; pos = hay.find(needle, pos + 1)) {
    if (isWordStart(hay, pos)) return 1;
  }
  return 2;
}

std::string foldCase(std::string_view s) {
  std::string out(s);
  for (size_t i = 0; i < out.size(); ++i) {
    const auto c = static_cast<unsigned char>(out[i]);
    if (c >= 'A' && c <= 'Z') {
      out[i] = static_cast<char>(c + ('a' - 'A'));
    } else if (c == 0xC3 && i + 1 < out.size()) {
      const auto n = static_cast<unsigned char>(out[i + 1]);
      if (n == 0x84 || n == 0x96 || n == 0x9C) out[i + 1] = static_cast<char>(n + 0x20);
      ++i;
    }
  }
  return out;
}

std::vector<std::string_view> matchSuggestions(std::string_view query, const std::vector<std::string> &candidates, size_t limit) {
  std::vector<std::string_view> result;
  const std::string needle = foldCase(trim(query));
  if (needle.empty() || limit == 0) return result;

  std::array<std::vector<std::string_view>, 3> tiers;
  for (const auto &candidate : candidates) {
    const int r = rank(foldCase(candidate), needle);
    if (r >= 0) tiers[r].emplace_back(candidate);
  }

  for (const auto &tier : tiers) {
    for (auto sv : tier) {
      if (result.size() >= limit) return result;
      result.push_back(sv);
    }
  }
  return result;
}
} // namespace st::text
