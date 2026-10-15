#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

/// Desktop notifications ("Benachrichtigungen") independent of any UI.
///
/// A feature registers a `Channel` (e.g. "orders.prepared") and reports the set of
/// items that currently need attention (e.g. all orders ready for shipping). The
/// AlertCenter detects which items are new and queues `Notification`s; the app shows
/// them as Windows notifications and sets the taskbar badge while any channel has items.
namespace st::alerts {

/// Something a channel reports, e.g. one order or one device.
struct Item {
  /// Stable key (order id, serial number ...). Used to detect new items.
  std::string id;
  /// Text for per-item messages, e.g. the serial number.
  std::string label;

  bool operator==(const Item &) const = default;
};

/// How new items are announced.
enum class Mode {
  /// One notification with the total count ("Es sind 3 Bestellungen versandbereit").
  Summary,
  /// One notification per new item ("Neuer Reader im Wareneingang 0601-1234").
  PerItem
};

/// A kind of notification. Ids are stored in the user preferences (muted channels) –
/// never rename them.
struct Channel {
  std::string id;
  /// German name for the settings ("Bestellungen versandbereit").
  std::string label;
  /// Notification title ("ServiceTracker - Bestellungen").
  std::string title;
  Mode mode = Mode::Summary;
  /// Body for Summary mode and for collapsed PerItem bursts. Gets the total count.
  std::function<std::string(std::size_t count)> summaryText;
  /// Body for PerItem mode.
  std::function<std::string(const Item &item)> itemText;
  /// Window that is focused when the notification is clicked (may be empty).
  std::string target;
  /// User groups that receive this channel; empty = everybody.
  std::vector<std::string> audience;
  /// Announce existing items on the first report (program start).
  bool notifyOnStart = true;
  /// PerItem mode: more new items than this at once are collapsed into one summary
  /// (only if `summaryText` is set).
  std::size_t maxItemNotifications = 3;
};

/// A notification ready to be shown.
struct Notification {
  std::string channel;
  std::string title;
  std::string body;
  std::string target;

  bool operator==(const Notification &) const = default;
};

class AlertCenter {
public:
  /// Registers a channel. Returns false if the id is empty or already registered.
  bool addChannel(Channel channel);
  bool hasChannel(std::string_view id) const;
  const Channel *channel(std::string_view id) const;
  /// Channels in registration order.
  std::vector<const Channel *> channels() const;

  /// Reports all items that currently need attention. Queues notifications for
  /// items that were not reported before (and for all items on the first report if
  /// `notifyOnStart`). Unknown channels are ignored.
  void report(std::string_view channel, const std::vector<Item> &items);

  /// Marks an item as known without announcing it, e.g. because the user changed it
  /// himself. Call before the next `report`.
  void markSeen(std::string_view channel, std::string_view itemId);

  /// Muted channels queue no notifications and do not count for the badge.
  void setMuted(std::string_view channel, bool muted);
  /// Replaces the muted set (ids of unknown channels are kept for later registration).
  void setMutedChannels(const std::vector<std::string> &ids);
  bool isMuted(std::string_view channel) const;

  /// Removes and returns all queued notifications (oldest first). A Summary channel
  /// has at most one queued notification – the newest count wins.
  std::vector<Notification> takePending();
  bool hasPending() const { return !mPending.empty(); }

  /// Current number of items of a channel.
  std::size_t count(std::string_view channel) const;
  /// Items of all channels that are not muted (drives the taskbar badge).
  std::size_t activeCount() const;
  /// One line per non-muted channel with items, e.g. for the tray tooltip.
  std::vector<std::string> summaryLines() const;
  /// Target window of the first non-muted channel with items (empty if none).
  std::string firstActiveTarget() const;

private:
  struct State {
    Channel channel;
    std::set<std::string> known;
    std::size_t count = 0;
    bool reported = false;
  };
  State *find(std::string_view id);
  const State *find(std::string_view id) const;
  void queue(const State &state, std::string body);
  std::string summaryOf(const State &state) const;

  std::vector<State> mStates;
  std::set<std::string, std::less<>> mMuted;
  std::vector<Notification> mPending;
};

/// "Es ist 1 Bestellung versandbereit" / "Es sind 3 Bestellungen versandbereit".
std::string countSentence(std::size_t count, std::string_view singular, std::string_view plural, std::string_view predicate);

} // namespace st::alerts
