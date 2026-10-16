#include "modules/history/history_module.h"

#include "framework/platform.h"
#include "framework/widgets.h"

#include <st/exporting/reports.h>
#include <st/exporting/xlsx.h>
#include <st/service/legacy_import.h>
#include <st/users/user_store.h>
#include <st/util/strings.h>

#include <imgui.h>

#include <algorithm>
#include <set>

namespace st::app {

namespace {
enum Column { ColDevice,
              ColService,
              ColInfo,
              ColServicePath,
              ColSyncPath,
              ColPrice,
              ColDuration,
              ColEnd,
              ColStatus,
              ColCount };

/// Year of a row: end date if present, otherwise the service date.
std::string yearOf(const service::HistoryEntry &row) {
  const std::string date = row.endDate.empty() ? service::ServiceManager::serviceDate(row.record.activeService) : row.endDate;
  return date.size() >= 4 ? date.substr(0, 4) : std::string();
}
} // namespace

bool HistoryModule::available(const AppContext &ctx) const {
  return ctx.syncAvailable() && (ctx.hasAccess(users::groups::Techsupport) || ctx.hasAccess(users::groups::Sales));
}

void HistoryModule::init(AppContext &ctx) {
  mSource = ctx.paths().historyFile();
  mPoll.setInterval(std::chrono::milliseconds(std::max(1000, ctx.settings().refreshIntervalMs * 2)));
  ctx.ui.addWindow("Service Historie", [this, &ctx] { drawWindow(ctx); }, true, ImGuiWindowFlags_HorizontalScrollbar);
  reload(ctx);
}

void HistoryModule::update(AppContext &ctx) {
  if (mPoll.due() && util::lastWriteTime(mSource) != mSourceTime)
    reload(ctx);
}

std::vector<util::fs::path> HistoryModule::sources(const AppContext &ctx) const {
  std::vector<util::fs::path> out = {ctx.paths().historyFile()};
  if (util::pathExists(ctx.paths().externHistoryFile()))
    out.push_back(ctx.paths().externHistoryFile());
  out.push_back(ctx.paths().syncFile());
  if (const auto externFile = util::pathFromUtf8(ctx.paths().externFile); util::pathExists(externFile))
    out.push_back(externFile);
  for (const auto &file : util::listDirectory(ctx.paths().oldHistoryDir(), false, true))
    if (util::toLower(util::pathToUtf8(file.extension())) == ".json")
      out.push_back(file);
  return out;
}

void HistoryModule::reload(AppContext &ctx) {
  mSourceTime = util::lastWriteTime(mSource);
  auto rows = service::loadHistoryRows(mSource);
  if (!rows) {
    ctx.notify.error(rows.error().message);
    mRows.clear();
  } else {
    mRows = std::move(*rows);
  }
  sortRows(mSortColumn, mSortAscending);
  applyFilter();
}

void HistoryModule::applyFilter() {
  mVisible.clear();
  for (std::size_t i = 0; i < mRows.size(); ++i) {
    const auto &r = mRows[i];
    if (!mYear.empty() && yearOf(r) != mYear)
      continue;
    if (!mSearch.empty()) {
      const bool match = util::containsIgnoreCase(r.device, mSearch) || util::containsIgnoreCase(r.record.activeService, mSearch) ||
                         util::containsIgnoreCase(r.record.additionalInfo, mSearch) || util::containsIgnoreCase(r.record.status, mSearch) ||
                         util::containsIgnoreCase(r.servicePath(), mSearch) || util::containsIgnoreCase(r.endDate, mSearch);
      if (!match)
        continue;
    }
    mVisible.push_back(i);
  }
}

void HistoryModule::sortRows(int column, bool ascending) {
  mSortColumn = column;
  mSortAscending = ascending;
  std::stable_sort(mRows.begin(), mRows.end(), [&](const service::HistoryEntry &a, const service::HistoryEntry &b) {
    int cmp = 0;
    switch (column) {
    case ColDevice: cmp = a.device.compare(b.device); break;
    case ColService: cmp = a.record.activeService.compare(b.record.activeService); break;
    case ColInfo: cmp = a.record.additionalInfo.compare(b.record.additionalInfo); break;
    case ColServicePath: cmp = a.servicePath().compare(b.servicePath()); break;
    case ColSyncPath: cmp = a.record.syncFolder.compare(b.record.syncFolder); break;
    case ColPrice: cmp = a.totalPrice < b.totalPrice ? -1 : (a.totalPrice > b.totalPrice ? 1 : 0); break;
    case ColDuration: cmp = a.record.duration < b.record.duration ? -1 : (a.record.duration > b.record.duration ? 1 : 0); break;
    case ColEnd: cmp = a.endDate.compare(b.endDate); break;
    case ColStatus: cmp = a.record.status.compare(b.record.status); break;
    default: break;
    }
    return ascending ? cmp < 0 : cmp > 0;
  });
}

void HistoryModule::drawWindow(AppContext &ctx) {
  // Source list
  ImGui::SetNextItemWidth(260.0f);
  if (ImGui::BeginCombo("Liste", util::pathToUtf8(mSource.filename()).c_str())) {
    for (const auto &source : sources(ctx)) {
      if (ImGui::Selectable(util::pathToUtf8(source.filename()).c_str(), source == mSource)) {
        mSource = source;
        reload(ctx);
      }
      ImGui::SetItemTooltip("%s", util::pathToUtf8(source).c_str());
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  // Year filter
  std::set<std::string> years;
  for (const auto &r : mRows)
    if (auto y = yearOf(r); !y.empty())
      years.insert(y);
  ImGui::SetNextItemWidth(110.0f);
  if (ImGui::BeginCombo("Jahr", mYear.empty() ? "Alle" : mYear.c_str())) {
    if (ImGui::Selectable("Alle", mYear.empty())) {
      mYear.clear();
      applyFilter();
    }
    for (auto it = years.rbegin(); it != years.rend(); ++it)
      if (ImGui::Selectable(it->c_str(), *it == mYear)) {
        mYear = *it;
        applyFilter();
      }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  if (ImGui::Button("Jahresexport")) {
    mExportFile.clear();
    ImGui::OpenPopup("Jahresexport erstellen");
  }
  ImGui::SetItemTooltip("Excel-Auswertung der angezeigten Einträge (Jahresfilter beachten)");
  if (ctx.isPoweruser()) {
    ImGui::SameLine();
    if (ImGui::Button("Alte Liste konvertieren")) {
      const auto in = platform::openFileDialog({"Tabelle:csv"});
      if (!in.empty()) {
        const auto out = platform::saveFileDialog({"Historie:json"});
        const auto outPath = util::pathFromUtf8(out);
        if (!out.empty() && outPath != ctx.paths().historyFile() && outPath != ctx.paths().externHistoryFile() && outPath != ctx.paths().syncFile()) {
          if (auto csv = util::readFile(util::pathFromUtf8(in)); ctx.notify.check(csv, "CSV lesen")) {
            const auto json = service::convertLegacyServiceList(*csv, ctx.deviceRegistry);
            if (ctx.notify.check(util::json::save(outPath, json), "Konvertieren"))
              ctx.notify.success(std::to_string(json.size()) + " Geräte konvertiert");
          }
        } else if (!out.empty()) {
          ctx.notify.error("History.json und Sync.json dürfen nicht überschrieben werden");
        }
      }
    }
  }
  drawExportPopup(ctx);

  if (ui::searchField("histsearch", mSearch))
    applyFilter();
  ImGui::TextDisabled("%zu von %zu Einträgen", mVisible.size(), mRows.size());

  const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                                ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX;
  if (!ImGui::BeginTable("RegistryTable", ColCount, flags, ImVec2(0.0f, 0.0f)))
    return;
  ImGui::TableSetupColumn("Reader", ImGuiTableColumnFlags_DefaultSort, 0.0f, ColDevice);
  ImGui::TableSetupColumn("Service", 0, 0.0f, ColService);
  ImGui::TableSetupColumn("Info", 0, 0.0f, ColInfo);
  ImGui::TableSetupColumn("Service Pfad", 0, 0.0f, ColServicePath);
  ImGui::TableSetupColumn("Sync Pfad", ImGuiTableColumnFlags_DefaultHide, 0.0f, ColSyncPath);
  ImGui::TableSetupColumn("Preis", 0, 0.0f, ColPrice);
  ImGui::TableSetupColumn("Durchlaufzeit", 0, 0.0f, ColDuration);
  ImGui::TableSetupColumn("Abschlussdatum", 0, 0.0f, ColEnd);
  ImGui::TableSetupColumn("Status", 0, 0.0f, ColStatus);
  ImGui::TableSetupScrollFreeze(1, 1);
  ImGui::TableHeadersRow();

  if (ImGuiTableSortSpecs *specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsDirty && specs->SpecsCount > 0) {
    sortRows(static_cast<int>(specs->Specs[0].ColumnUserID), specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
    applyFilter();
    specs->SpecsDirty = false;
  }

  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(mVisible.size()));
  while (clipper.Step()) {
    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
      const auto &row = mRows[mVisible[static_cast<std::size_t>(i)]];
      ui::ScopedId id(i);
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(ColDevice);
      if (ImGui::Selectable(row.device.c_str()))
        ctx.selectDevice(row.device);
      ImGui::SetItemTooltip("Gerät auswählen");
      ImGui::TableSetColumnIndex(ColService);
      if (!row.record.activeService.empty() && ImGui::Selectable(row.record.activeService.c_str())) {
        ctx.selectDevice(row.device);
        ctx.selectService(row.record.activeService);
      }
      ImGui::TableSetColumnIndex(ColInfo);
      const std::string firstLine = util::splitAt(row.record.additionalInfo, "\n").first;
      if (ImGui::Selectable((firstLine + "##info").c_str()))
        ImGui::SetClipboardText(row.record.additionalInfo.c_str());
      if (!row.record.additionalInfo.empty())
        ImGui::SetItemTooltip("%s\n\n(Klicken zum Kopieren)", row.record.additionalInfo.c_str());
      ImGui::TableSetColumnIndex(ColServicePath);
      const std::string servicePath = row.servicePath();
      if (!servicePath.empty() && ImGui::Selectable((servicePath + "##sp").c_str()))
        platform::openPath(util::pathFromUtf8(servicePath));
      ImGui::TableSetColumnIndex(ColSyncPath);
      if (!row.record.syncFolder.empty() && ImGui::Selectable((row.record.syncFolder + "##sy").c_str()))
        platform::openPath(util::pathFromUtf8(row.record.syncFolder));
      ImGui::TableSetColumnIndex(ColPrice);
      ImGui::Text("%.2f EUR", row.totalPrice);
      ImGui::TableSetColumnIndex(ColDuration);
      ImGui::Text("%ld", row.record.duration);
      ImGui::TableSetColumnIndex(ColEnd);
      ImGui::TextUnformatted(row.endDate.c_str());
      ImGui::TableSetColumnIndex(ColStatus);
      ui::statusBadge(row.record.status);
    }
  }
  ImGui::EndTable();
}

void HistoryModule::drawExportPopup(AppContext &ctx) {
  if (!ImGui::BeginPopupModal("Jahresexport erstellen", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    return;
  ImGui::Text("Einträge: %zu (%s)", mVisible.size(), mYear.empty() ? "alle Jahre" : mYear.c_str());
  if (ImGui::Button("Ausgabedatei wählen...")) {
    mExportFile = platform::saveFileDialog({"Excel:xlsx"});
    if (!mExportFile.empty() && !mExportFile.ends_with(".xlsx"))
      mExportFile += ".xlsx";
  }
  ImGui::TextUnformatted(mExportFile.empty() ? "(keine Datei gewählt)" : mExportFile.c_str());
  if (!exporting::xlsxAvailable())
    ui::errorText("Excel-Export ist in diesem Build nicht verfügbar.");
  {
    ui::Disabled d(mExportFile.empty());
    if (ImGui::Button("Export erstellen")) {
      std::vector<service::HistoryEntry> rows;
      for (const auto i : mVisible)
        rows.push_back(mRows[i]);
      const auto file = util::pathFromUtf8(mExportFile);
      if (ctx.notify.check(exporting::writeXlsx(file, exporting::buildHistoryReport(rows)), "Jahresexport")) {
        platform::openPath(file);
        ImGui::CloseCurrentPopup();
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Schließen") || ImGui::IsKeyPressed(ImGuiKey_Escape))
    ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

} // namespace st::app
