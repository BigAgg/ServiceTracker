#pragma once

#include "st/util/time.h"

#include <string>

namespace st::util {

/// Name of the logged in Windows/Linux user in lower case (USERNAME / USER).
std::string currentUserName();

/// Who is doing something and when. Every operation that stamps data
/// (checklist changes, finished sections, status changes ...) takes a Session,
/// so tests can run with a fixed user and a fixed clock.
class Session {
public:
  Session(std::string user, const Clock &clock) : mUser(std::move(user)), mClock(&clock) {}

  const std::string &user() const { return mUser; }
  const Clock &clock() const { return *mClock; }
  TimePoint now() const { return mClock->now(); }
  /// "YYYY-MM-DD HH:MM:SS"
  std::string timestamp() const { return formatTimestamp(now()); }
  /// "YYYY.MM.DD"
  std::string date() const { return formatDate(now()); }

private:
  std::string mUser;
  const Clock *mClock;
};

} // namespace st::util
