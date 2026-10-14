#pragma once

#include "st/util/fs.h"

#include <string>
#include <string_view>
#include <vector>

/// Users and permission groups ("Abteilungen").
///
/// The table is stored in the legacy binary format (users.bin): a sequence of
/// records `uint32 length` + `bytes XOR 0x5A`, each record reads
/// `name[poweruser]=GROUP1,GROUP2`.
namespace st::users {

/// Groups the program itself checks for. Any other group name can be used for
/// checklist section permissions.
namespace groups {
inline constexpr std::string_view ServiceCreator = "SERVICE_CREATOR";
inline constexpr std::string_view ServiceCreatorExtern = "SERVICE_CREATOR_EXTERN";
inline constexpr std::string_view Techsupport = "TECHSUPPORT";
inline constexpr std::string_view Sales = "SALES";
} // namespace groups

struct User {
  std::string name;
  std::vector<std::string> groups;
  bool poweruser = false;

  bool operator==(const User &) const = default;
};

/// Decodes the legacy binary table. Broken trailing records are ignored.
std::vector<User> decodeUserTable(std::string_view bytes);
/// Encodes the legacy binary table.
std::string encodeUserTable(const std::vector<User> &users);

class UserStore {
public:
  UserStore() = default;
  /// `bootstrapAdmin` is always a power user (cannot lock yourself out).
  UserStore(util::fs::path file, std::string bootstrapAdmin);

  /// Reads the table from disk. A missing file results in an empty table.
  util::Status load();
  /// Writes the table to disk.
  util::Status save() const;

  const std::vector<User> &users() const { return mUsers; }
  const User *find(std::string_view name) const;
  std::vector<std::string> names() const;
  /// All groups used by any user, sorted.
  std::vector<std::string> knownGroups() const;

  bool isPoweruser(std::string_view user) const;
  bool hasAccess(std::string_view user, std::string_view group) const;
  /// True if the user is in at least one of `groups`. An empty list grants access to everybody.
  bool hasAnyAccess(std::string_view user, const std::vector<std::string> &groups) const;

  /// Adds or replaces a user and saves the table.
  util::Status upsert(User user);
  /// Removes a user and saves the table.
  util::Status remove(std::string_view name);

private:
  void ensureBootstrapAdmin();

  util::fs::path mFile;
  std::string mBootstrapAdmin;
  std::vector<User> mUsers;
};

} // namespace st::users
