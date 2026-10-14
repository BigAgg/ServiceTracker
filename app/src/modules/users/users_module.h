#pragma once

#include "framework/module.h"

#include <st/users/user_store.h>

#include <string>

namespace st::app {

/// User and permission management. Menu "Benutzer" (power users) with an editor dialog.
class UsersModule final : public Module {
public:
  std::string_view name() const override { return "users"; }
  bool available(const AppContext &ctx) const override;
  void init(AppContext &ctx) override;
  void drawOverlay(AppContext &ctx) override;
  /// Opens the editor dialog (optionally with a user selected).
  void show(std::string user = {}) {
    mOpen = true;
    mPendingSelect = std::move(user);
  }

private:
  void drawEditor(AppContext &ctx);

  bool mOpen = false;
  std::string mPendingSelect;
  std::string mSelected;
  users::User mDraft;
  bool mIsNew = false;
  std::string mNewGroup;
  std::string mSearch;
};

} // namespace st::app
