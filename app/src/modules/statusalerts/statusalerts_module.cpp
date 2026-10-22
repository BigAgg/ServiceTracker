#include "modules/statusalerts/statusalerts_module.h"

#include "framework/events.h"

#include <st/alerts/alert_center.h>
#include <st/service/records.h>
#include <st/users/user_store.h>
#include <st/util/strings.h>
#include <st/util/suggestions.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>

namespace st::app {

namespace {

constexpr const char *kEditorWindow = "Status-Benachrichtigungen";

alerts::Channel channelFor(const alerts::StatusRule &rule) {
  alerts::Channel channel;
  channel.id = alerts::statusChannelId(rule.status);
  channel.label = "Status „" + rule.status + "“";
  channel.title = "ServiceTracker - " + rule.status;
  channel.mode = alerts::Mode::PerItem;
  const std::string status = rule.status;
  channel.itemText = [status](const alerts::Item &item) { return item.label + ": Status „" + status + "“"; };
  channel.summaryText = [status](std::size_t n) { return alerts::countSentence(n, "Reader", "Reader", "mit Status „" + status + "“"); };
  channel.target = "Reader Control";
  channel.audience = rule.groups;
  return channel;
}

bool isStatusChannel(std::string_view id) { return id.starts_with("status."); }

} // namespace

void StatusAlertsModule::init(AppContext &ctx) {
  mStore = alerts::StatusRuleStore(util::pathFromUtf8(ctx.paths().configsDir) / "statusAlerts.json");
  mPoll.setInterval(std::chrono::milliseconds(std::max(1000, ctx.settings().refreshIntervalMs * 5)));
  if (util::isDirectory(mStore.file().parent_path()))
    ctx.notify.check(mStore.load(), "Status-Benachrichtigungen");
  registerChannels(ctx);
  report(ctx);

  ctx.events.subscribe<events::RecordsChanged>([this, &ctx](const auto &) { report(ctx); });
  // Status changes made by this user are no news for him
  ctx.events.subscribe<events::RecordApplied>([this, &ctx](const events::RecordApplied &e) {
    for (const auto &id : mChannels)
      ctx.alerts.markSeen(id, e.device);
  });
  // Click on "<Gerät>: Status ..." selects the device
  ctx.events.subscribe<events::AlertActivated>([&ctx](const events::AlertActivated &e) {
    if (isStatusChannel(e.channel) && !e.item.empty() && ctx.deviceRegistry.find(e.item))
      ctx.selectDevice(e.item);
  });
  ctx.events.subscribe<events::ShowStatusAlertsEditor>([this, &ctx](const auto &) { show(ctx); });
}

void StatusAlertsModule::update(AppContext &ctx) {
  // Rules edited by a power user on another PC
  if (!mPoll.due() || !util::isDirectory(mStore.file().parent_path()) || !mStore.changedOnDisk())
    return;
  if (ctx.notify.check(mStore.load(), "Status-Benachrichtigungen")) {
    registerChannels(ctx);
    report(ctx);
  }
}

void StatusAlertsModule::registerChannels(AppContext &ctx) {
  std::vector<std::string> wanted;
  for (const auto &rule : mStore.rules())
    wanted.push_back(alerts::statusChannelId(rule.status));
  for (const auto &id : mChannels)
    if (std::find(wanted.begin(), wanted.end(), id) == wanted.end())
      ctx.alerts.removeChannel(id);
  mChannels.clear();
  for (const auto &rule : mStore.rules()) {
    const auto id = alerts::statusChannelId(rule.status);
    // A changed audience needs a new registration
    ctx.alerts.removeChannel(id);
    if (ctx.alerts.addChannel(ctx, channelFor(rule)))
      mChannels.push_back(id);
  }
}

void StatusAlertsModule::report(AppContext &ctx) {
  const auto &index = ctx.statusIndex();
  for (const auto &rule : mStore.rules()) {
    const auto id = alerts::statusChannelId(rule.status);
    if (!ctx.alerts.subscribed(id))
      continue;
    const std::string wanted = util::foldCase(rule.status);
    std::vector<alerts::Item> items;
    for (const auto &device : index.devices())
      if (util::foldCase(index.statusOf(device)) == wanted)
        items.push_back({device, device});
    ctx.alerts.report(id, items);
  }
}

void StatusAlertsModule::show(const AppContext &ctx) {
  (void)ctx;
  mDraft = mStore.rules();
  mStatusInput.assign(mDraft.size(), {});
  mDirty = false;
  mOpen = true;
}

void StatusAlertsModule::drawOverlay(AppContext &ctx) {
  if (!mOpen)
    return;
  ImGui::SetNextWindowSize(ImVec2(720.0f, 420.0f), ImGuiCond_FirstUseEver);
  if (ImGui::Begin(kEditorWindow, &mOpen, ImGuiWindowFlags_NoDocking | (mDirty ? ImGuiWindowFlags_UnsavedDocument : 0)))
    drawEditor(ctx);
  ImGui::End();
}

void StatusAlertsModule::drawEditor(AppContext &ctx) {
  ui::hintText("Benachrichtigt die gewählten Abteilungen, sobald ein Gerät mit diesem Status in der Liste erscheint.");
  ui::hintText("Datei: " + util::pathToUtf8(mStore.file()));

  // Status suggestions: statuses currently in use + the program statuses
  std::vector<std::string> statuses = ctx.statusIndex().statuses();
  for (const auto s : {service::status::Received, service::status::ExternFinished, service::status::Done})
    if (std::find(statuses.begin(), statuses.end(), s) == statuses.end())
      statuses.emplace_back(s);
  std::vector<std::string> groups = ctx.userStore.knownGroups();
  for (const auto g : {users::groups::Techsupport, users::groups::Sales})
    if (std::find(groups.begin(), groups.end(), g) == groups.end())
      groups.emplace_back(g);
  std::sort(groups.begin(), groups.end());

  std::size_t remove = mDraft.size();
  if (ImGui::BeginTable("##rules", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("Status");
    ImGui::TableSetupColumn("Abteilungen");
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30.0f);
    ImGui::TableHeadersRow();
    for (std::size_t i = 0; i < mDraft.size(); ++i) {
      auto &rule = mDraft[i];
      ui::ScopedId id(static_cast<int>(i));
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::SetNextItemWidth(-FLT_MIN);
      mDirty |= ui::autocompleteInput("##status", rule.status, statuses, mStatusInput[i]);
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(-FLT_MIN);
      const std::string preview = rule.groups.empty() ? std::string("keine") : util::join(rule.groups, ", ");
      if (ImGui::BeginCombo("##groups", preview.c_str())) {
        for (const auto &group : groups) {
          const auto it = std::find(rule.groups.begin(), rule.groups.end(), group);
          bool selected = it != rule.groups.end();
          if (ImGui::Checkbox(group.c_str(), &selected)) {
            if (selected)
              rule.groups.push_back(group);
            else
              rule.groups.erase(it);
            mDirty = true;
          }
        }
        ImGui::EndCombo();
      }
      ImGui::TableSetColumnIndex(2);
      if (ImGui::SmallButton("x"))
        remove = i;
      ImGui::SetItemTooltip("Regel entfernen");
    }
    ImGui::EndTable();
  }
  if (remove < mDraft.size()) {
    mDraft.erase(mDraft.begin() + static_cast<std::ptrdiff_t>(remove));
    mStatusInput.erase(mStatusInput.begin() + static_cast<std::ptrdiff_t>(remove));
    mDirty = true;
  }
  if (ImGui::Button("Regel hinzufügen")) {
    mDraft.push_back({"", {std::string(users::groups::Techsupport)}});
    mStatusInput.emplace_back().refocus = true;
    mDirty = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Standard wiederherstellen")) {
    mDraft = alerts::defaultStatusRules();
    mStatusInput.assign(mDraft.size(), {});
    mDirty = true;
  }
  ImGui::SetItemTooltip("Ersetzt die Liste durch die mitgelieferten Regeln (erst mit \"Speichern\" wirksam)");

  ImGui::Separator();
  {
    ui::Disabled d(!mDirty);
    if (ImGui::Button("Speichern")) {
      if (ctx.notify.check(mStore.save(mDraft), "Status-Benachrichtigungen speichern")) {
        mDraft = mStore.rules();
        mStatusInput.assign(mDraft.size(), {});
        mDirty = false;
        registerChannels(ctx);
        report(ctx);
        ctx.notify.success("Status-Benachrichtigungen gespeichert");
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Verwerfen"))
      show(ctx);
  }
  ImGui::SameLine();
  ui::hintText("Regeln ohne Status oder Abteilung werden beim Speichern entfernt, gleiche Status zusammengefasst.");
}

} // namespace st::app
