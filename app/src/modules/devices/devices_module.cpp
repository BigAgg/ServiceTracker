#include "modules/devices/devices_module.h"

#include "framework/app_context.h"
#include "framework/events.h"
#include "framework/platform.h"
#include "framework/widgets.h"

#include <st/devices/device.h>
#include <st/util/strings.h>

#include <app/app.h>
#include <imgui.h>

namespace st::app {

namespace {

/// Notification channel id – stored in the preferences, never rename.
constexpr const char *kAlertDevicesReceived = "devices.received";

/// TECHSUPPORT gets a notification as soon as a device appears in the service list
/// with status "Wareneingang" (new device or e.g. an expected device that arrived).
alerts::Channel receivedChannel() {
  alerts::Channel channel;
  channel.id = kAlertDevicesReceived;
  channel.label = "Reader im Wareneingang";
  channel.title = "ServiceTracker - Wareneingang";
  channel.mode = alerts::Mode::PerItem; // one notification per device
  channel.itemText = [](const alerts::Item &item) { return "Neuer Reader im Wareneingang: " + item.label; };
  channel.summaryText = [](std::size_t n) { return alerts::countSentence(n, "Reader", "Reader", "im Wareneingang"); };
  channel.target = "Reader Control";
  channel.audience = {std::string(users::groups::Techsupport)};
  return channel;
}

} // namespace

void DevicesModule::init(AppContext &ctx) {
  ctx.ui.addWindow("Reader Control", [this, &ctx] { drawControl(ctx); }, true, ImGuiWindowFlags_HorizontalScrollbar);
  ctx.ui.addWindow("Reader Filewalker", [this, &ctx] { drawFilewalker(ctx); }, true, ImGuiWindowFlags_HorizontalScrollbar);
  if (ctx.deviceRegistry.hasAccess()) {
    ctx.ui.addWindow("ReaderInfo", [this, &ctx] { drawInfo(ctx); }, true, ImGuiWindowFlags_HorizontalScrollbar);
    ctx.ui.addWindow("Konfiguration", [this, &ctx] { drawConfiguration(ctx); }, true, ImGuiWindowFlags_HorizontalScrollbar);
  }
  ctx.events.subscribe<events::RecordsChanged>([this, &ctx](const auto &) {
    mDirty = true;
    reportReceived(ctx);
  });
  // Devices this user booked in himself are no news for him
  ctx.events.subscribe<events::RecordApplied>([&ctx](const events::RecordApplied &e) { ctx.alerts.markSeen(kAlertDevicesReceived, e.device); });
  // Click on "Neuer Reader im Wareneingang: <serial>" selects that device
  ctx.events.subscribe<events::AlertActivated>([&ctx](const events::AlertActivated &e) {
    if (e.channel == kAlertDevicesReceived && !e.item.empty() && ctx.deviceRegistry.find(e.item))
      ctx.selectDevice(e.item);
  });
  ctx.events.subscribe<events::DevicesRescanned>([this](const auto &) { mDirty = true; });
  ctx.events.subscribe<events::PreferencesChanged>([this](const auto &) { mDirty = true; });
  ctx.events.subscribe<events::DeviceSelected>([this, &ctx](const auto &) {
    const auto *device = ctx.activeDevice();
    mBrowseRoot = device ? device->path : util::fs::path();
    // Offline (external service at a customer): browse the local copy
    if (device && !util::isDirectory(device->path) && util::isDirectory(ctx.services().externCopyFor(*device)))
      mBrowseRoot = ctx.services().externCopyFor(*device);
    mBrowsePath = mBrowseRoot;
    mEntries.clear();
    if (!mBrowsePath.empty())
      mEntries = util::listDirectory(mBrowsePath);
  });
  if (ctx.alerts.addChannel(ctx, receivedChannel()))
    reportReceived(ctx);
}

void DevicesModule::reportReceived(AppContext &ctx) {
  std::vector<alerts::Item> items;
  for (auto &device : ctx.statusIndex().devicesWithStatus(service::status::Received))
    items.push_back({device, device});
  ctx.alerts.report(kAlertDevicesReceived, items);
}

void DevicesModule::refilter(AppContext &ctx) {
  mFilter.showClosed = ctx.prefs().showClosedDevices;
  mFiltered = service::filterDevices(ctx.deviceRegistry, ctx.statusIndex(), mFilter);
  mDirty = false;
}

void DevicesModule::openFolder(const util::fs::path &folder) {
  mBrowsePath = folder;
  mEntries = util::listDirectory(folder);
}

void DevicesModule::drawControl(AppContext &ctx) {
  if (mDirty)
    refilter(ctx);
  ImGui::PushItemWidth(-FLT_MIN);

  if (ui::searchField("devsearch", mFilter.text))
    mDirty = true;

  // Project filter
  ImGui::TextUnformatted("Nach Projekt");
  if (ImGui::BeginCombo("##project", mFilter.project.empty() ? "Alle" : mFilter.project.c_str())) {
    if (ImGui::Selectable("Alle", mFilter.project.empty())) {
      mFilter.project.clear();
      mDirty = true;
    }
    for (const auto &project : ctx.deviceRegistry.projects()) {
      if (ImGui::Selectable(project.c_str(), project == mFilter.project)) {
        mFilter.project = project;
        mDirty = true;
      }
    }
    ImGui::EndCombo();
  }

  // Status filter with counts
  ImGui::TextUnformatted("Nach Status");
  if (ImGui::BeginCombo("##status", mFilter.status.empty() ? "Alle" : mFilter.status.c_str())) {
    if (ImGui::Selectable("Alle", mFilter.status.empty())) {
      mFilter.status.clear();
      mDirty = true;
    }
    for (const auto &status : ctx.statusIndex().statuses()) {
      const std::string label = status + " (" + std::to_string(ctx.statusIndex().count(status)) + ")";
      ImGui::PushStyleColor(ImGuiCol_Text, ui::statusColor(status));
      if (ImGui::Selectable(label.c_str(), status == mFilter.status)) {
        mFilter.status = status;
        mDirty = true;
      }
      ImGui::PopStyleColor();
    }
    ImGui::EndCombo();
  }

  // Recently used devices
  if (!ctx.prefs().recentDevices.empty() && ImGui::BeginCombo("##recent", "Zuletzt verwendet...")) {
    for (const auto &name : ctx.prefs().recentDevices)
      if (ImGui::Selectable(name.c_str(), name == ctx.activeDeviceName()))
        ctx.selectDevice(name);
    ImGui::EndCombo();
  }
  ImGui::PopItemWidth();

  ImGui::Text("Reader Liste (%zu)", mFiltered.size());
  ImGui::SameLine();
  if (ImGui::SmallButton("Neu laden")) {
    ctx.deviceRegistry.scan(util::pathFromUtf8(ctx.paths().devicesRoot));
    ctx.reloadRecords();
    ctx.refreshActiveDevice();
    ctx.events.publish(events::DevicesRescanned{});
    ctx.notify.info(std::to_string(ctx.deviceRegistry.size()) + " Geräte eingelesen");
  }
  ImGui::SetItemTooltip("Geräteordner und Service-Liste neu einlesen");

  const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
  if (ImGui::BeginTable("##devices", 2, flags, ImVec2(0.0f, -FLT_MIN))) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Gerät", ImGuiTableColumnFlags_WidthStretch, 0.55f);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 0.45f);
    ImGui::TableHeadersRow();
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(mFiltered.size()));
    while (clipper.Step()) {
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
        const std::string &name = mFiltered[static_cast<std::size_t>(i)];
        ui::ScopedId id(i);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const bool selected = name == ctx.activeDeviceName();
        if (ImGui::Selectable(name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
          ctx.selectDevice(name);
          if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            if (const auto *device = ctx.activeDevice())
              platform::openPath(device->path);
        }
        if (ImGui::BeginPopupContextItem("##ctx")) {
          const auto *device = ctx.deviceRegistry.find(name);
          if (ImGui::MenuItem("Ordner öffnen", nullptr, false, device && !device->path.empty()))
            platform::openPath(device->path);
          if (ImGui::MenuItem("Name kopieren"))
            ImGui::SetClipboardText(name.c_str());
          if (ImGui::MenuItem("Seriennummer kopieren"))
            ImGui::SetClipboardText(devices::serialOf(name).c_str());
          ImGui::EndPopup();
        }
        ImGui::TableSetColumnIndex(1);
        ui::statusBadge(ctx.statusIndex().statusOf(name));
      }
    }
    ImGui::EndTable();
  }
}

void DevicesModule::drawFilewalker(AppContext &ctx) {
  const auto *device = ctx.activeDevice();
  if (!device || mBrowsePath.empty()) {
    ui::hintText("Gerät in \"Reader Control\" wählen.");
    return;
  }
  {
    ui::Disabled d(!mBrowsePath.has_parent_path() || mBrowsePath == mBrowsePath.root_path());
    if (ImGui::ArrowButton("##up", ImGuiDir_Left))
      openFolder(mBrowsePath.parent_path());
  }
  ImGui::SetItemTooltip("Übergeordneter Ordner");
  ImGui::SameLine();
  {
    ui::Disabled d(mBrowsePath == mBrowseRoot);
    if (ImGui::Button("Zum Gerät"))
      openFolder(mBrowseRoot);
  }
  ImGui::SameLine();
  if (ImGui::Button("Aktualisieren"))
    openFolder(mBrowsePath);
  ImGui::SameLine();
  if (ImGui::Button("Im Explorer öffnen"))
    platform::openPath(mBrowsePath);
  ImGui::TextUnformatted(util::pathToUtf8(mBrowsePath.filename()).c_str());
  ImGui::SetItemTooltip("%s", util::pathToUtf8(mBrowsePath).c_str());

  ui::searchField("filesearch", mFileSearch);
  if (ImGui::BeginListBox("##files", ImVec2(-FLT_MIN, -FLT_MIN))) {
    util::fs::path navigateTo;
    for (const auto &entry : mEntries) {
      const std::string fileName = util::pathToUtf8(entry.filename());
      if (!util::containsIgnoreCase(fileName, mFileSearch))
        continue;
      const bool isDir = util::isDirectory(entry);
      const std::string label = (isDir ? "[+] " : "    ") + fileName;
      ui::ScopedId id(fileName.c_str());
      if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
        if (isDir)
          navigateTo = entry;
        else if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
          platform::openPath(entry);
      }
      ImGui::SetItemTooltip("%s", isDir ? "Klicken zum Öffnen" : "Doppelklick zum Öffnen");
      if (ImGui::BeginPopupContextItem("##fctx")) {
        if (ImGui::MenuItem("Öffnen"))
          platform::openPath(entry);
        if (ImGui::MenuItem("Im Explorer zeigen"))
          platform::revealInExplorer(entry);
        if (ImGui::MenuItem("Pfad kopieren"))
          ImGui::SetClipboardText(util::pathToUtf8(entry).c_str());
        ImGui::EndPopup();
      }
    }
    ImGui::EndListBox();
    if (!navigateTo.empty())
      openFolder(navigateTo);
  }

  // Drag & drop: copy files into the current folder
  if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ::app::HasDropfiles()) {
    int copied = 0;
    for (const auto &file : ::app::GetDropfiles()) {
      const auto source = util::pathFromUtf8(file);
      const auto target = mBrowsePath / source.filename();
      const auto status = util::isDirectory(source) ? util::copyTree(source, target) : util::copyFile(source, target);
      if (ctx.notify.check(status, "Kopieren"))
        ++copied;
    }
    if (copied > 0)
      ctx.notify.success(std::to_string(copied) + " Datei(en) kopiert nach " + util::pathToUtf8(mBrowsePath.filename()));
    openFolder(mBrowsePath);
  }
}

void DevicesModule::drawInfo(AppContext &ctx) {
  const auto *device = ctx.activeDevice();
  if (!device) {
    ui::hintText("Kein Gerät gewählt.");
    return;
  }
  if (ImGui::Button("Neu laden"))
    ctx.refreshActiveDevice();
  const auto &i = device->info;
  if (ImGui::BeginTable("##info", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
    const auto row = [](const char *label, const std::string &value) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(label);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(value.c_str());
      ui::copyContextMenu(label, value);
    };
    row("Datum", ctx.displayTime(i.fileDate));
    row("Reader active time", i.readerActiveTime);
    row("Executed testruns", std::to_string(i.executedTestruns));
    row("Processed wells", std::to_string(i.processedWells));
    row("Well movements", std::to_string(i.wellMovements));
    row("Shaking active time", i.shakingTime);
    row("Performed flashes (flash lamp)", std::to_string(i.performedFlashesLamp));
    row("Performed flashes (TRF laser)", std::to_string(i.performedFlashesLaser));
    row("AlphaScreen laser active time", i.alphascreenLaserActiveTime);
    row("Pump 1 strokes", std::to_string(i.pump1Strokes));
    row("Pump 2 strokes", std::to_string(i.pump2Strokes));
    ImGui::EndTable();
  }
}

void DevicesModule::drawConfiguration(AppContext &ctx) {
  const auto *device = ctx.activeDevice();
  if (!device) {
    ui::hintText("Kein Gerät gewählt.");
    return;
  }
  if (!ctx.configStore.find(device->project()))
    ui::hintText("Für dieses Projekt gibt es keine Konfiguration.");
  ui::Disabled d(true);
  for (const auto &flag : devices::kFlags) {
    bool active = devices::hasFlag(device->flags, flag.flag);
    ImGui::Checkbox(std::string(flag.label).c_str(), &active);
  }
}

} // namespace st::app
