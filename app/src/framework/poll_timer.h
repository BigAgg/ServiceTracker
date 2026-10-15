#pragma once

#include <chrono>

namespace st::app {

/// Fires at most every `interval` (used to throttle polling of network files).
class PollTimer {
public:
  explicit PollTimer(std::chrono::milliseconds interval = std::chrono::milliseconds(1000)) : mInterval(interval) {}
  void setInterval(std::chrono::milliseconds interval) { mInterval = interval; }
  bool due() {
    const auto now = std::chrono::steady_clock::now();
    if (now - mLast < mInterval)
      return false;
    mLast = now;
    return true;
  }
  void reset() { mLast = {}; }

private:
  std::chrono::milliseconds mInterval;
  std::chrono::steady_clock::time_point mLast{};
};

} // namespace st::app
