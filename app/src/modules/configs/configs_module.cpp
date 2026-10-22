#include "modules/configs/configs_module.h"

#include "framework/app_context.h"
#include "framework/events.h"
#include "framework/platform.h"
#include "framework/widgets.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>

namespace st::app {

namespace {

/// Combo with all flags (single flag selection).
bool flagCombo(const char *label, devices::ConfigFlags &flag) {
  bool changed = false;
  if (ImGui::BeginCombo(label, std::string(devices::flagLabel(flag)).c_str())) {
    if (ImGui::Selectable("None", flag == 0)) {
      flag = 0;
      changed = true;
    }
    for (const auto &info : devices::kFlags) {
      const auto value = static_cast<devices::ConfigFlags>(info.flag);
      if (ImGui::Selectable(std::string(info.label).c_str(), value == flag)) {
        flag = value;
        changed = true;
      }
    }
    ImGui::EndCombo();
  }
  return changed;
}

/// Editable list of folder names. Returns true when changed.
bool folderList(const char *id, std::vector<std::string> &folders, std::string &input) {
  ui::ScopedId scope(id);
  bool changed = false;
  ImGui::SetNextItemWidth(250.0f);
  const bool enter = ImGui::InputTextWithHint("##new", "Ordnername", &input, ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SameLine();
  const bool valid = !input.empty() && std::find(folders.begin(), folders.end(), input) == folders.end();
  {
    ui::Disabled d(!valid);
    if ((ImGui::Button("Hinzufügen") || enter) && valid) {
      folders.push_back(input);
      input.clear();
      changed = true;
    }
  }
  for (std::size_t i = 0; i < folders.size(); ++i) {
    ui::ScopedId fid(static_cast<int>(i));
    if (ImGui::SmallButton("x")) {
      folders.erase(folders.begin() + static_cast<std::ptrdiff_t>(i));
      changed = true;
      break;
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(folders[i].c_str());
  }
  return changed;
}

/// Editable list of template files (source + target name). Returns true when changed.
bool templateList(const char *id, std::vector<devices::TemplateFile> &files) {
  ui::ScopedId scope(id);
  bool changed = false;
  std::size_t remove = files.size();
  if (!files.empty() && ImGui::BeginTable("##files", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("Quelle");
    ImGui::TableSetupColumn("Zielname");
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30.0f);
    ImGui::TableHeadersRow();
    for (std::size_t i = 0; i < files.size(); ++i) {
      ui::ScopedId fid(static_cast<int>(i));
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(files[i].path.c_str());
      if (!util::pathExists(util::pathFromUtf8(files[i].path)))
        ui::errorText("Datei nicht gefunden");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(-FLT_MIN);
      changed |= ImGui::InputText("##rename", &files[i].rename);
      ImGui::TableSetColumnIndex(2);
      if (ImGui::SmallButton("x"))
        remove = i;
      ImGui::SetItemTooltip("Vorlage entfernen");
    }
    ImGui::EndTable();
  }
  if (remove < files.size()) {
    files.erase(files.begin() + static_cast<std::ptrdiff_t>(remove));
    changed = true;
  }
  if (ImGui::Button("Datei hinzufügen...")) {
    const auto path = platform::openFileDialog();
    if (!path.empty()) {
      const auto fileName = util::pathToUtf8(util::pathFromUtf8(path).filename());
      files.push_back({path, "{reader}_" + fileName});
      changed = true;
    }
  }
  return changed;
}

} // namespace

bool ConfigsModule::available(const AppContext &ctx) const { return ctx.isPoweruser(); }

void ConfigsModule::init(AppContext &ctx) {
  ctx.ui.addMenu("Configs", [this, &ctx] {
    if (ImGui::MenuItem("Configs bearbeiten..."))
      mOpen = true;
    if (ImGui::MenuItem("Status-Benachrichtigungen..."))
      ctx.events.publish(events::ShowStatusAlertsEditor{});
    if (ImGui::MenuItem("configs.json neu laden"))
      ctx.notify.check(ctx.configStore.load(), "configs.json");
    if (ImGui::MenuItem("Ordner öffnen"))
      platform::openPath(util::pathFromUtf8(ctx.paths().configsDir));
  });
}

void ConfigsModule::select(AppContext &ctx, const std::string &project) {
  mSelected = project;
  const auto *config = ctx.configStore.find(project);
  mDraft = config ? *config : devices::DeviceConfig{};
  mDirty = false;
}

void ConfigsModule::drawOverlay(AppContext &ctx) {
  if (!mOpen)
    return;
  if (!mPendingSelect.empty()) {
    select(ctx, mPendingSelect);
    mPendingSelect.clear();
  }
  ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Reader Configs", &mOpen, ImGuiWindowFlags_NoDocking | (mDirty ? ImGuiWindowFlags_UnsavedDocument : 0)))
    drawEditor(ctx);
  ImGui::End();
}

void ConfigsModule::drawEditor(AppContext &ctx) {
  // Left: project list
  ImGui::BeginChild("##projects", ImVec2(180.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
  ImGui::SetNextItemWidth(-FLT_MIN);
  const bool enter = ImGui::InputTextWithHint("##newproject", "Neues Projekt (z.B. 0601)", &mNewProject, ImGuiInputTextFlags_EnterReturnsTrue);
  const bool exists = ctx.configStore.find(mNewProject) != nullptr;
  {
    ui::Disabled d(mNewProject.empty() || exists);
    if ((ImGui::Button("Anlegen") || enter) && !mNewProject.empty() && !exists) {
      devices::DeviceConfig config;
      config.project = mNewProject;
      if (ctx.notify.check(ctx.configStore.upsert(config), "Config anlegen"))
        select(ctx, mNewProject);
      mNewProject.clear();
    }
  }
  ImGui::Separator();
  for (const auto &project : ctx.configStore.projects()) {
    if (ImGui::Selectable(project.c_str(), project == mSelected)) {
      if (mDirty)
        ctx.notify.warning("Ungespeicherte Änderungen an " + mSelected + " verworfen");
      select(ctx, project);
    }
  }
  ImGui::EndChild();
  ImGui::SameLine();

  // Right: editor
  ImGui::BeginChild("##editor", ImVec2(0.0f, 0.0f));
  if (mSelected.empty()) {
    ui::hintText("Projekt links wählen oder neu anlegen.");
    ImGui::EndChild();
    return;
  }
  auto &c = mDraft;
  ImGui::Text("Projekt %s", c.project.c_str());
  ImGui::SameLine();
  {
    ui::Disabled d(!mDirty);
    if (ImGui::Button("Speichern")) {
      if (ctx.notify.check(ctx.configStore.upsert(c), "Config speichern")) {
        mDirty = false;
        ctx.refreshActiveDevice();
        ctx.notify.success("Config " + c.project + " gespeichert");
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Verwerfen"))
      select(ctx, mSelected);
  }
  ImGui::SameLine();
  if (ui::confirmButton("Config löschen", "Config wirklich löschen?")) {
    if (ctx.notify.check(ctx.configStore.remove(c.project), "Config löschen")) {
      mSelected.clear();
      mDraft = {};
      mDirty = false;
    }
    ImGui::EndChild();
    return;
  }

  if (ImGui::CollapsingHeader("Serviceordner", ImGuiTreeNodeFlags_DefaultOpen)) {
    ui::hintText("Werden bei jedem neuen Service im Service-Ordner angelegt.");
    mDirty |= folderList("folders", c.folders, mNewFolder);
  }

  if (ImGui::CollapsingHeader("Vorlagen (Dateien kopieren)", ImGuiTreeNodeFlags_DefaultOpen)) {
    ui::hintText("Platzhalter im Zielnamen: {reader} = Gerätename, {service} = Datum des Service.");
    mDirty |= templateList("files", c.files);
  }

  if (ImGui::CollapsingHeader("Vorlagen für externe Services", ImGuiTreeNodeFlags_DefaultOpen)) {
    ui::hintText(c.filesExtern.empty() ? "Keine eigenen Vorlagen – externe Services verwenden die normalen Vorlagen."
                                       : "Externe Services verwenden nur diese Vorlagen (z. B. eigene Checkliste).");
    mDirty |= templateList("filesExtern", c.filesExtern);
    if (c.filesExtern.empty() && !c.files.empty()) {
      ImGui::SameLine();
      if (ImGui::Button("Normale Vorlagen übernehmen")) {
        c.filesExtern = c.files;
        mDirty = true;
      }
      ImGui::SetItemTooltip("Kopiert die normalen Vorlagen als Ausgangspunkt");
    }
  }

  if (ImGui::CollapsingHeader("Basis-Konfiguration")) {
    bool custom = c.baseFlags.has_value();
    if (ImGui::Checkbox("Eigene Basis-Konfiguration", &custom)) {
      c.baseFlags = custom ? std::optional<devices::ConfigFlags>(c.effectiveBaseFlags()) : std::nullopt;
      mDirty = true;
    }
    ui::help("Konfiguration, die jedes Gerät des Projekts hat (unabhängig vom EEPROM). Ohne eigene Angabe gilt: 0601 = ABS, 0471/0472 = FP.");
    ui::Disabled d(!custom);
    devices::ConfigFlags flags = c.effectiveBaseFlags();
    for (const auto &info : devices::kFlags) {
      bool on = devices::hasFlag(flags, info.flag);
      if (ImGui::Checkbox(std::string(info.label).c_str(), &on)) {
        flags ^= static_cast<devices::ConfigFlags>(info.flag);
        c.baseFlags = flags;
        mDirty = true;
      }
      ImGui::SameLine();
    }
    ImGui::NewLine();
  }

  if (ImGui::CollapsingHeader("EEPROM-Adressen", ImGuiTreeNodeFlags_DefaultOpen)) {
    ui::hintText("Hat die Adresse im neuesten EEPROM-Dump (*.btc) den Validierungswert, erhält das Gerät die Konfiguration.");
    std::size_t remove = c.addresses.size();
    for (std::size_t i = 0; i < c.addresses.size(); ++i) {
      auto &rule = c.addresses[i];
      ui::ScopedId id(static_cast<int>(i));
      ImGui::Separator();
      ImGui::SetNextItemWidth(100.0f);
      mDirty |= ImGui::InputInt("Adresse", &rule.address, 0, 0);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(100.0f);
      mDirty |= ImGui::InputInt("Wert", &rule.validationValue, 0, 0);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(140.0f);
      mDirty |= flagCombo("Konfiguration", rule.flag);
      ImGui::SameLine();
      if (ImGui::SmallButton("Entfernen"))
        remove = i;
      if (ImGui::TreeNode("##folders", "Zusätzliche Ordner (%zu)", rule.folders.size())) {
        mDirty |= folderList("rulefolders", rule.folders, mNewRuleFolder);
        ImGui::TreePop();
      }
    }
    if (remove < c.addresses.size()) {
      c.addresses.erase(c.addresses.begin() + static_cast<std::ptrdiff_t>(remove));
      mDirty = true;
    }
    ImGui::SeparatorText("Neue Adresse");
    ImGui::SetNextItemWidth(100.0f);
    ImGui::InputInt("Adresse##new", &mNewRule.address, 0, 0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::InputInt("Wert##new", &mNewRule.validationValue, 0, 0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    flagCombo("Konfiguration##new", mNewRule.flag);
    ImGui::SameLine();
    ui::Disabled d(!mNewRule.valid());
    if (ImGui::Button("Hinzufügen##rule")) {
      c.addresses.push_back(mNewRule);
      mNewRule = {};
      mDirty = true;
    }
  }
  ImGui::EndChild();
}

} // namespace st::app
