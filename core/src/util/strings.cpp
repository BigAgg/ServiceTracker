#include "st/util/strings.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <locale>
#include <sstream>

namespace st::util {

std::pair<std::string, std::string> splitAt(std::string_view input, std::string_view delimiter) {
  const auto pos = input.find(delimiter);
  if (pos == std::string_view::npos || delimiter.empty())
    return {std::string(input), std::string()};
  return {std::string(input.substr(0, pos)), std::string(input.substr(pos + delimiter.size()))};
}

std::vector<std::string> split(std::string_view input, char delimiter, bool skipEmpty) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (true) {
    const auto pos = input.find(delimiter, start);
    const auto part = input.substr(start, pos == std::string_view::npos ? std::string_view::npos : pos - start);
    if (!skipEmpty || !part.empty())
      out.emplace_back(part);
    if (pos == std::string_view::npos)
      break;
    start = pos + 1;
  }
  return out;
}

std::vector<std::string> splitLines(std::string_view text) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start < text.size()) {
    auto pos = text.find('\n', start);
    if (pos == std::string_view::npos)
      pos = text.size();
    auto line = text.substr(start, pos - start);
    if (!line.empty() && line.back() == '\r')
      line.remove_suffix(1);
    lines.emplace_back(line);
    start = pos + 1;
  }
  return lines;
}

std::string join(const std::vector<std::string> &parts, std::string_view separator) {
  std::string out;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i)
      out += separator;
    out += parts[i];
  }
  return out;
}

void replaceAll(std::string &str, std::string_view from, std::string_view to) {
  if (from.empty())
    return;
  std::size_t pos = 0;
  while ((pos = str.find(from, pos)) != std::string::npos) {
    str.replace(pos, from.size(), to);
    pos += to.size();
  }
}

std::string replaced(std::string str, std::string_view from, std::string_view to) {
  replaceAll(str, from, to);
  return str;
}

std::string toLower(std::string_view str) {
  std::string out(str);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

std::string trim(std::string_view str) {
  const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
  std::size_t begin = 0;
  std::size_t end = str.size();
  while (begin < end && isSpace(static_cast<unsigned char>(str[begin])))
    ++begin;
  while (end > begin && isSpace(static_cast<unsigned char>(str[end - 1])))
    --end;
  return std::string(str.substr(begin, end - begin));
}

bool containsIgnoreCase(std::string_view haystack, std::string_view needle) {
  if (needle.empty())
    return true;
  return toLower(haystack).find(toLower(needle)) != std::string::npos;
}

std::string singleLine(std::string_view str) {
  std::string out(str);
  replaceAll(out, "\r\n", " ");
  std::replace(out.begin(), out.end(), '\n', ' ');
  std::replace(out.begin(), out.end(), '\r', ' ');
  return out;
}

namespace {
template <typename T>
std::optional<T> parseIntegral(std::string_view str) {
  const std::string trimmed = trim(str);
  if (trimmed.empty())
    return std::nullopt;
  const char *begin = trimmed.data();
  const char *end = trimmed.data() + trimmed.size();
  if (*begin == '+')
    ++begin;
  T value{};
  const auto [ptr, ec] = std::from_chars(begin, end, value);
  if (ec != std::errc() || ptr != end)
    return std::nullopt;
  return value;
}
} // namespace

std::optional<int> parseInt(std::string_view str) { return parseIntegral<int>(str); }
std::optional<long long> parseInt64(std::string_view str) { return parseIntegral<long long>(str); }
std::optional<unsigned long long> parseUInt64(std::string_view str) { return parseIntegral<unsigned long long>(str); }

std::optional<double> parseDouble(std::string_view str) {
  std::string trimmed = trim(str);
  if (trimmed.empty())
    return std::nullopt;
  // Tolerate a decimal comma (German user input)
  if (trimmed.find('.') == std::string::npos)
    std::replace(trimmed.begin(), trimmed.end(), ',', '.');
  const char *begin = trimmed.data();
  const char *end = trimmed.data() + trimmed.size();
  if (*begin == '+')
    ++begin;
  double value{};
  const auto [ptr, ec] = std::from_chars(begin, end, value);
  if (ec != std::errc() || ptr != end)
    return std::nullopt;
  return value;
}

std::string formatDoubleLegacy(double value) {
  std::ostringstream os;
  os.imbue(std::locale::classic());
  os << value;
  return os.str();
}

std::string htmlEscape(std::string_view str) {
  std::string out;
  out.reserve(str.size());
  for (const char c : str) {
    switch (c) {
    case '&': out += "&amp;"; break;
    case '<': out += "&lt;"; break;
    case '>': out += "&gt;"; break;
    case '"': out += "&quot;"; break;
    case '\'': out += "&#39;"; break;
    default: out += c; break;
    }
  }
  return out;
}

} // namespace st::util
