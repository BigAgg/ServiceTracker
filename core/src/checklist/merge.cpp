#include "st/checklist/merge.h"

#include <algorithm>

namespace st::checklist {

namespace {

/// Timestamps are "YYYY-MM-DD HH:MM:SS" and therefore comparable as strings.
bool isNewer(const std::string &candidate, const std::string &current) { return candidate > current; }

std::size_t mergeTask(Task &local, const Task &remote) {
  if (!isNewer(remote.timestamp, local.timestamp))
    return 0;
  local = remote;
  return 1;
}

std::size_t mergeGroup(TaskGroup &local, const TaskGroup &remote) {
  if (local.tasks.size() != remote.tasks.size())
    return 0;
  std::size_t changes = 0;
  for (std::size_t i = 0; i < local.tasks.size(); ++i)
    changes += mergeTask(local.tasks[i], remote.tasks[i]);
  return changes;
}

std::size_t mergeTable(TaskTable &local, const TaskTable &remote) {
  std::size_t changes = 0;
  if (isNewer(remote.timestamp, local.timestamp)) {
    // Remote changed the row structure more recently: take its rows, keep newer local cells.
    std::vector<TaskGroup> rows = remote.rows;
    const std::size_t common = std::min(rows.size(), local.rows.size());
    for (std::size_t i = 0; i < common; ++i)
      mergeGroup(rows[i], local.rows[i]);
    local.rows = std::move(rows);
    local.timestamp = remote.timestamp;
    return 1;
  }
  const std::size_t common = std::min(local.rows.size(), remote.rows.size());
  for (std::size_t i = 0; i < common; ++i)
    changes += mergeGroup(local.rows[i], remote.rows[i]);
  return changes;
}

std::size_t mergeItem(Item &local, const Item &remote) {
  if (local.index() != remote.index())
    return 0;
  return std::visit([&](auto &l) -> std::size_t {
    using T = std::decay_t<decltype(l)>;
    const T &r = std::get<T>(remote);
    if constexpr (std::is_same_v<T, Task>)
      return mergeTask(l, r);
    else if constexpr (std::is_same_v<T, TaskGroup>)
      return mergeGroup(l, r);
    else
      return mergeTable(l, r);
  },
                    local);
}

std::size_t mergeSection(Section &local, const Section &remote) {
  std::size_t changes = 0;
  if (isNewer(remote.timestamp, local.timestamp)) {
    local.timestamp = remote.timestamp;
    local.user = remote.user;
    local.finished = remote.finished;
    ++changes;
  }
  if (local.items.size() != remote.items.size())
    return changes;
  for (std::size_t i = 0; i < local.items.size(); ++i)
    changes += mergeItem(local.items[i], remote.items[i]);
  return changes;
}

} // namespace

std::size_t mergeNewer(Checklist &local, const Checklist &remote) {
  std::size_t changes = 0;
  if (isNewer(remote.timestamp, local.timestamp)) {
    local.timestamp = remote.timestamp;
    local.user = remote.user;
    local.finished = remote.finished;
    ++changes;
  }
  for (const auto &remoteSection : remote.sections) {
    for (auto &section : local.sections) {
      if (section.name == remoteSection.name) {
        changes += mergeSection(section, remoteSection);
        break;
      }
    }
  }
  return changes;
}

} // namespace st::checklist
