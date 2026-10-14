#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace st::app {

/// Typed publish/subscribe between modules.
///
/// Events are plain structs (see events.h). Handlers run synchronously in
/// `publish`, in the order they were subscribed. Modules never call each other
/// directly – they publish what happened and react to what others publish.
///
/// \code
///   ctx.events.subscribe<DeviceSelected>([this](const DeviceSelected &e) { ... });
///   ctx.events.publish(DeviceSelected{"0601-1234"});
/// \endcode
class EventBus {
public:
  using SubscriptionId = std::size_t;

  template <typename Event>
  SubscriptionId subscribe(std::function<void(const Event &)> handler) {
    const SubscriptionId id = ++mNextId;
    auto &list = mHandlers[std::type_index(typeid(Event))];
    list.push_back(Entry{id, [h = std::move(handler)](const void *e) { h(*static_cast<const Event *>(e)); }});
    return id;
  }

  void unsubscribe(SubscriptionId id) {
    for (auto &[type, list] : mHandlers)
      std::erase_if(list, [id](const Entry &e) { return e.id == id; });
  }

  template <typename Event>
  void publish(const Event &event) {
    const auto it = mHandlers.find(std::type_index(typeid(Event)));
    if (it == mHandlers.end())
      return;
    // copy: handlers may subscribe/unsubscribe while being called
    const auto handlers = it->second;
    for (const auto &entry : handlers)
      entry.fn(&event);
  }

private:
  struct Entry {
    SubscriptionId id;
    std::function<void(const void *)> fn;
  };
  std::unordered_map<std::type_index, std::vector<Entry>> mHandlers;
  SubscriptionId mNextId = 0;
};

} // namespace st::app
