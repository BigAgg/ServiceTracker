#include "modules/changes/changes_module.h"

#include "framework/app_context.h"
#include "framework/platform.h"
#include "framework/widgets.h"

#include <st/util/strings.h>

#include <app/app.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>

namespace st::app {

bool ChangesModule::available(const AppContext &ctx) const {
  return util::isDirectory(util::pathFromUtf8(ctx.paths().requestsDir));
}

void ChangesModule::init(AppContext &ctx) {
  mStore = changes::ChangeRequestStore(util::pathFromUtf8(ctx.paths().requestsDir));
  ctx.ui.addMenu("Änderungen", [this, &ctx] {
    if (ImGui::MenuItem("Änderung anstoßen..."))
      mSubmitOpen = true;
    if (ImGui::MenuItem("Änderungen verfolgen...")) {
      reload(ctx);
      mOverviewOpen = true;
    }
  });
}

void ChangesModule::reload(AppContext &ctx) {
  auto all = mStore.loadAll();
  if (ctx.notify.check(all, "Änderungswünsche"))
    mRequests = std::move(*all);
}

void ChangesModule::drawOverlay(AppContext &ctx) {
  if (mSubmitOpen) {
    ImGui::SetNextWindowSize(ImVec2(650.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Änderungswunsch", &mSubmitOpen, ImGuiWindowFlags_NoDocking))
      drawSubmit(ctx);
    ImGui::End();
  }
  if (mOverviewOpen) {
    ImGui::SetNextWindowSize(ImVec2(800.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Änderungen verfolgen", &mOverviewOpen, ImGuiWindowFlags_NoDocking))
      drawOverview(ctx);
    ImGui::End();
  }
}

void ChangesModule::drawSubmit(AppContext &ctx) {
  ImGui::TextUnformatted("Titel");
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::InputText("##subject", &mSubject);
  ImGui::TextUnformatted("Beschreibung");
  ImGui::InputTextMultiline("##message", &mMessage, ImVec2(-FLT_MIN, 200.0f));
  ImGui::TextUnformatted("Dateien");
  ui::help("Dateien (z.B. Screenshots) per Drag & Drop in dieses Fenster ziehen. Rechtsklick entfernt.");
  if (ImGui::BeginListBox("##files", ImVec2(-FLT_MIN, 120.0f))) {
    for (std::size_t i = 0; i < mAttachments.size(); ++i) {
      ui::ScopedId id(static_cast<int>(i));
      if (ImGui::Selectable(mAttachments[i].c_str()))
        platform::openPath(util::pathFromUtf8(mAttachments[i]));
      if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        mAttachments.erase(mAttachments.begin() + static_cast<std::ptrdiff_t>(i));
        break;
      }
    }
    ImGui::EndListBox();
  }
  if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ::app::HasDropfiles())
    for (const auto &file : ::app::GetDropfiles())
      if (std::find(mAttachments.begin(), mAttachments.end(), file) == mAttachments.end())
        mAttachments.push_back(file);

  const bool valid = !util::trim(mSubject).empty() && !util::trim(mMessage).empty();
  {
    ui::Disabled d(!valid);
    if (ImGui::Button("Abschicken")) {
      std::vector<util::fs::path> files;
      for (const auto &a : mAttachments)
        files.push_back(util::pathFromUtf8(a));
      auto created = mStore.submit(mSubject, mMessage, files, ctx.user(), ctx.session().timestamp());
      if (ctx.notify.check(created, "Änderungswunsch")) {
        ctx.notify.success("Änderungswunsch abgeschickt. Danke!");
        mSubject.clear();
        mMessage.clear();
        mAttachments.clear();
        mSubmitOpen = false;
      }
    }
  }
  if (!valid)
    ui::hintText("Titel und Beschreibung ausfüllen.");
  ImGui::SameLine();
  if (ImGui::Button("Verwerfen")) {
    mSubject.clear();
    mMessage.clear();
    mAttachments.clear();
    mSubmitOpen = false;
  }
}

void ChangesModule::drawOverview(AppContext &ctx) {
  if (ImGui::Button("Aktualisieren"))
    reload(ctx);
  ImGui::BeginChild("##list", ImVec2(280.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
  for (const bool open : {true, false}) {
    ImGui::SeparatorText(open ? "Offen" : "Bearbeitet");
    for (const auto &r : mRequests) {
      if (r.open != open)
        continue;
      ui::ScopedId id(static_cast<int>(r.id % 0x7fffffff));
      if (ImGui::Selectable(r.subject.c_str(), r.id == mSelected))
        mSelected = r.id;
    }
  }
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("##detail");
  const auto it = std::find_if(mRequests.begin(), mRequests.end(), [&](const auto &r) { return r.id == mSelected; });
  if (it == mRequests.end()) {
    ui::hintText("Änderungswunsch wählen.");
  } else {
    ImGui::Text("Titel: %s", it->subject.c_str());
    if (!it->author.empty())
      ImGui::TextDisabled("von %s am %s", it->author.c_str(), ctx.displayTime(it->created).c_str());
    std::string message = it->message;
    ui::inputMultiline("##message", message, 8.0f, ImGuiInputTextFlags_ReadOnly);
    ImGui::TextUnformatted("Dateien");
    for (const auto &file : it->attachments)
      if (ImGui::Selectable(file.c_str()))
        platform::openPath(util::pathFromUtf8(file));
    if (ctx.isPoweruser()) {
      ImGui::Separator();
      if (ImGui::Button(it->open ? "Als bearbeitet markieren" : "Wieder öffnen")) {
        if (ctx.notify.check(mStore.setOpen(it->id, !it->open), "Status ändern"))
          reload(ctx);
      }
    }
  }
  ImGui::EndChild();
}

} // namespace st::app
