#include "st/util/time.h"

#include "st/util/strings.h"

#include <algorithm>
#include <ctime>
#include <format>

namespace st::util {

namespace {
std::tm toLocalTm(TimePoint tp) {
  const std::time_t t = std::chrono::system_clock::to_time_t(tp);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  return tm;
}
} // namespace

TimePoint makeLocalTime(int year, int month, int day, int hour, int minute, int second) {
  std::tm tm{};
  tm.tm_year = year - 1900;
  tm.tm_mon = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_sec = second;
  tm.tm_isdst = -1;
  return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::string formatTimestamp(TimePoint tp) {
  const std::tm tm = toLocalTm(tp);
  return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

std::string formatDate(TimePoint tp) {
  const std::tm tm = toLocalTm(tp);
  return std::format("{:04}.{:02}.{:02}", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

std::optional<std::chrono::sys_days> parseDate(std::string_view str) {
  const std::string s = trim(str);
  if (s.size() < 10)
    return std::nullopt;
  const char sep = s[4];
  if ((sep != '.' && sep != '-') || s[7] != sep)
    return std::nullopt;
  const auto y = parseInt(std::string_view(s).substr(0, 4));
  const auto m = parseInt(std::string_view(s).substr(5, 2));
  const auto d = parseInt(std::string_view(s).substr(8, 2));
  if (!y || !m || !d)
    return std::nullopt;
  const std::chrono::year_month_day ymd{std::chrono::year{*y}, std::chrono::month{static_cast<unsigned>(*m)}, std::chrono::day{static_cast<unsigned>(*d)}};
  if (!ymd.ok())
    return std::nullopt;
  return std::chrono::sys_days{ymd};
}

std::optional<long> daysBetween(std::string_view from, std::string_view to) {
  const auto a = parseDate(from);
  const auto b = parseDate(to);
  if (!a || !b)
    return std::nullopt;
  return static_cast<long>((*b - *a).count());
}

std::string toGermanTimestamp(std::string_view timestamp) {
  const auto date = parseDate(timestamp);
  if (!date)
    return std::string(timestamp);
  const std::chrono::year_month_day ymd{*date};
  std::string out = std::format("{:02}.{:02}.{:04}", static_cast<unsigned>(ymd.day()), static_cast<unsigned>(ymd.month()), static_cast<int>(ymd.year()));
  if (timestamp.size() > 10)
    out += std::string(timestamp.substr(10));
  return out;
}

std::string toGermanDate(std::string_view date) {
  return toGermanTimestamp(date.substr(0, std::min<std::size_t>(date.size(), 10)));
}

} // namespace st::util
