#pragma once

#include <st/util/fs.h>

#include <chrono>
#include <deque>
#include <string>

namespace st::app {

/// Short messages ("toasts") in the lower right corner plus a status line.
/// Every message is also written to the log file.
class Notifications {
public:
  enum class Kind { Info,
                    Success,
                    Warning,
                    Error };

  void info(std::string text) { push(Kind::Info, std::move(text)); }
  void success(std::string text) { push(Kind::Success, std::move(text)); }
  void warning(std::string text) { push(Kind::Warning, std::move(text)); }
  void error(std::string text) { push(Kind::Error, std::move(text)); }

  /// Shows the error of a failed operation. Returns true if `result` succeeded.
  template <typename T>
  bool check(const util::Result<T> &result, std::string_view context = {}) {
    if (result)
      return true;
    error(context.empty() ? result.error().message : std::string(context) + ": " + result.error().message);
    return false;
  }
  bool check(const util::Status &status, std::string_view context = {}) {
    if (status)
      return true;
    error(context.empty() ? status.error().message : std::string(context) + ": " + status.error().message);
    return false;
  }

  /// Text shown permanently in the status overlay (e.g. "Sync: 10:21:03").
  void setStatusLine(std::string text) { mStatusLine = std::move(text); }

  /// Draws toasts and the status line. Call once per frame inside the ImGui frame.
  void draw();

private:
  struct Toast {
    Kind kind;
    std::string text;
    std::chrono::steady_clock::time_point created;
  };
  void push(Kind kind, std::string text);

  std::deque<Toast> mToasts;
  std::string mStatusLine;
};

} // namespace st::app
