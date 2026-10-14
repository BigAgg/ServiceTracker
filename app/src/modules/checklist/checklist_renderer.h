#pragma once

#include <st/checklist/document.h>
#include <st/checklist/model.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace st::app {

class AppContext;

/// UI state of a checklist view that must survive between frames.
struct ChecklistViewState {
  /// Copy of the task edited in the task editor popup.
  checklist::Task editTask;
  std::string newComboOption;
  /// Section that waits for "Trotzdem fertigmelden" and its open tasks.
  std::string confirmSection;
  std::vector<std::string> openTasks;
  /// Section whose post popup should be shown.
  std::string postPopupSection;
  /// One-shot request to open/close all section headers.
  std::optional<bool> expandAll;
  /// One-shot: open the sections that still need work (set when a checklist is opened).
  bool autoExpand = true;
  /// Section edited in the section editor popup (index).
  int editSection = -1;
  /// Only show tasks that are not done yet.
  bool onlyOpen = false;
  /// Table that just got a new row by pressing Enter (focus its first cell).
  const void *focusTable = nullptr;
};

/// Draws a checklist for working (edit = false) or for editing the structure
/// (edit = true, "Checklisten Editor"). Every change calls `onChange`.
class ChecklistRenderer {
public:
  /// `onChange(immediate)` is called after every modification; `immediate` asks for saving right away
  /// (e.g. a section was finished) instead of the delayed autosave.
  ChecklistRenderer(AppContext &ctx, checklist::ChecklistDocument &doc, ChecklistViewState &state, bool edit, std::function<void(bool)> onChange);

  /// Draws all sections.
  void drawSections();

  /// Number of done / total tasks of a section (table cells count as tasks).
  static std::pair<int, int> progress(const checklist::Section &section);
  static std::pair<int, int> progress(const checklist::Checklist &checklist);

private:
  void drawSection(checklist::Section &section, std::size_t index);
  bool sectionLocked(const checklist::Section &section) const;
  void drawSectionToolbar(checklist::Section &section);
  void drawSectionEditor(checklist::Section &section, std::size_t index);
  void drawItems(checklist::Section &section);
  void drawFinishPopups(checklist::Section &section);

  /// Returns true when the value changed (task is stamped by the caller).
  bool drawTaskValue(checklist::Task &task, bool hideLabel);
  void drawTask(checklist::Task &task, bool hideLabel, bool showMeta);
  void drawTaskMeta(checklist::Task &task);
  void drawGroup(checklist::TaskGroup &group);
  void drawTable(checklist::TaskTable &table);

  /// Editor popup for a task definition. Returns true on "Übernehmen".
  bool drawTaskEditor(checklist::Task &task);
  void drawPermissionEditor(std::vector<std::string> &permissions);

  void changed(checklist::Task &task);
  void changed(bool immediate = false);

  AppContext &mCtx;
  checklist::ChecklistDocument &mDoc;
  ChecklistViewState &mState;
  bool mEdit;
  std::function<void(bool)> mOnChange;
};

} // namespace st::app
