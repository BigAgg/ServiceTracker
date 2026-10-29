#pragma once

#include <chrono>
#include <future>
#include <optional>
#include <utility>

namespace st::app {

/// Runs one piece of work on a worker thread and hands the result back to the UI
/// thread, which polls `take()` once per frame. The work must only use data it owns
/// (copies) – never the AppContext. Destroying a running job waits for it.
template <typename T>
class BackgroundJob {
public:
  bool running() const { return mFuture.valid(); }

  /// Starts `work` (callable returning T). Ignored while a job is running.
  template <typename Fn>
  bool start(Fn &&work) {
    if (running())
      return false;
    mFuture = std::async(std::launch::async, std::forward<Fn>(work));
    return true;
  }

  /// The result once the work is done (exactly once), otherwise empty.
  std::optional<T> take() {
    if (!running() || mFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
      return std::nullopt;
    return mFuture.get();
  }

  /// Blocks until the work is done (tests, shutdown).
  std::optional<T> wait() {
    if (!running())
      return std::nullopt;
    return mFuture.get();
  }

private:
  std::future<T> mFuture;
};

} // namespace st::app
