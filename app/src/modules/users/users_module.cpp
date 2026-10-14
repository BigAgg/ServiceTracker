#include "modules/users/users_module.h"

#include "framework/app_context.h"
#include "framework/widgets.h"

#include <st/util/strings.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>

namespace st::app {

bool UsersModule::available(const AppContext &ctx) const { return ctx.isPoweruser(); }

void UsersModule::init(AppContext &ctx) {
  ctx.ui.addMenu("Benutzer", [this, &ctx] {
    if (ImGui::MenuItem("Benutzer verwalten..."))
      mOpen = true;
    if (ImGui::MenuItem("Neu laden"))
      ctx.notify.check(ctx.userStore.load(), "Benutzer laden");
  });
}

void UsersModule::drawOverlay(AppContext &ctx) {
  if (!mOpen)
    return;
  if (!mPendingSelect.empty()) {
    if (const auto *user = ctx.userStore.find(mPendingSelect)) {
      mSelected = user->name;
      mDraft = *user;
      mIsNew = false;
    }
    mPendingSelect.clear();
  }
  ImGui::SetNextWindowSize(ImVec2(700.0f, 450.0f), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Benutzerverwaltung", &mOpen, ImGuiWindowFlags_NoDocking))
    drawEditor(ctx);
  ImGui::End();
}

void UsersModule::drawEditor(AppContext &ctx) {
  auto &store = ctx.userStore;
  ImGui::BeginChild("##userlist", ImVec2(220.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
  if (ImGui::Button("Neuer Benutzer")) {
    mDraft = {};
    mSelected.clear();
    mIsNew = true;
  }
  ui::searchField("usersearch", mSearch);
  for (const auto &name : store.names()) {
    if (!util::containsIgnoreCase(name, mSearch))
      continue;
    const auto *user = store.find(name);
    const std::string label = name + (user && user->poweruser ? "  *" : "");
    if (ImGui::Selectable(label.c_str(), name == mSelected && !mIsNew)) {
      mSelected = name;
      mDraft = *user;
      mIsNew = false;
    }
  }
  ImGui::EndChild();
  ImGui::SameLine();

  ImGui::BeginChild("##useredit");
  if (!mIsNew && mSelected.empty()) {
    ui::hintText("Benutzer wählen oder neu anlegen. * = Poweruser");
    ImGui::EndChild();
    return;
  }
  if (mIsNew) {
    ImGui::InputTextWithHint("Windows-Benutzername", "z.B. mmueller", &mDraft.name);
    if (store.find(mDraft.name))
      ui::errorText("Benutzer existiert bereits");
  } else {
    ImGui::Text("Benutzer: %s", mDraft.name.c_str());
  }
  ImGui::Checkbox("Poweruser", &mDraft.poweruser);
  ui::help("Poweruser dürfen Checklisten-Vorlagen, Configs und Benutzer bearbeiten.");

  ImGui::SeparatorText("Abteilungen / Berechtigungen");
  if (ImGui::BeginCombo("##groups", "Vorhandene Abteilung hinzufügen...")) {
    for (const auto &group : store.knownGroups())
      if (std::find(mDraft.groups.begin(), mDraft.groups.end(), group) == mDraft.groups.end() && ImGui::Selectable(group.c_str()))
        mDraft.groups.push_back(group);
    ImGui::EndCombo();
  }
  ImGui::SetNextItemWidth(200.0f);
  const bool enter = ImGui::InputTextWithHint("##newgroup", "neue Abteilung", &mNewGroup, ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SameLine();
  if ((ImGui::Button("Hinzufügen") || enter) && !util::trim(mNewGroup).empty()) {
    mDraft.groups.push_back(util::trim(mNewGroup));
    mNewGroup.clear();
  }
  ui::hintText("Vom Programm genutzt: SERVICE_CREATOR, SERVICE_CREATOR_EXTERN, TECHSUPPORT, SALES");
  for (std::size_t i = 0; i < mDraft.groups.size(); ++i) {
    ui::ScopedId id(static_cast<int>(i));
    if (ImGui::SmallButton("x")) {
      mDraft.groups.erase(mDraft.groups.begin() + static_cast<std::ptrdiff_t>(i));
      break;
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(mDraft.groups[i].c_str());
  }

  ImGui::Separator();
  const bool valid = !util::trim(mDraft.name).empty() && (!mIsNew || !store.find(mDraft.name));
  {
    ui::Disabled d(!valid);
    if (ImGui::Button("Speichern")) {
      if (ctx.notify.check(store.upsert(mDraft), "Benutzer speichern")) {
        ctx.notify.success("Benutzer " + mDraft.name + " gespeichert");
        mSelected = util::toLower(util::trim(mDraft.name));
        mIsNew = false;
        if (const auto *u = store.find(mSelected))
          mDraft = *u;
      }
    }
  }
  if (!mIsNew) {
    ImGui::SameLine();
    if (ui::confirmButton("Entfernen", "Benutzer wirklich entfernen?", mDraft.name == ctx.user())) {
      if (ctx.notify.check(store.remove(mDraft.name), "Benutzer entfernen")) {
        mSelected.clear();
        mDraft = {};
      }
    }
  }
  ImGui::EndChild();
}

} // namespace st::app
