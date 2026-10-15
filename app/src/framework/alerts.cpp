#include "framework/alerts.h"

#include "framework/app_context.h"
#include "framework/events.h"

#include <st/util/log.h>

namespace st::app {

Alerts::Alerts() = default;
Alerts::~Alerts() = default;

void Alerts::setNotifier(std::unique_ptr<DesktopNotifier> notifier) { mNotifier = std::move(notifier); }

bool Alerts::addChannel(const AppContext &ctx, alerts::Channel channel) {
  if (!ctx.hasAnyAccess(channel.audience))
    return false;
  const std::string id = channel.id;
  if (!mCenter.addChannel(std::move(channel))) {
    log::warning("Benachrichtigungskanal doppelt registriert: {}", id);
    return false;
  }
  mCenter.setMutedChannels(ctx.prefs().mutedAlerts);
  return true;
}

void Alerts::showTest(AppContext &ctx, std::string_view channelId) {
  const alerts::Channel *channel = mCenter.channel(channelId);
  if (!channel)
    return;
  const std::size_t count = mCenter.count(channelId);
  alerts::Notification test{channel->id, channel->title,
                            channel->summaryText ? channel->summaryText(count) : channel->label, channel->target,
                            channel->id};
  if (mNotifier && ctx.prefs().desktopNotifications) {
    mNotifier->show(test, ctx.prefs().persistentNotifications); // also when in front – the user wants to see it
    mOnScreen.insert(test.channel);
  } else {
    ctx.notify.info(test.title + ": " + test.body);
  }
}

void Alerts::deliver(AppContext &ctx, const alerts::Notification &notification) {
  if (mNotifier && ctx.prefs().desktopNotifications && !mNotifier->appInForeground()) {
    log::info("Benachrichtigung: {} – {}", notification.title, notification.body);
    mNotifier->show(notification, ctx.prefs().persistentNotifications);
    mOnScreen.insert(notification.channel);
    return;
  }
  ctx.notify.info(notification.body);
}

void Alerts::update(AppContext &ctx) {
  if (mCenter.channels().empty())
    return;
  const auto &prefs = ctx.prefs();
  mCenter.setMutedChannels(prefs.mutedAlerts);
  for (const auto &notification : mCenter.takePending())
    deliver(ctx, notification);

  if (!mNotifier)
    return;
  // Nothing left to announce (e.g. all orders shipped): remove the notification
  for (auto it = mOnScreen.begin(); it != mOnScreen.end();) {
    if (mCenter.count(*it) == 0 || mCenter.isMuted(*it)) {
      mNotifier->withdraw(*it);
      it = mOnScreen.erase(it);
    } else {
      ++it;
    }
  }
  if (auto activation = mNotifier->takeActivation()) {
    const alerts::Channel *channel = mCenter.channel(activation->channel);
    mFocusTarget = channel && !channel->target.empty() ? channel->target : mCenter.firstActiveTarget();
    std::string item;
    if (channel && activation->key.starts_with(channel->id + "/"))
      item = activation->key.substr(channel->id.size() + 1);
    ctx.events.publish(events::AlertActivated{activation->channel, item});
  }
  // Both calls are cheap: the notifier only talks to Windows when something changed
  mNotifier->setBadge(prefs.taskbarBadge && mCenter.activeCount() > 0);
  std::string tooltip = "ServiceTracker";
  for (const auto &line : mCenter.summaryLines())
    tooltip += "\n" + line;
  if (tooltip != mTooltip) {
    mTooltip = tooltip;
    mNotifier->setTooltip(tooltip);
  }
}

void Alerts::drawFrame(AppContext &ctx) {
  if (mFocusTarget.empty())
    return;
  ctx.ui.focus(mFocusTarget);
  mFocusTarget.clear();
}

} // namespace st::app
