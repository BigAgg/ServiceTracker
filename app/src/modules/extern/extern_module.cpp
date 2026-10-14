#include "modules/extern/extern_module.h"

#include "framework/events.h"
#include "framework/platform.h"
#include "framework/widgets.h"
#include "modules/service/service_module.h"

#include <st/checklist/actions.h>
#include <st/users/user_store.h>
#include <st/util/strings.h>

#include <imgui.h>
#include <imgui/ImGui_helpers.h>

namespace st::app {

bool ExternModule::available(const AppContext &ctx) const {
  const auto file = util::pathFromUtf8(ctx.paths().externFile);
  return ctx.hasAccess(users::groups::Techsupport) && util::isDirectory(file.parent_path());
}

void ExternModule::init(AppContext &ctx) {
  mInfo = kExternTemplate;
  mPoll.setInterval(std::chrono::milliseconds(std::max(1000, ctx.settings().refreshIntervalMs * 2)));
  ctx.notify.check(ctx.services().environment().externStore.ensureExists(), "extern.json");
  ctx.ui.addWindow("Externe Services", [this, &ctx] { drawWindow(ctx); }, true, ImGuiWindowFlags_HorizontalScrollbar);
  ctx.events.subscribe<events::RecordsChanged>([this, &ctx](const auto &) { reload(ctx); });
  reload(ctx);
}

void ExternModule::update(AppContext &ctx) {
  if (mPoll.due() && ctx.services().environment().externStore.lastWriteTime() != mListTime)
    reload(ctx);
}

void ExternModule::reload(AppContext &ctx) {
  const auto &store = ctx.services().environment().externStore;
  mListTime = store.lastWriteTime();
  auto list = store.loadAll();
  if (list)
    mList = std::move(*list);
}

void ExternModule::drawPlanning(AppContext &ctx) {
  auto *device = ctx.activeDevice();
  const auto &record = ctx.activeRecord();
  if (!device) {
    ui::errorText("Gerät in \"Reader Control\" wählen...");
    return;
  }
  ImGui::Text("Gerät: %s", device->name.c_str());
  ImGui::DatePicker("Service Datum", mDate, false, 0.0f, 0.0f, "%Y.%m.%d");
  ImGui::SetItemTooltip("Rechtsklick entfernt das Datum");
  if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    mDate.clear();
  ui::todayButton("today", mDate, ctx.session().date());

  const bool inList = !service::isClosedStatus(record.status);
  {
    ui::Disabled d(inList);
    ImGui::TextUnformatted("Serviceinformationen");
    ui::inputMultiline("##externinfo", mInfo, 5.0f);
  }
  if (inList)
    ui::errorText("Gerät ist bereits in der Liste (Status \"" + record.status + "\")");
  else if (mDate.empty())
    ui::errorText("Datum wählen...");
  ui::Disabled d(inList || mDate.empty());
  if (ImGui::Button("Externen Service erstellen")) {
    auto created = ctx.services().createExternService(*device, mDate, mInfo, ctx.session());
    if (ctx.notify.check(created, "Externer Service")) {
      ctx.applyRecord(*created);
      reload(ctx);
      ctx.notify.success("Externer Service für " + device->name + " am " + mDate + " geplant. Gerätedaten wurden lokal kopiert.");
      mDate.clear();
      mInfo = kExternTemplate;
    }
  }
}

void ExternModule::drawWindow(AppContext &ctx) {
  {
    ui::Disabled d(!ctx.deviceRegistry.hasAccess());
    if (ImGui::TreeNode("Service planen")) {
      drawPlanning(ctx);
      ImGui::TreePop();
    }
  }

  ImGui::SeparatorText("Externe Services");
  if (ImGui::BeginTable("##extern", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, 10.0f * ImGui::GetTextLineHeightWithSpacing()))) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Gerät");
    ImGui::TableSetupColumn("Service");
    ImGui::TableSetupColumn("Status");
    ImGui::TableHeadersRow();
    for (const auto &r : mList) {
      ui::ScopedId id(r.name.c_str());
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      if (ImGui::Selectable(r.name.c_str(), r.name == ctx.activeDeviceName(), ImGuiSelectableFlags_SpanAllColumns))
        ctx.selectDevice(r.name);
      if (!r.additionalInfo.empty())
        ImGui::SetItemTooltip("%s", r.additionalInfo.c_str());
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(r.activeService.c_str());
      ImGui::TableSetColumnIndex(2);
      ui::statusBadge(ctx.statusIndex().statusOf(r.name).empty() ? r.status : ctx.statusIndex().statusOf(r.name));
    }
    ImGui::EndTable();
  }
  if (mList.empty())
    ui::hintText("Keine externen Services geplant.");

  auto *device = ctx.activeDevice();
  const auto &record = ctx.activeRecord();
  namespace s = service::status;
  const bool externalActive = device && (record.status == s::PlannedExternal || record.status == s::External);
  {
    ui::Disabled d(!externalActive);
    if (ImGui::Button("Service abschließen"))
      ImGui::OpenPopup("Externen Service abschließen");
    ImGui::SameLine();
    if (ImGui::Button("Als \"extern\" markieren") && device) {
      if (auto r = ctx.services().setStatus(*device, s::External); ctx.notify.check(r))
        ctx.applyRecord(*r);
    }
    ImGui::SetItemTooltip("Techniker ist beim Kunden");
    ImGui::SameLine();
    if (ImGui::Button("Lokalen Ordner öffnen") && device)
      platform::openPath(ctx.services().externCopyFor(*device));
    ImGui::SameLine();
    if (ui::confirmButton("Aus Liste entfernen", "Externen Service wirklich entfernen?\nDie lokale Kopie bleibt erhalten.", !externalActive) && device) {
      if (ctx.notify.check(ctx.services().cancelExternService(*device), "Entfernen")) {
        auto updated = record;
        updated.status.clear();
        ctx.applyRecord(updated);
        reload(ctx);
      }
    }
  }
  if (!externalActive && device)
    ui::hintText("Gewähltes Gerät hat keinen offenen externen Service.");

  if (ImGui::BeginPopupModal("Externen Service abschließen", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Externen Service für %s abschließen?", ctx.activeDeviceName().c_str());
    ImGui::TextUnformatted("Der Service-Ordner wird aus der lokalen Kopie in den Geräteordner\n"
                           "kopiert, die lokale Kopie wird gelöscht und der Service in die\n"
                           "Historie übernommen.");
    if (ImGui::Button("Abschließen")) {
      finish(ctx);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Abbrechen") || ImGui::IsKeyPressed(ImGuiKey_Escape))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}

void ExternModule::finish(AppContext &ctx) {
  auto *device = ctx.activeDevice();
  if (!device)
    return;
  auto &session = ctx.checklist();
  const checklist::Checklist *data = nullptr;
  // The open checklist belongs to this service if it lives in the local copy
  if (session.doc && session.doc->path().native().starts_with(ctx.services().externCopyFor(*device).native())) {
    checklist::finishChecklist(session.doc->data(), ctx.expressions, ctx.sectionActions, ctx.session());
    if (!ctx.notify.check(session.doc->save(), "Checkliste speichern"))
      return;
    session.dirty = false;
    data = &session.doc->data();
  }
  auto result = ctx.services().finishExternService(*device, data, ServiceModule::htmlOptions(ctx), ctx.session());
  if (!ctx.notify.check(result, "Externen Service abschließen"))
    return;
  ctx.applyRecord(result->record);
  reload(ctx);
  ctx.notify.success(device->name + ": externer Service abgeschlossen");
  if (!result->reportFile.empty())
    platform::openPath(result->reportFile);
  const std::string service = ctx.selectedService();
  ctx.selectService({});
  ctx.selectService(service);
}

} // namespace st::app
