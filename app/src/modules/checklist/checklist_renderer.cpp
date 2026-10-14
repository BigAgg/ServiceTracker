#include "modules/checklist/checklist_renderer.h"

#include "framework/app_context.h"
#include "framework/platform.h"
#include "framework/widgets.h"

#include <st/checklist/actions.h>
#include <st/util/strings.h>

#include <app/app.h>
#include <imgui.h>
#include <imgui/ImGui_helpers.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>

namespace st::app {

using namespace checklist;

namespace {

/// Structural edit requested while iterating a vector; applied afterwards.
struct PendingOp {
  enum Kind { None,
              Up,
              Down,
              Remove } kind = None;
  std::size_t index = 0;

  template <typename T>
  bool apply(std::vector<T> &items) {
    if (kind == None || index >= items.size())
      return false;
    if (kind == Up && index > 0)
      std::swap(items[index], items[index - 1]);
    else if (kind == Down && index + 1 < items.size())
      std::swap(items[index], items[index + 1]);
    else if (kind == Remove)
      items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));
    else
      return false;
    return true;
  }
};

void moveButtons(PendingOp &op, std::size_t index, std::size_t count, const char *removeQuestion) {
  {
    ui::Disabled d(index == 0);
    if (ImGui::ArrowButton("##up", ImGuiDir_Up))
      op = {PendingOp::Up, index};
  }
  ImGui::SameLine();
  {
    ui::Disabled d(index + 1 >= count);
    if (ImGui::ArrowButton("##down", ImGuiDir_Down))
      op = {PendingOp::Down, index};
  }
  ImGui::SameLine();
  if (ui::confirmButton("Entfernen", removeQuestion))
    op = {PendingOp::Remove, index};
}

float textWidth(const std::string &text, float minimum) {
  return std::max(minimum, ImGui::CalcTextSize(text.c_str()).x + ImGui::GetStyle().FramePadding.x * 4.0f);
}

const char *doubleFormat(const Task &task) { return task.fmt.empty() ? "%.2f" : task.fmt.c_str(); }

} // namespace

ChecklistRenderer::ChecklistRenderer(AppContext &ctx, ChecklistDocument &doc, ChecklistViewState &state, bool edit, std::function<void(bool)> onChange)
    : mCtx(ctx), mDoc(doc), mState(state), mEdit(edit), mOnChange(std::move(onChange)) {}

std::pair<int, int> ChecklistRenderer::progress(const Section &section) {
  int done = 0;
  int total = 0;
  forEachTask(section, [&](const Task &t) {
    ++total;
    if (t.isDone())
      ++done;
  });
  return {done, total};
}

std::pair<int, int> ChecklistRenderer::progress(const Checklist &checklist) {
  int done = 0;
  int total = 0;
  for (const auto &s : checklist.sections) {
    const auto [d, t] = progress(s);
    done += d;
    total += t;
  }
  return {done, total};
}

void ChecklistRenderer::changed(Task &task) {
  stamp(task, mCtx.session());
  changed(false);
}

void ChecklistRenderer::changed(bool immediate) {
  if (mOnChange)
    mOnChange(immediate);
}

bool ChecklistRenderer::sectionLocked(const Section &section) const {
  const auto &prefs = mCtx.prefs();
  const bool finishedLock = section.finished && !prefs.ignoreFinished;
  const bool permissionLock = !mCtx.hasAnyAccess(section.permissions) && !prefs.ignorePermissions;
  return finishedLock || permissionLock;
}

void ChecklistRenderer::drawSections() {
  auto &sections = mDoc.data().sections;
  PendingOp op;
  for (std::size_t i = 0; i < sections.size(); ++i) {
    Section &section = sections[i];
    if (!mEdit && mCtx.prefs().onlyPermittedSections && !mCtx.prefs().ignorePermissions && !mCtx.hasAnyAccess(section.permissions))
      continue;
    ui::ScopedId id(static_cast<int>(i));
    if (mEdit) {
      moveButtons(op, i, sections.size(), "Arbeitsgang mit allen Aufgaben entfernen?");
      ImGui::SameLine();
      if (ImGui::Button("Bearbeiten")) {
        mState.editSection = static_cast<int>(i);
        ImGui::OpenPopup("Arbeitsgang bearbeiten");
      }
      drawSectionEditor(section, i);
      ImGui::SameLine();
    }
    drawSection(section, i);
  }
  if (op.apply(sections))
    changed(true);
  mState.expandAll.reset();
  mState.autoExpand = false;
}

void ChecklistRenderer::drawSection(Section &section, std::size_t index) {
  const auto [done, total] = progress(section);
  std::string label = section.name + "   [" + std::to_string(done) + "/" + std::to_string(total) + "]";
  if (section.finished)
    label += "   Abgeschlossen von " + section.user + " am " + mCtx.displayTime(section.timestamp);
  label += "###section" + std::to_string(index);

  if (mState.expandAll)
    ImGui::SetNextItemOpen(*mState.expandAll);
  else if (mState.autoExpand)
    ImGui::SetNextItemOpen(mEdit || (!section.finished && !sectionLocked(section)));
  if (section.finished)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.80f, 0.45f, 1.0f));
  const bool open = ImGui::CollapsingHeader(label.c_str());
  if (section.finished)
    ImGui::PopStyleColor();
  if (!section.permissions.empty())
    ImGui::SetItemTooltip("Berechtigungen: %s", util::join(section.permissions, ", ").c_str());

  if (open) {
    ImGui::Indent(ImGui::GetStyle().IndentSpacing * 0.5f);
    drawSectionToolbar(section);
    drawItems(section);
    ImGui::Unindent(ImGui::GetStyle().IndentSpacing * 0.5f);
  }
  drawFinishPopups(section);
}

void ChecklistRenderer::drawSectionToolbar(Section &section) {
  ImGui::SetNextItemWidth(80.0f);
  ImGui::InputDouble("Bearbeitungszeit", &section.time, 0.0, 0.0, "%.2fh");
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    section.time = normalizeHours(section.time);
    changed(false);
  }
  ImGui::SetItemTooltip("Stunden.Minuten, z.B. 1.30 = 1 Stunde 30 Minuten");
  ImGui::SameLine();

  const bool noPermission = !mCtx.hasAnyAccess(section.permissions) && !mCtx.prefs().ignorePermissions;
  {
    ui::Disabled d(noPermission || section.finished);
    if (ImGui::Button("Arbeitsgang fertigmelden")) {
      mState.openTasks = prepareSectionFinish(section, mCtx.expressions, mCtx.session());
      changed(false);
      if (mState.openTasks.empty()) {
        finishSection(mDoc.data(), section, mCtx.sectionActions, mCtx.session());
        changed(true);
        if (section.postPopup.isComplete())
          ImGui::OpenPopup("##postpopup");
      } else {
        mState.confirmSection = section.name;
        ImGui::OpenPopup("Arbeitsgang fertigmelden");
      }
    }
  }
  if (noPermission)
    ImGui::SetItemTooltip("Keine Berechtigung für diesen Arbeitsgang");

  if (!mEdit)
    return;
  ImGui::SameLine();
  bool finished = section.finished;
  if (ImGui::Checkbox("Fertig", &finished)) {
    if (finished)
      finishSection(mDoc.data(), section, mCtx.sectionActions, mCtx.session());
    else
      reopenSection(section, mCtx.session());
    changed(true);
  }
  if (ImGui::Button("Neue Einzelaufgabe")) {
    mState.editTask = Task::make("", false);
    ImGui::OpenPopup("Neue Einzelaufgabe");
  }
  ImGui::SameLine();
  if (ImGui::Button("Neue Aufgabengruppe")) {
    section.items.emplace_back(TaskGroup{});
    changed(false);
  }
  ImGui::SameLine();
  if (ImGui::Button("Neue Tabelle")) {
    TaskTable table;
    table.name = "Neue Tabelle";
    section.items.emplace_back(std::move(table));
    changed(false);
  }
  if (ImGui::BeginPopup("Neue Einzelaufgabe")) {
    if (drawTaskEditor(mState.editTask)) {
      section.items.emplace_back(mState.editTask);
      changed(false);
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void ChecklistRenderer::drawFinishPopups(Section &section) {
  if (ImGui::BeginPopupModal("Arbeitsgang fertigmelden", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::SeparatorText("Es gibt unbearbeitete Aufgaben. Bitte bestätigen");
    for (const auto &task : mState.openTasks)
      ImGui::BulletText("%s", task.c_str());
    ImGui::Separator();
    bool showPost = false;
    if (ImGui::Button("Trotzdem fertigmelden")) {
      finishSection(mDoc.data(), section, mCtx.sectionActions, mCtx.session());
      changed(true);
      showPost = section.postPopup.isComplete();
      mState.openTasks.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Nicht fertigmelden") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      mState.openTasks.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
    if (showPost)
      mState.postPopupSection = section.name;
  }
  if (mState.postPopupSection == section.name) {
    mState.postPopupSection.clear();
    ImGui::OpenPopup("##postpopup");
  }
  if (ImGui::BeginPopupModal("##postpopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(section.postPopup.title.c_str());
    ImGui::SeparatorText(section.postPopup.header.c_str());
    ImGui::TextUnformatted(section.postPopup.text.c_str());
    if (ImGui::Button("Ok") || ImGui::IsKeyPressed(ImGuiKey_Enter))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}

void ChecklistRenderer::drawSectionEditor(Section &section, std::size_t index) {
  if (mState.editSection != static_cast<int>(index))
    return;
  ImGui::SetNextWindowSize(ImVec2(560.0f, 0.0f), ImGuiCond_Appearing);
  if (!ImGui::BeginPopupModal("Arbeitsgang bearbeiten"))
    return;
  if (ImGui::InputText("Name", &section.name))
    changed(false);

  ImGui::SeparatorText("Post-Action");
  if (ImGui::BeginCombo("Aktion", section.postAction.empty() ? "(keine)" : section.postAction.c_str())) {
    if (ImGui::Selectable("(keine)", section.postAction.empty())) {
      section.postAction.clear();
      section.postActionArgs.clear();
      changed(false);
    }
    for (const auto &name : mCtx.sectionActions.names()) {
      if (ImGui::Selectable(name.c_str(), name == section.postAction)) {
        section.postAction = name;
        changed(false);
      }
      if (const auto desc = mCtx.sectionActions.description(name); !desc.empty())
        ImGui::SetItemTooltip("%s", desc.c_str());
    }
    ImGui::EndCombo();
  }
  if (!section.postAction.empty() && ImGui::InputText("Argumente", &section.postActionArgs))
    changed(false);

  ImGui::SeparatorText("Berechtigungen");
  drawPermissionEditor(section.permissions);

  ImGui::SeparatorText("Hinweis nach dem Fertigmelden");
  bool popupChanged = ImGui::InputText("Titel", &section.postPopup.title);
  popupChanged |= ImGui::InputText("Überschrift", &section.postPopup.header);
  popupChanged |= ui::inputMultiline("Text", section.postPopup.text, 3.0f);
  if (popupChanged)
    changed(false);
  ui::hintText("Der Hinweis erscheint nur, wenn Titel, Überschrift und Text ausgefüllt sind.");

  ImGui::Separator();
  if (ImGui::Button("Schließen") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    mState.editSection = -1;
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

void ChecklistRenderer::drawPermissionEditor(std::vector<std::string> &permissions) {
  static std::string custom;
  if (ImGui::BeginCombo("##addgroup", "Gruppe hinzufügen...")) {
    for (const auto &group : mCtx.userStore.knownGroups()) {
      if (std::find(permissions.begin(), permissions.end(), group) != permissions.end())
        continue;
      if (ImGui::Selectable(group.c_str())) {
        permissions.push_back(group);
        changed(false);
      }
    }
    ImGui::SetNextItemWidth(150.0f);
    ImGui::InputTextWithHint("##customgroup", "andere Gruppe", &custom);
    ImGui::SameLine();
    if (ImGui::SmallButton("+") && !custom.empty()) {
      permissions.push_back(custom);
      custom.clear();
      changed(false);
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndCombo();
  }
  if (permissions.empty())
    ui::hintText("Keine Einschränkung: alle Benutzer dürfen den Arbeitsgang bearbeiten.");
  for (std::size_t i = 0; i < permissions.size(); ++i) {
    ui::ScopedId id(static_cast<int>(i));
    if (i > 0)
      ImGui::SameLine();
    if (ImGui::SmallButton((permissions[i] + "  x").c_str())) {
      permissions.erase(permissions.begin() + static_cast<std::ptrdiff_t>(i));
      changed(false);
      break;
    }
    ImGui::SetItemTooltip("Entfernen");
  }
}

void ChecklistRenderer::drawItems(Section &section) {
  ui::Disabled locked(!mEdit && sectionLocked(section));
  PendingOp op;
  for (std::size_t i = 0; i < section.items.size(); ++i) {
    ui::ScopedId id(static_cast<int>(i));
    auto &item = section.items[i];
    if (mState.onlyOpen && !mEdit) {
      if (const auto *task = std::get_if<Task>(&item); task && task->isDone())
        continue;
      if (const auto *group = std::get_if<TaskGroup>(&item))
        if (std::all_of(group->tasks.begin(), group->tasks.end(), [](const Task &t) { return t.isDone(); }))
          continue;
    }
    ImGui::Separator();
    if (mEdit) {
      moveButtons(op, i, section.items.size(), "Element entfernen?");
      ImGui::SameLine();
    }
    std::visit([&](auto &value) {
      using T = std::decay_t<decltype(value)>;
      if constexpr (std::is_same_v<T, Task>) {
        drawTask(value, false, true);
        if (mEdit) {
          ImGui::SameLine();
          if (ImGui::SmallButton("...")) {
            mState.editTask = value;
            ImGui::OpenPopup("Aufgabe bearbeiten");
          }
          ImGui::SetItemTooltip("Aufgabe bearbeiten");
          if (ImGui::BeginPopup("Aufgabe bearbeiten")) {
            if (drawTaskEditor(mState.editTask)) {
              value = mState.editTask;
              changed(false);
              ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
          }
        }
      } else if constexpr (std::is_same_v<T, TaskGroup>) {
        drawGroup(value);
      } else {
        drawTable(value);
      }
    },
               item);
  }
  if (op.apply(section.items))
    changed(false);
}

bool ChecklistRenderer::drawTaskValue(Task &task, bool hideLabel) {
  const std::string label = (hideLabel ? "##" : "") + task.name + "##value";
  return std::visit([&](auto &v) -> bool {
    using T = std::decay_t<decltype(v)>;
    if constexpr (std::is_same_v<T, int>) {
      if (!hideLabel)
        ImGui::SetNextItemWidth(120.0f);
      return ImGui::InputInt(label.c_str(), &v, 0, 0);
    } else if constexpr (std::is_same_v<T, double>) {
      if (!hideLabel)
        ImGui::SetNextItemWidth(120.0f);
      return ImGui::InputDouble(label.c_str(), &v, 0.0, 0.0, doubleFormat(task));
    } else if constexpr (std::is_same_v<T, bool>) {
      return ImGui::Checkbox(label.c_str(), &v);
    } else if constexpr (std::is_same_v<T, Text>) {
      if (!hideLabel)
        ImGui::SetNextItemWidth(textWidth(v.value, 150.0f));
      return ImGui::InputText(label.c_str(), &v.value);
    } else if constexpr (std::is_same_v<T, TextBox>) {
      const float lines = static_cast<float>(std::count(v.value.begin(), v.value.end(), '\n') + 2);
      const ImVec2 size(hideLabel ? -FLT_MIN : std::max(300.0f, ImGui::CalcTextSize(v.value.c_str()).x + 30.0f),
                        std::min(lines, 15.0f) * ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f);
      return ImGui::InputTextMultiline(label.c_str(), &v.value, size);
    } else if constexpr (std::is_same_v<T, Combo>) {
      bool changedValue = false;
      if (!hideLabel)
        ImGui::SetNextItemWidth(textWidth(v.selected, 150.0f));
      if (ImGui::BeginCombo(label.c_str(), v.selected.c_str())) {
        for (const auto &option : v.options) {
          if (ImGui::Selectable(option.c_str(), option == v.selected)) {
            v.selected = option;
            changedValue = true;
          }
        }
        ImGui::EndCombo();
      }
      if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !v.selected.empty()) {
        v.selected.clear();
        changedValue = true;
      }
      return changedValue;
    } else {
      // DatePicker prints its label verbatim, so no "##value" suffix here (the ID is
      // unique through the surrounding ScopedId of the task).
      bool changedValue = ImGui::DatePicker((hideLabel ? "##" : "") + task.name, v.value);
      if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !v.value.empty()) {
        v.value.clear();
        changedValue = true;
      }
      return changedValue;
    }
  },
                    task.value);
}

void ChecklistRenderer::drawTask(Task &task, bool hideLabel, bool showMeta) {
  ui::ScopedId id(&task);
  if (hideLabel)
    ImGui::SetNextItemWidth(-FLT_MIN);
  if (drawTaskValue(task, hideLabel))
    changed(task);
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::BeginTooltip();
    if (!task.tooltip.empty())
      ImGui::TextUnformatted(task.tooltip.c_str());
    if (task.isDone())
      ImGui::TextDisabled("%s, %s", task.user.c_str(), mCtx.displayTime(task.timestamp).c_str());
    else
      ImGui::TextDisabled("noch nicht bearbeitet");
    if (std::holds_alternative<Combo>(task.value) || std::holds_alternative<Date>(task.value))
      ImGui::TextDisabled("Rechtsklick: Auswahl entfernen");
    ImGui::EndTooltip();
  }
  if (showMeta)
    drawTaskMeta(task);
}

void ChecklistRenderer::drawTaskMeta(Task &task) {
  ImGui::SameLine();
  const bool hasExtras = !task.comment.empty() || !task.attachments.empty();
  ImGui::PushStyleColor(ImGuiCol_Text, hasExtras ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextUnformatted("(?)");
  ImGui::PopStyleColor();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("(Klicken zum Bearbeiten)\nBearbeiter: %s\nZeitstempel: %s\nAnlagen: %zu\nKommentar:\n%s", task.user.c_str(),
                      mCtx.displayTime(task.timestamp).c_str(), task.attachments.size(), task.comment.c_str());
  if (ImGui::IsItemClicked())
    ImGui::OpenPopup("##meta");
  if (!ImGui::BeginPopup("##meta"))
    return;
  ImGui::TextUnformatted(task.name.c_str());
  ImGui::SeparatorText("Kommentar");
  ImGui::SetNextItemWidth(420.0f);
  if (ui::inputMultiline("##comment", task.comment, 3.0f))
    changed(task);
  ImGui::SeparatorText("Anhänge");
  ui::hintText("Dateien per Drag & Drop hierher ziehen. Klick öffnet, Rechtsklick entfernt.");
  std::size_t removeIndex = task.attachments.size();
  if (ImGui::BeginListBox("##attachments", ImVec2(420.0f, 5.0f * ImGui::GetTextLineHeightWithSpacing()))) {
    for (std::size_t i = 0; i < task.attachments.size(); ++i) {
      const auto path = util::pathFromUtf8(task.attachments[i]);
      ui::ScopedId aid(static_cast<int>(i));
      if (ImGui::Selectable(util::pathToUtf8(path.filename()).c_str()))
        platform::openPath(path);
      ImGui::SetItemTooltip("%s", task.attachments[i].c_str());
      if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        removeIndex = i;
    }
    ImGui::EndListBox();
  }
  if (removeIndex < task.attachments.size()) {
    const auto path = util::pathFromUtf8(task.attachments[removeIndex]);
    // only delete files that live in the checklist's own attachment folder
    if (path.parent_path() == mDoc.attachmentsDir())
      (void)util::removeAll(path);
    task.attachments.erase(task.attachments.begin() + static_cast<std::ptrdiff_t>(removeIndex));
    changed(task);
  }
  if (::app::HasDropfiles()) {
    for (const auto &file : ::app::GetDropfiles()) {
      const auto source = util::pathFromUtf8(file);
      const auto target = mDoc.attachmentsDir() / source.filename();
      if (mCtx.notify.check(util::copyFile(source, target), "Anhang")) {
        task.attachments.push_back(util::pathToUtf8(target));
        changed(task);
      }
    }
  }
  if (ImGui::Button("Schließen"))
    ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

void ChecklistRenderer::drawGroup(TaskGroup &group) {
  if (mEdit) {
    ImGui::SetNextItemWidth(textWidth(group.name, 150.0f));
    if (ImGui::InputTextWithHint("##groupname", "Gruppenname", &group.name))
      changed(false);
    ImGui::SameLine();
    if (ImGui::Button("Neue Aufgabe")) {
      mState.editTask = Task::make("", false);
      ImGui::OpenPopup("Neue Aufgabe");
    }
    if (ImGui::BeginPopup("Neue Aufgabe")) {
      if (drawTaskEditor(mState.editTask)) {
        group.tasks.push_back(mState.editTask);
        changed(false);
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  } else if (!group.name.empty()) {
    ImGui::TextUnformatted(group.name.c_str());
  }

  PendingOp op;
  for (std::size_t i = 0; i < group.tasks.size(); ++i) {
    Task &task = group.tasks[i];
    if (mState.onlyOpen && !mEdit && task.isDone())
      continue;
    if (i > 0)
      ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 3.0f);
    ui::ScopedId id(static_cast<int>(i));
    drawTask(task, false, true);
    if (!mEdit)
      continue;
    ImGui::SameLine();
    if (ImGui::SmallButton("...")) {
      mState.editTask = task;
      ImGui::OpenPopup("Aufgabe bearbeiten");
    }
    ImGui::SetItemTooltip("Aufgabe bearbeiten / verschieben / entfernen");
    if (ImGui::BeginPopup("Aufgabe bearbeiten")) {
      if (ImGui::ArrowButton("##left", ImGuiDir_Left)) {
        op = {PendingOp::Up, i};
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::ArrowButton("##right", ImGuiDir_Right)) {
        op = {PendingOp::Down, i};
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Entfernen")) {
        op = {PendingOp::Remove, i};
        ImGui::CloseCurrentPopup();
      }
      ImGui::Separator();
      if (drawTaskEditor(mState.editTask)) {
        task = mState.editTask;
        changed(false);
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }
  if (op.apply(group.tasks))
    changed(false);
}

void ChecklistRenderer::drawTable(TaskTable &table) {
  if (table.rows.size() > 0 && table.rows.front().tasks.size() != table.templateRow.tasks.size())
    if (alignTableRows(table))
      changed(false);

  if (mEdit) {
    ImGui::SetNextItemWidth(textWidth(table.name, 200.0f));
    if (ImGui::InputTextWithHint("##tablename", "Tabellenname", &table.name))
      changed(false);
    ImGui::SameLine();
    if (ImGui::Button("Neue Spalte")) {
      mState.editTask = Task::make("", Text{});
      ImGui::OpenPopup("Neue Spalte");
    }
    if (ImGui::BeginPopup("Neue Spalte")) {
      if (drawTaskEditor(mState.editTask)) {
        table.templateRow.tasks.push_back(mState.editTask);
        alignTableRows(table);
        changed(false);
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  } else {
    ImGui::TextUnformatted(table.name.c_str());
    std::string tips;
    for (const auto &column : table.templateRow.tasks)
      if (!column.tooltip.empty())
        tips += column.name + ":\n  " + util::replaced(column.tooltip, "\n", "\n  ") + "\n";
    if (!tips.empty())
      ui::help(tips.c_str());
  }

  const auto addRow = [&] {
    table.rows.push_back(table.templateRow);
    table.timestamp = mCtx.session().timestamp();
    changed(false);
  };
  ImGui::SameLine();
  int rows = static_cast<int>(table.rows.size());
  ImGui::SetNextItemWidth(110.0f);
  if (ImGui::InputInt("Zeilen", &rows) && rows >= 0 && rows < 1000) {
    while (static_cast<int>(table.rows.size()) < rows)
      table.rows.push_back(table.templateRow);
    while (static_cast<int>(table.rows.size()) > rows)
      table.rows.pop_back();
    table.timestamp = mCtx.session().timestamp();
    changed(false);
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("+ Zeile"))
    addRow();
  ImGui::SetItemTooltip("Neue Zeile anhängen (oder Enter in der letzten Zeile)");

  auto &columns = table.templateRow.tasks;
  if (columns.empty()) {
    ui::hintText("Die Tabelle hat noch keine Spalten.");
    return;
  }

  // Column definitions (editor)
  if (mEdit) {
    PendingOp colOp;
    for (std::size_t c = 0; c < columns.size(); ++c) {
      ui::ScopedId id(static_cast<int>(c) + 10000);
      if (c > 0)
        ImGui::SameLine();
      if (ImGui::SmallButton((columns[c].name + " ...").c_str())) {
        mState.editTask = columns[c];
        ImGui::OpenPopup("Spalte bearbeiten");
      }
      if (ImGui::BeginPopup("Spalte bearbeiten")) {
        if (ImGui::ArrowButton("##left", ImGuiDir_Left)) {
          colOp = {PendingOp::Up, c};
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::ArrowButton("##right", ImGuiDir_Right)) {
          colOp = {PendingOp::Down, c};
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Spalte entfernen")) {
          colOp = {PendingOp::Remove, c};
          ImGui::CloseCurrentPopup();
        }
        ImGui::Separator();
        if (drawTaskEditor(mState.editTask)) {
          columns[c] = mState.editTask;
          alignTableRows(table);
          changed(false);
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
    }
    if (colOp.apply(columns)) {
      alignTableRows(table);
      changed(false);
    }
  }

  const int columnCount = static_cast<int>(columns.size()) + 1;
  const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingStretchProp;
  if (!ImGui::BeginTable("##table", columnCount, flags))
    return;
  ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, mEdit ? 110.0f : 60.0f);
  for (const auto &column : columns)
    ImGui::TableSetupColumn(column.name.c_str());
  ImGui::TableHeadersRow();

  PendingOp rowOp;
  bool addAfter = false;
  for (std::size_t r = 0; r < table.rows.size(); ++r) {
    ui::ScopedId rowId(static_cast<int>(r));
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%zu", r + 1);
    ImGui::SameLine();
    if (ImGui::SmallButton("x"))
      rowOp = {PendingOp::Remove, r};
    ImGui::SetItemTooltip("Zeile löschen");
    if (mEdit) {
      ImGui::SameLine();
      if (ImGui::ArrowButton("##rup", ImGuiDir_Up))
        rowOp = {PendingOp::Up, r};
      ImGui::SameLine();
      if (ImGui::ArrowButton("##rdown", ImGuiDir_Down))
        rowOp = {PendingOp::Down, r};
    }
    auto &row = table.rows[r];
    for (std::size_t c = 0; c < row.tasks.size(); ++c) {
      ImGui::TableSetColumnIndex(static_cast<int>(c) + 1);
      if (mState.focusTable == &table && r + 1 == table.rows.size() && c == 0) {
        ImGui::SetKeyboardFocusHere();
        mState.focusTable = nullptr;
      }
      drawTask(row.tasks[c], true, false);
      const bool last = r + 1 == table.rows.size();
      if (last && ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGuiKey_Enter) && !ImGui::IsKeyDown(ImGuiMod_Shift))
        addAfter = true;
    }
  }
  ImGui::EndTable();
  if (rowOp.apply(table.rows)) {
    table.timestamp = mCtx.session().timestamp();
    changed(false);
  }
  if (addAfter) {
    addRow();
    mState.focusTable = &table;
  }
}

bool ChecklistRenderer::drawTaskEditor(Task &task) {
  ImGui::SetNextItemWidth(300.0f);
  ImGui::InputTextWithHint("Name", "Aufgabenname", &task.name);
  ImGui::SetNextItemWidth(300.0f);
  ui::inputMultiline("Tooltip", task.tooltip, 2.0f);

  const ValueType type = valueTypeOf(task.value);
  ImGui::SetNextItemWidth(300.0f);
  if (ImGui::BeginCombo("Typ", std::string(valueTypeName(type)).c_str())) {
    for (const auto candidate : kAllValueTypes) {
      if (ImGui::Selectable(std::string(valueTypeName(candidate)).c_str(), candidate == type) && candidate != type) {
        task.value = defaultValue(candidate);
        if (candidate == ValueType::Combo || candidate == ValueType::Date)
          task.expression.clear();
      }
    }
    ImGui::EndCombo();
  }

  if (type != ValueType::Combo && type != ValueType::Date) {
    ImGui::SetNextItemWidth(300.0f);
    if (ImGui::BeginCombo("Expression", task.expression.empty() ? "(keine)" : task.expression.c_str())) {
      if (ImGui::Selectable("(keine)", task.expression.empty()))
        task.expression.clear();
      for (const auto &name : mCtx.expressions.names()) {
        if (ImGui::Selectable(name.c_str(), name == task.expression))
          task.expression = name;
        if (const auto desc = mCtx.expressions.description(name); !desc.empty())
          ImGui::SetItemTooltip("%s", desc.c_str());
      }
      ImGui::EndCombo();
    }
    ui::help("Wird beim Fertigmelden des Arbeitsgangs automatisch ausgefüllt.");
  }

  ImGui::SeparatorText("Standardwert");
  std::visit([&](auto &v) {
    using T = std::decay_t<decltype(v)>;
    if constexpr (std::is_same_v<T, int>) {
      ImGui::InputInt("Wert", &v, 0, 0);
    } else if constexpr (std::is_same_v<T, double>) {
      ImGui::InputDouble("Wert", &v, 0.0, 0.0, doubleFormat(task));
      ImGui::SetNextItemWidth(120.0f);
      ImGui::InputTextWithHint("Format", "%.2f", &task.fmt);
      ui::help("printf-Format, z.B. %.2f oder %.1f h. Text nach dem f wird als Einheit angezeigt.");
    } else if constexpr (std::is_same_v<T, bool>) {
      ImGui::Checkbox("Wert", &v);
    } else if constexpr (std::is_same_v<T, Text>) {
      ImGui::InputText("Wert", &v.value);
    } else if constexpr (std::is_same_v<T, TextBox>) {
      ui::inputMultiline("Wert", v.value, 3.0f);
    } else if constexpr (std::is_same_v<T, Combo>) {
      if (ImGui::BeginCombo("Wert", v.selected.c_str())) {
        for (const auto &option : v.options)
          if (ImGui::Selectable(option.c_str(), option == v.selected))
            v.selected = option;
        ImGui::EndCombo();
      }
      ImGui::SetNextItemWidth(220.0f);
      const bool enter = ImGui::InputTextWithHint("##newoption", "neue Option", &mState.newComboOption, ImGuiInputTextFlags_EnterReturnsTrue);
      ImGui::SameLine();
      if ((ImGui::Button("Einfügen") || enter) && !mState.newComboOption.empty() &&
          std::find(v.options.begin(), v.options.end(), mState.newComboOption) == v.options.end()) {
        v.options.push_back(mState.newComboOption);
        mState.newComboOption.clear();
      }
      for (std::size_t i = 0; i < v.options.size(); ++i) {
        ui::ScopedId id(static_cast<int>(i));
        if (ImGui::SmallButton("x")) {
          if (v.selected == v.options[i])
            v.selected.clear();
          v.options.erase(v.options.begin() + static_cast<std::ptrdiff_t>(i));
          break;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(v.options[i].c_str());
      }
    } else {
      ImGui::DatePicker("Wert", v.value);
    }
  },
             task.value);

  ImGui::Separator();
  const bool valid = !util::trim(task.name).empty();
  bool apply = false;
  {
    ui::Disabled d(!valid);
    apply = ImGui::Button("Übernehmen");
  }
  if (!valid)
    ui::errorText("Name darf nicht leer sein");
  ImGui::SameLine();
  if (ImGui::Button("Abbrechen"))
    ImGui::CloseCurrentPopup();
  return apply && valid;
}

} // namespace st::app
