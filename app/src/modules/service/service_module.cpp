#include "modules/service/service_module.h"

#include "framework/app_context.h"
#include "framework/events.h"
#include "framework/platform.h"
#include "framework/widgets.h"

#include <st/checklist/actions.h>
#include <st/users/user_store.h>
#include <st/util/log.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace st::app {

using checklist::Task;
using checklist::Text;

checklist::HtmlOptions ServiceModule::htmlOptions(const AppContext &ctx) {
  checklist::HtmlOptions options;
  options.appVersion = APP_VERSION;
  options.title = ctx.settings().recordTitle;
  options.footer = ctx.settings().recordFooter;
  const auto styleFile = ctx.paths().styleFile();
  if (util::pathExists(styleFile))
    if (auto content = util::readFile(styleFile))
      options.customHead = std::move(*content);
  return options;
}

void ServiceModule::init(AppContext &ctx) {
  registerExpressions(ctx);
  ctx.ui.addWindow("Service Control", [this, &ctx] { drawWindow(ctx); }, true, ImGuiWindowFlags_HorizontalScrollbar);
}

void ServiceModule::registerExpressions(AppContext &ctx) {
  auto &ex = ctx.expressions;
  const auto device = [&ctx]() -> const devices::Device * { return ctx.activeDevice(); };
  const auto info = [device]() { return device() ? device()->info : devices::DeviceInfo{}; };

  ex.add("@rinfo.readerinfo.reader_active_time", [info](Task &t) { t.value = Text{info().readerActiveTime}; }, "Reader active time");
  ex.add("@rinfo.readerinfo.executed_testruns", [info](Task &t) { t.value = info().executedTestruns; }, "Executed testruns");
  ex.add("@rinfo.readerinfo.processed_wells", [info](Task &t) { t.value = info().processedWells; }, "Processed wells");
  ex.add("@rinfo.readerinfo.well_movements", [info](Task &t) { t.value = info().wellMovements; }, "Well movements");
  ex.add("@rinfo.readerinfo.shaking_time", [info](Task &t) { t.value = Text{info().shakingTime}; }, "Shaking active time");
  ex.add("@rinfo.readerinfo.performed_flashes_lamp", [info](Task &t) { t.value = info().performedFlashesLamp; }, "Flashes (flash lamp)");
  ex.add("@rinfo.readerinfo.performed_flashes_laser", [info](Task &t) { t.value = info().performedFlashesLaser; }, "Flashes (TRF laser)");
  ex.add("@rinfo.readerinfo.alphascreen_laser_active_time", [info](Task &t) { t.value = Text{info().alphascreenLaserActiveTime}; }, "AlphaScreen laser time");
  ex.add("@rinfo.readerinfo.pump_1_strokes", [info](Task &t) { t.value = info().pump1Strokes; }, "Pump 1 strokes");
  ex.add("@rinfo.readerinfo.pump_2_strokes", [info](Task &t) { t.value = info().pump2Strokes; }, "Pump 2 strokes");
  for (const auto &flag : devices::kFlags) {
    const auto f = flag.flag;
    ex.add("@rinfo.config." + std::string(flag.key), [device, f](Task &t) { t.value = device() && devices::hasFlag(device()->flags, f); },
           "Gerät hat " + std::string(flag.label));
  }
  ex.add("@rinfo.project", [device](Task &t) { t.value = Text{device() ? device()->project() : std::string()}; }, "Projektnummer");
  ex.add("@rinfo.serial", [device](Task &t) { t.value = Text{device() ? device()->serial() : std::string()}; }, "Seriennummer");
  ex.add("@rinfo.path", [device](Task &t) { t.value = Text{device() ? util::pathToUtf8(device()->path) : std::string()}; }, "Geräteordner");
  ex.add("@rinfo.activeService", [&ctx](Task &t) { t.value = Text{ctx.selectedService()}; }, "Gewählter Service");

  ctx.sectionActions.add(
      std::string(service::kUpdateStatusAction),
      [&ctx](checklist::SectionActionContext &a) {
        auto *dev = ctx.activeDevice();
        if (!dev || a.section.postActionArgs.empty())
          return;
        if (!service::shouldAdvanceStatus(a.checklist, a.section, ctx.activeRecord().status))
          return;
        auto record = ctx.services().setStatus(*dev, a.section.postActionArgs);
        if (ctx.notify.check(record, "Status setzen")) {
          ctx.applyRecord(*record);
          ctx.notify.info(dev->name + ": Status \"" + record->status + "\"");
        }
      },
      "Setzt den Gerätestatus auf die Post-Action-Argumente (nur vorwärts)");
}

void ServiceModule::drawWindow(AppContext &ctx) {
  auto *device = ctx.activeDevice();
  const auto &record = ctx.activeRecord();
  if (!device) {
    ui::hintText("Gerät in \"Reader Control\" wählen.");
    return;
  }
  if (mInfoBufferDevice != device->name || ImGui::IsWindowAppearing()) {
    mInfoBufferDevice = device->name;
    mInfoBuffer = record.additionalInfo;
  }

  ImGui::Text("%s", device->name.c_str());
  ImGui::SameLine();
  ui::statusBadge(record.status);

  ImGui::PushItemWidth(-FLT_MIN);
  ImGui::TextUnformatted("Services");
  const float listHeight = 6.0f * ImGui::GetTextLineHeightWithSpacing();
  if (ImGui::BeginListBox("##services", ImVec2(-FLT_MIN, listHeight))) {
    for (auto it = record.services.rbegin(); it != record.services.rend(); ++it) { // newest first
      const bool active = *it == record.activeService;
      const std::string label = *it + (active ? "  (aktiv)" : "");
      if (ImGui::Selectable(label.c_str(), *it == ctx.selectedService(), ImGuiSelectableFlags_AllowDoubleClick)) {
        ctx.selectService(*it);
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
          platform::openPath(device->path / util::pathFromUtf8(*it));
      }
      ImGui::SetItemTooltip("Doppelklick öffnet den Ordner");
    }
    ImGui::EndListBox();
  }
  ImGui::PopItemWidth();

  const bool access = ctx.hasAccess(users::groups::ServiceCreator);
  const bool running = !record.syncFolder.empty() || !record.activeService.empty();
  {
    ui::Disabled d(!access || running || !ctx.syncAvailable());
    if (ImGui::Button("Service erstellen")) {
      auto created = ctx.services().createService(*device, ctx.session());
      if (ctx.notify.check(created, "Service erstellen")) {
        ctx.applyRecord(*created);
        ctx.events.publish(events::DevicesRescanned{});
        ctx.notify.success("Service " + created->activeService + " angelegt");
      }
    }
  }
  if (!access)
    ImGui::SetItemTooltip("Benötigt die Berechtigung SERVICE_CREATOR");
  ImGui::SameLine();
  {
    ui::Disabled d(!access || !service::isClosedStatus(record.status));
    if (ImGui::Button("Erwarten")) {
      mExpectInfo = kExpectTemplate;
      ImGui::OpenPopup("Gerät erwarten");
    }
  }
  ImGui::SetItemTooltip("Gerät mit Zusatzinformationen als \"erwartet\" in die Liste aufnehmen");
  ImGui::SameLine();
  if (ui::confirmButton("Aus Liste nehmen", "Gerät wirklich aus der Liste entfernen?", !access || record.status.empty())) {
    if (ctx.notify.check(ctx.services().removeFromList(*device), "Aus Liste nehmen")) {
      auto updated = record;
      updated.status.clear();
      ctx.applyRecord(updated);
    }
  }
  {
    namespace s = service::status;
    const bool canFinish = access && !record.activeService.empty() && record.status != s::Done && record.status != s::Expected &&
                           record.status != s::DoneExternal && record.status != s::PlannedExternal;
    ui::Disabled d(!canFinish);
    if (ImGui::Button("Service fertigmelden"))
      ImGui::OpenPopup("Service fertigmelden");
  }

  // Manual status (power users)
  if (ctx.isPoweruser() && !record.status.empty()) {
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("##manualstatus", "Status setzen...")) {
      for (const auto &s : ctx.statusIndex().statuses())
        if (ImGui::Selectable(s.c_str(), s == record.status))
          if (auto r = ctx.services().setStatus(*device, s); ctx.notify.check(r))
            ctx.applyRecord(*r);
      ImGui::SetNextItemWidth(150.0f);
      ImGui::InputTextWithHint("##newstatus", "neuer Status", &mManualStatus);
      ImGui::SameLine();
      if (ImGui::SmallButton("Setzen") && !mManualStatus.empty()) {
        if (auto r = ctx.services().setStatus(*device, mManualStatus); ctx.notify.check(r))
          ctx.applyRecord(*r);
        mManualStatus.clear();
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndCombo();
    }
  }

  ImGui::SeparatorText("Zusatzinformationen");
  if (ctx.hasAccess(users::groups::Techsupport) && !record.status.empty()) {
    ui::inputMultiline("##additionalinfo", mInfoBuffer, 6.0f);
    ui::Disabled d(mInfoBuffer == record.additionalInfo);
    if (ImGui::Button("Änderungen übernehmen")) {
      auto updated = ctx.services().setAdditionalInfo(*device, mInfoBuffer);
      if (ctx.notify.check(updated, "Zusatzinformationen"))
        ctx.applyRecord(*updated);
    }
    ImGui::SameLine();
    if (ImGui::Button("Verwerfen"))
      mInfoBuffer = record.additionalInfo;
  } else {
    std::string readonly = record.additionalInfo;
    ui::inputMultiline("##additionalinfo", readonly, 6.0f, ImGuiInputTextFlags_ReadOnly);
  }

  drawExpectPopup(ctx);
  drawFinishPopup(ctx);
}

void ServiceModule::drawExpectPopup(AppContext &ctx) {
  ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Appearing);
  if (!ImGui::BeginPopupModal("Gerät erwarten", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    return;
  ImGui::TextUnformatted("Zusatzinformationen");
  ui::inputMultiline("##expectinfo", mExpectInfo, 7.0f);
  if (ImGui::Button("Übernehmen")) {
    if (auto *device = ctx.activeDevice()) {
      auto updated = ctx.services().expect(*device, mExpectInfo);
      if (ctx.notify.check(updated, "Erwarten")) {
        ctx.applyRecord(*updated);
        mInfoBuffer = updated->additionalInfo;
      }
    }
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Verwerfen") || ImGui::IsKeyPressed(ImGuiKey_Escape))
    ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

void ServiceModule::drawFinishPopup(AppContext &ctx) {
  if (!ImGui::BeginPopupModal("Service fertigmelden", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    return;
  ImGui::Text("%s wirklich fertigmelden?", ctx.activeDeviceName().c_str());
  ImGui::Separator();
  ImGui::TextUnformatted("Nach dem Fertigmelden können keine Änderungen mehr\n"
                         "in der Checkliste vorgenommen werden. Das Gerät wird\n"
                         "als \"erledigt\" markiert und der Historie hinzugefügt.");
  if (!ctx.checklist().isOpen())
    ui::errorText("Keine Checkliste geöffnet – es wird kein Service Record erstellt.");
  if (ImGui::Button("Fertigmelden")) {
    finishService(ctx);
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Abbrechen") || ImGui::IsKeyPressed(ImGuiKey_Escape))
    ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

void ServiceModule::finishService(AppContext &ctx) {
  auto *device = ctx.activeDevice();
  if (!device)
    return;
  auto &session = ctx.checklist();
  const checklist::Checklist *data = nullptr;
  if (session.doc) {
    checklist::finishChecklist(session.doc->data(), ctx.expressions, ctx.sectionActions, ctx.session());
    if (!ctx.notify.check(session.doc->save(), "Checkliste speichern"))
      return;
    session.dirty = false;
    data = &session.doc->data();
  }
  auto result = ctx.services().finishService(*device, data, htmlOptions(ctx), ctx.session());
  if (!ctx.notify.check(result, "Service fertigmelden"))
    return;
  ctx.applyRecord(result->record);
  ctx.notify.success(device->name + ": Service abgeschlossen");
  if (!result->reportFile.empty())
    platform::openPath(result->reportFile);
  // reopen the checklist from the device folder
  const std::string service = ctx.selectedService();
  ctx.selectService({});
  ctx.selectService(service);
}

} // namespace st::app
