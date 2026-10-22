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
  // Offline (at a customer) the window works with the local copies
  return ctx.hasAccess(users::groups::Techsupport) &&
         (util::isDirectory(file.parent_path()) || util::isDirectory(ctx.services().environment().externLocalDir));
}

void ExternModule::init(AppContext &ctx) {
  mInfo = kExternTemplate;
  mPoll.setInterval(std::chrono::milliseconds(std::max(1000, ctx.settings().refreshIntervalMs * 2)));
  if (ctx.syncAvailable())
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
  auto &services = ctx.services();
  auto list = ctx.syncAvailable() ? store.loadAll() : services.localExternRecords();
  if (!list)
    return;
  mList = std::move(*list);
  mLocal.clear();
  if (ctx.syncAvailable())
    services.refreshLocalExtern(mList);
  // Every local copy on this PC is listed, also without an entry in extern.json
  if (auto local = services.localExternRecords()) {
    for (auto &record : *local) {
      mLocal.insert(record.name);
      if (std::none_of(mList.begin(), mList.end(), [&](const service::ServiceRecord &r) { return r.name == record.name; }))
        mList.push_back(std::move(record));
    }
  }
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
  if (ImGui::BeginTable("##extern", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, 10.0f * ImGui::GetTextLineHeightWithSpacing()))) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Gerät");
    ImGui::TableSetupColumn("Service");
    ImGui::TableSetupColumn("Status");
    ImGui::TableSetupColumn("Lokal", ImGuiTableColumnFlags_WidthFixed);
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
      ImGui::TableSetColumnIndex(3);
      if (mLocal.contains(r.name)) {
        ImGui::TextUnformatted("ja");
        ImGui::SetItemTooltip("Lokale Kopie auf diesem PC:\n%s",
                              util::pathToUtf8(ctx.services().environment().externLocalDir / util::pathFromUtf8(r.name)).c_str());
      } else {
        ImGui::TextDisabled("-");
        ImGui::SetItemTooltip("Keine lokale Kopie auf diesem PC (\"Ordner lokal kopieren\")");
      }
    }
    ImGui::EndTable();
  }
  if (mList.empty())
    ui::hintText("Keine externen Services geplant.");

  auto *device = ctx.activeDevice();
  const auto &record = ctx.activeRecord();
  namespace s = service::status;
  const bool externalActive = device && service::isExternalWorkStatus(record.status);
  const bool online = ctx.syncAvailable();
  {
    ui::Disabled d(!externalActive || !online || !ctx.deviceRegistry.hasAccess());
    if (ImGui::Button("Extern abschließen"))
      ImGui::OpenPopup("Extern abschließen");
    ImGui::SetItemTooltip("Wieder im Haus: lokale Arbeit in den Geräteordner übernehmen.\n"
                          "Danach in \"Service Control\" \"Service fertigmelden\".");
  }
  ImGui::SameLine();
  {
    ui::Disabled d(!externalActive || !online);
    if (ImGui::Button("Als \"extern\" markieren") && device) {
      if (auto r = ctx.services().setStatus(*device, s::External); ctx.notify.check(r))
        ctx.applyRecord(*r);
    }
    ImGui::SetItemTooltip("Techniker ist beim Kunden");
  }
  ImGui::SameLine();
  const bool localCopy = device && ctx.services().hasLocalCopy(*device);
  {
    ui::Disabled d(!externalActive || !online || localCopy || !ctx.deviceRegistry.hasAccess());
    if (ImGui::Button("Ordner lokal kopieren") && device) {
      if (ctx.notify.check(ctx.services().copyExternToLocal(*device), "Ordner lokal kopieren")) {
        reload(ctx);
        ctx.notify.success(device->name + ": Geräteordner lokal kopiert – es kann beim Kunden gearbeitet werden.");
        // open the checklist of the local copy
        const std::string service = ctx.selectedService();
        ctx.selectService({});
        ctx.selectService(service);
      }
    }
    ImGui::SetItemTooltip(localCopy ? "Lokale Kopie ist bereits vorhanden"
                                    : "Für den Techniker, der zum Kunden fährt, wenn ein Kollege den Service angelegt hat:\n"
                                      "kopiert den Geräteordner auf diesen PC (Arbeit offline möglich)");
  }
  ImGui::SameLine();
  {
    ui::Disabled d(!localCopy);
    if (ImGui::Button("Lokalen Ordner öffnen") && device)
      platform::openPath(ctx.services().externCopyFor(*device));
  }
  ImGui::SameLine();
  if (ui::confirmButton("Aus Liste entfernen", "Externen Service wirklich entfernen?\nDie lokale Kopie bleibt erhalten.", !externalActive || !online) && device) {
    if (ctx.notify.check(ctx.services().cancelExternService(*device), "Entfernen")) {
      auto updated = record;
      updated.status.clear();
      ctx.applyRecord(updated);
      reload(ctx);
    }
  }
  if (!ctx.syncAvailable())
    ui::hintText("Offline – es wird in der lokalen Kopie gearbeitet. \"Extern abschließen\" ist erst im Haus möglich.");
  else if (device && record.status == s::ExternFinished)
    ui::hintText("Extern abgeschlossen – in \"Service Control\" \"Service fertigmelden\" wählen.");
  else if (!externalActive && device)
    ui::hintText("Gewähltes Gerät hat keinen laufenden externen Service.");
  else if (externalActive && !ctx.deviceRegistry.hasAccess())
    ui::hintText("Geräteordner nicht erreichbar – \"Extern abschließen\" ist erst im Haus möglich.");

  if (ImGui::BeginPopupModal("Extern abschließen", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Externen Service für %s abschließen?", ctx.activeDeviceName().c_str());
    ImGui::TextUnformatted("- Checkliste wird ein letztes Mal mit dem Geräteordner abgeglichen\n"
                           "- der lokale Geräteordner wird über den Original-Geräteordner kopiert\n"
                           "- die lokale Kopie wird gelöscht, Status \"extern abgeschlossen\"\n\n"
                           "Anschließend in \"Service Control\" \"Service fertigmelden\" wählen\n"
                           "(Service Record und Eintrag in der externen Historie).");
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
  // The open checklist of the local copy must be on disk; the core syncs and copies it
  auto &session = ctx.checklist();
  if (session.dirty) {
    ctx.events.publish(events::SaveChecklistRequest{});
    if (session.dirty) {
      ctx.notify.error("Checkliste konnte nicht gespeichert werden – Extern abschließen abgebrochen");
      return;
    }
  }
  auto result = ctx.services().finishExternService(*device);
  if (!ctx.notify.check(result, "Extern abschließen"))
    return;
  ctx.applyRecord(*result);
  reload(ctx);
  ctx.notify.success(device->name + ": extern abgeschlossen. Jetzt in \"Service Control\" fertigmelden.");
  // the local checklist is gone: reopen the one in the device folder
  const std::string service = ctx.selectedService();
  ctx.selectService({});
  ctx.selectService(service);
}

} // namespace st::app
