#include "st/alerts/alert_center.h"

#include <algorithm>
#include <format>

namespace st::alerts {

bool AlertCenter::addChannel(Channel channel) {
  if (channel.id.empty() || hasChannel(channel.id))
    return false;
  State state;
  state.channel = std::move(channel);
  mStates.push_back(std::move(state));
  return true;
}

bool AlertCenter::hasChannel(std::string_view id) const { return find(id) != nullptr; }

const Channel *AlertCenter::channel(std::string_view id) const {
  const State *state = find(id);
  return state ? &state->channel : nullptr;
}

std::vector<const Channel *> AlertCenter::channels() const {
  std::vector<const Channel *> result;
  result.reserve(mStates.size());
  for (const auto &state : mStates)
    result.push_back(&state.channel);
  return result;
}

AlertCenter::State *AlertCenter::find(std::string_view id) {
  auto it = std::find_if(mStates.begin(), mStates.end(), [&](const State &s) { return s.channel.id == id; });
  return it == mStates.end() ? nullptr : &*it;
}

const AlertCenter::State *AlertCenter::find(std::string_view id) const {
  auto it = std::find_if(mStates.begin(), mStates.end(), [&](const State &s) { return s.channel.id == id; });
  return it == mStates.end() ? nullptr : &*it;
}

std::string AlertCenter::summaryOf(const State &state) const {
  if (state.channel.summaryText)
    return state.channel.summaryText(state.count);
  return std::format("{}: {}", state.channel.label, state.count);
}

void AlertCenter::queue(const State &state, std::string body) {
  mPending.push_back({state.channel.id, state.channel.title, std::move(body), state.channel.target});
}

void AlertCenter::report(std::string_view channelId, const std::vector<Item> &items) {
  State *state = find(channelId);
  if (!state)
    return;
  const Channel &ch = state->channel;
  const bool firstReport = !state->reported;
  state->reported = true;
  state->count = items.size();

  std::vector<const Item *> fresh;
  if (!firstReport || ch.notifyOnStart)
    for (const auto &item : items)
      if (!state->known.contains(item.id))
        fresh.push_back(&item);

  state->known.clear();
  for (const auto &item : items)
    state->known.insert(item.id);

  const auto dropQueued = [&] { std::erase_if(mPending, [&](const Notification &n) { return n.channel == ch.id; }); };
  if (items.empty() && ch.mode == Mode::Summary)
    dropQueued(); // nothing left to announce
  if (fresh.empty() || isMuted(ch.id))
    return;

  const bool collapse = ch.mode == Mode::Summary ||
                        (fresh.size() > ch.maxItemNotifications && ch.summaryText) || !ch.itemText;
  if (collapse) {
    if (ch.mode == Mode::Summary)
      dropQueued();
    queue(*state, summaryOf(*state));
    return;
  }
  for (const Item *item : fresh)
    queue(*state, ch.itemText(*item));
}

void AlertCenter::markSeen(std::string_view channelId, std::string_view itemId) {
  if (State *state = find(channelId))
    state->known.insert(std::string(itemId));
}

void AlertCenter::setMuted(std::string_view channelId, bool muted) {
  if (muted) {
    mMuted.insert(std::string(channelId));
    std::erase_if(mPending, [&](const Notification &n) { return n.channel == channelId; });
  } else if (auto it = mMuted.find(channelId); it != mMuted.end()) {
    mMuted.erase(it);
  }
}

void AlertCenter::setMutedChannels(const std::vector<std::string> &ids) {
  for (auto it = mMuted.begin(); it != mMuted.end();)
    it = std::find(ids.begin(), ids.end(), *it) == ids.end() ? mMuted.erase(it) : std::next(it);
  for (const auto &id : ids)
    setMuted(id, true);
}

bool AlertCenter::isMuted(std::string_view channelId) const { return mMuted.contains(channelId); }

std::vector<Notification> AlertCenter::takePending() {
  std::vector<Notification> result;
  result.swap(mPending);
  return result;
}

std::size_t AlertCenter::count(std::string_view channelId) const {
  const State *state = find(channelId);
  return state ? state->count : 0;
}

std::size_t AlertCenter::activeCount() const {
  std::size_t total = 0;
  for (const auto &state : mStates)
    if (!isMuted(state.channel.id))
      total += state.count;
  return total;
}

std::vector<std::string> AlertCenter::summaryLines() const {
  std::vector<std::string> lines;
  for (const auto &state : mStates)
    if (state.count > 0 && !isMuted(state.channel.id))
      lines.push_back(summaryOf(state));
  return lines;
}

std::string AlertCenter::firstActiveTarget() const {
  for (const auto &state : mStates)
    if (state.count > 0 && !isMuted(state.channel.id) && !state.channel.target.empty())
      return state.channel.target;
  return {};
}

std::string countSentence(std::size_t count, std::string_view singular, std::string_view plural, std::string_view predicate) {
  if (count == 1)
    return std::format("Es ist 1 {} {}", singular, predicate);
  return std::format("Es sind {} {} {}", count, plural, predicate);
}

} // namespace st::alerts
