#include "st/util/log.h"

#include <cstdio>
#include <mutex>

namespace st::log {

namespace {
std::mutex &sinkMutex() {
  static std::mutex m;
  return m;
}

Sink &currentSink() {
  static Sink sink;
  return sink;
}

const char *prefix(Level level) {
  switch (level) {
  case Level::Info: return "[INFO]";
  case Level::Warning: return "[WARNING]";
  case Level::Error: return "[ERROR]";
  }
  return "";
}
} // namespace

void setSink(Sink sink) {
  std::lock_guard lock(sinkMutex());
  currentSink() = std::move(sink);
}

void write(Level level, std::string_view message) {
  std::lock_guard lock(sinkMutex());
  if (currentSink()) {
    currentSink()(level, message);
    return;
  }
  std::fprintf(stderr, "%s %.*s\n", prefix(level), static_cast<int>(message.size()), message.data());
}

} // namespace st::log
