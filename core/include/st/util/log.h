#pragma once

#include <format>
#include <functional>
#include <string>
#include <string_view>

/// Minimal logging facade. The core library logs through this interface; the
/// application redirects it into the cpp_app log file via `setSink`.
namespace st::log {

enum class Level { Info,
                   Warning,
                   Error };

using Sink = std::function<void(Level, std::string_view)>;

/// Replaces the output target (default: stderr). Pass nullptr to restore the default.
void setSink(Sink sink);

/// Writes one message to the current sink.
void write(Level level, std::string_view message);

template <typename... Args>
void info(std::format_string<Args...> fmt, Args &&...args) {
  write(Level::Info, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void warning(std::format_string<Args...> fmt, Args &&...args) {
  write(Level::Warning, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void error(std::format_string<Args...> fmt, Args &&...args) {
  write(Level::Error, std::format(fmt, std::forward<Args>(args)...));
}

} // namespace st::log
