#include "st/users/user_store.h"

#include "st/util/strings.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <set>

namespace st::users {

namespace {
constexpr char kXorKey = 0x5A;

void xorCrypt(std::string &data) {
  for (char &c : data)
    c = static_cast<char>(c ^ kXorKey);
}

std::string normalize(std::string_view name) { return util::toLower(util::trim(name)); }
} // namespace

std::vector<User> decodeUserTable(std::string_view bytes) {
  std::vector<User> users;
  std::size_t pos = 0;
  while (pos + sizeof(std::uint32_t) <= bytes.size()) {
    std::uint32_t size = 0;
    std::memcpy(&size, bytes.data() + pos, sizeof(size)); // little endian like the legacy writer
    pos += sizeof(size);
    if (size > bytes.size() - pos)
      break;
    std::string record(bytes.substr(pos, size));
    pos += size;
    xorCrypt(record);

    auto [head, groupList] = util::splitAt(record, "=");
    auto [name, flag] = util::splitAt(head, "[");
    if (!flag.empty() && flag.back() == ']')
      flag.pop_back();
    User user;
    user.name = name;
    user.poweruser = util::parseInt(flag).value_or(0) != 0;
    user.groups = util::split(groupList, ',', true);
    if (user.name.empty())
      continue;
    // later records override earlier ones (legacy behaviour)
    auto it = std::find_if(users.begin(), users.end(), [&](const User &u) { return u.name == user.name; });
    if (it != users.end())
      *it = std::move(user);
    else
      users.push_back(std::move(user));
  }
  return users;
}

std::string encodeUserTable(const std::vector<User> &users) {
  std::string out;
  for (const auto &user : users) {
    std::string record = user.name + "[" + (user.poweruser ? "1" : "0") + "]=" + util::join(user.groups, ",");
    xorCrypt(record);
    const auto size = static_cast<std::uint32_t>(record.size());
    char header[sizeof(size)];
    std::memcpy(header, &size, sizeof(size));
    out.append(header, sizeof(size));
    out += record;
  }
  return out;
}

UserStore::UserStore(util::fs::path file, std::string bootstrapAdmin)
    : mFile(std::move(file)), mBootstrapAdmin(normalize(bootstrapAdmin)) {
  ensureBootstrapAdmin();
}

util::Status UserStore::load() {
  mUsers.clear();
  if (util::pathExists(mFile)) {
    auto bytes = util::readFile(mFile);
    if (!bytes)
      return std::unexpected(bytes.error());
    mUsers = decodeUserTable(*bytes);
  }
  ensureBootstrapAdmin();
  return {};
}

util::Status UserStore::save() const {
  return util::writeFileAtomic(mFile, encodeUserTable(mUsers));
}

void UserStore::ensureBootstrapAdmin() {
  if (mBootstrapAdmin.empty())
    return;
  auto it = std::find_if(mUsers.begin(), mUsers.end(), [&](const User &u) { return normalize(u.name) == mBootstrapAdmin; });
  if (it == mUsers.end())
    mUsers.insert(mUsers.begin(), User{mBootstrapAdmin, {}, true});
  else
    it->poweruser = true;
}

const User *UserStore::find(std::string_view name) const {
  const std::string key = normalize(name);
  auto it = std::find_if(mUsers.begin(), mUsers.end(), [&](const User &u) { return normalize(u.name) == key; });
  return it == mUsers.end() ? nullptr : &*it;
}

std::vector<std::string> UserStore::names() const {
  std::vector<std::string> out;
  for (const auto &u : mUsers)
    out.push_back(u.name);
  std::sort(out.begin(), out.end());
  return out;
}

std::vector<std::string> UserStore::knownGroups() const {
  std::set<std::string> groupSet;
  for (const auto &u : mUsers)
    groupSet.insert(u.groups.begin(), u.groups.end());
  return {groupSet.begin(), groupSet.end()};
}

bool UserStore::isPoweruser(std::string_view user) const {
  const User *u = find(user);
  return u && u->poweruser;
}

bool UserStore::hasAccess(std::string_view user, std::string_view group) const {
  const User *u = find(user);
  return u && std::find(u->groups.begin(), u->groups.end(), group) != u->groups.end();
}

bool UserStore::hasAnyAccess(std::string_view user, const std::vector<std::string> &groupList) const {
  if (groupList.empty())
    return true;
  return std::any_of(groupList.begin(), groupList.end(), [&](const std::string &g) { return hasAccess(user, g); });
}

util::Status UserStore::upsert(User user) {
  user.name = normalize(user.name);
  if (user.name.empty())
    return std::unexpected(util::Error{"Benutzername darf nicht leer sein"});
  std::sort(user.groups.begin(), user.groups.end());
  user.groups.erase(std::unique(user.groups.begin(), user.groups.end()), user.groups.end());
  std::erase_if(user.groups, [](const std::string &g) { return g.empty(); });
  auto it = std::find_if(mUsers.begin(), mUsers.end(), [&](const User &u) { return normalize(u.name) == user.name; });
  if (it != mUsers.end())
    *it = std::move(user);
  else
    mUsers.push_back(std::move(user));
  ensureBootstrapAdmin();
  return save();
}

util::Status UserStore::remove(std::string_view name) {
  const std::string key = normalize(name);
  if (key == mBootstrapAdmin)
    return std::unexpected(util::Error{"Der Administrator kann nicht entfernt werden"});
  std::erase_if(mUsers, [&](const User &u) { return normalize(u.name) == key; });
  return save();
}

} // namespace st::users
