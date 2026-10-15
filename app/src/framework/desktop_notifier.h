#pragma once

#include <st/alerts/alert_center.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace st::app {

/// Operating system side of the notification system: shows notifications outside
/// the program window, sets the taskbar badge and reports clicks.
///
/// Windows: notification via a tray icon balloon (shown as Windows notification /
/// toast on Windows 10/11, no shortcut or AUMID registration needed), red dot as
/// taskbar overlay icon (ITaskbarList3). Other platforms and the headless smoke test
/// use `RecordingNotifier`.
class DesktopNotifier {
public:
  /// A notification or the tray icon was clicked.
  struct Activation {
    /// Channel of the clicked notification; empty for a click on the tray icon.
    std::string channel;
  };

  virtual ~DesktopNotifier() = default;

  /// Shows a notification outside the program window.
  virtual void show(const alerts::Notification &notification) = 0;
  /// Shows/hides the red dot on the taskbar icon.
  virtual void setBadge(bool visible) = 0;
  /// Tooltip of the tray icon (several lines).
  virtual void setTooltip(const std::string &text) = 0;
  /// Returns and consumes the last click on a notification / the tray icon.
  virtual std::optional<Activation> takeActivation() = 0;
  /// True if the program window is the active window (notifications are then shown
  /// inside the program instead).
  virtual bool appInForeground() const = 0;
  /// Restores the program window and brings it to the front.
  virtual void activateApp() = 0;

  /// Implementation for the current platform.
  static std::unique_ptr<DesktopNotifier> create();
};

/// Notifier that only records the calls (Linux, headless smoke test).
class RecordingNotifier final : public DesktopNotifier {
public:
  void show(const alerts::Notification &notification) override { shown.push_back(notification); }
  void setBadge(bool visible) override { badge = visible; }
  void setTooltip(const std::string &text) override { tooltip = text; }
  std::optional<Activation> takeActivation() override {
    auto result = std::move(activation);
    activation.reset();
    return result;
  }
  bool appInForeground() const override { return foreground; }
  void activateApp() override { ++activations; }

  std::vector<alerts::Notification> shown;
  bool badge = false;
  std::string tooltip;
  std::optional<Activation> activation;
  bool foreground = false;
  int activations = 0;
};

} // namespace st::app
