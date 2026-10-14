#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

/// Time and date helpers. All formats match the legacy files:
///  - timestamps: "YYYY-MM-DD HH:MM:SS" (local time) – sortable as plain strings
///  - dates:      "YYYY.MM.DD"
namespace st::util {

using TimePoint = std::chrono::system_clock::time_point;

/// Source of the current time. Inject a FixedClock in tests.
class Clock {
public:
  virtual ~Clock() = default;
  virtual TimePoint now() const = 0;
};

/// Real wall clock.
class SystemClock final : public Clock {
public:
  TimePoint now() const override { return std::chrono::system_clock::now(); }
};

/// Clock returning a settable point in time (tests, simulations).
class FixedClock final : public Clock {
public:
  explicit FixedClock(TimePoint tp = {}) : mNow(tp) {}
  TimePoint now() const override { return mNow; }
  void set(TimePoint tp) { mNow = tp; }
  void advance(std::chrono::seconds s) { mNow += s; }

private:
  TimePoint mNow;
};

/// Builds a time point from local calendar values.
TimePoint makeLocalTime(int year, int month, int day, int hour = 0, int minute = 0, int second = 0);

/// "YYYY-MM-DD HH:MM:SS" in local time.
std::string formatTimestamp(TimePoint tp);

/// "YYYY.MM.DD" in local time (legacy service folder / order date format).
std::string formatDate(TimePoint tp);

/// Parses "YYYY.MM.DD" or "YYYY-MM-DD" (optionally followed by a time part).
std::optional<std::chrono::sys_days> parseDate(std::string_view str);

/// Whole days from `from` to `to` (negative if `to` is earlier). nullopt if a date is invalid.
std::optional<long> daysBetween(std::string_view from, std::string_view to);

/// "YYYY-MM-DD HH:MM:SS" -> "DD.MM.YYYY HH:MM:SS". Returns the input if it cannot be parsed.
std::string toGermanTimestamp(std::string_view timestamp);

/// "YYYY.MM.DD" -> "DD.MM.YYYY". Returns the input if it cannot be parsed.
std::string toGermanDate(std::string_view date);

} // namespace st::util
