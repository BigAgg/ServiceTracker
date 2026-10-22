#pragma once

#include "framework/desktop_notifier.h"

#include <st/alerts/alert_center.h>

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace st::app {

class AppContext;

/// Notification system of the program ("Benachrichtigungen").
///
/// Modules register channels in `init` and report the items that need attention
/// whenever their data changes:
///
///   ctx.alerts.addChannel(ctx, channel);            // only if the user is in channel.audience
///   ctx.alerts.report("orders.prepared", items);    // new items -> notification
///
/// Delivery: Windows notification if the program is not the active window, otherwise
/// an in-program message. While any channel has items the taskbar icon shows a red dot.
/// User preferences (on/off, badge, muted channels) are applied automatically.
/// See docs/benachrichtigungen.md.
class Alerts {
public:
  Alerts();
  ~Alerts();
  Alerts(const Alerts &) = delete;
  Alerts &operator=(const Alerts &) = delete;

  /// Sets the operating system backend (main: DesktopNotifier::create(), smoke test:
  /// RecordingNotifier). Without a backend everything is shown inside the program.
  void setNotifier(std::unique_ptr<DesktopNotifier> notifier);
  DesktopNotifier *notifier() { return mNotifier.get(); }

  /// Registers `channel` if the current user belongs to its audience.
  /// Returns true if the user receives this channel.
  bool addChannel(const AppContext &ctx, alerts::Channel channel);
  /// Removes a channel (e.g. a deleted status rule) and its notification on screen.
  void removeChannel(std::string_view channel);
  /// True if the user receives the channel.
  bool subscribed(std::string_view channel) const { return mCenter.hasChannel(channel); }

  /// Reports the items that currently need attention (ignored if not subscribed).
  void report(std::string_view channel, const std::vector<alerts::Item> &items) { mCenter.report(channel, items); }
  /// Marks an item as known, e.g. because the user changed it himself.
  void markSeen(std::string_view channel, std::string_view itemId) { mCenter.markSeen(channel, itemId); }

  /// Shows a test notification of `channel` with its current state (settings dialog).
  void showTest(AppContext &ctx, std::string_view channel);

  /// Delivers queued notifications, updates badge and tooltip, handles clicks.
  /// Called by AppContext::update (outside the ImGui frame).
  void update(AppContext &ctx);
  /// Focuses the window of a clicked notification. Called inside the ImGui frame.
  void drawFrame(AppContext &ctx);

  const alerts::AlertCenter &center() const { return mCenter; }

private:
  void deliver(AppContext &ctx, const alerts::Notification &notification);

  alerts::AlertCenter mCenter;
  std::unique_ptr<DesktopNotifier> mNotifier;
  /// Channels with a notification on screen / in the notification center.
  std::set<std::string, std::less<>> mOnScreen;
  std::string mFocusTarget;
  std::string mTooltip;
};

} // namespace st::app
