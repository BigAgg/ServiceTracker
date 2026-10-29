#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <vector>

/// Data model of a service checklist.
///
/// Structure (names of the legacy file format in brackets):
///   Checklist
///    └─ Section            [SECTION]    "Arbeitsgang"
///        ├─ Task           [CHECKINFO]  single task "Einzelaufgabe"
///        ├─ TaskGroup      [CHECKLINE]  tasks in one row "Aufgabengruppe"
///        └─ TaskTable      [CHECKTABLE] table with a template row "Tabelle"
///
/// A task counts as done as soon as it carries a timestamp. Timestamps are the
/// basis for merging concurrent edits of several users (see merge.h).
namespace st::checklist {

struct Text {
  std::string value;
  bool operator==(const Text &) const = default;
};

struct TextBox {
  std::string value;
  bool operator==(const TextBox &) const = default;
};

/// Date chosen with the date picker, stored as entered ("DD.MM.YYYY").
struct Date {
  std::string value;
  bool operator==(const Date &) const = default;
};

struct Combo {
  std::vector<std::string> options;
  std::string selected;
  bool operator==(const Combo &) const = default;
};

/// Value of a task. The alternative order matches the legacy program
/// (a default constructed value is an int 0).
using Value = std::variant<int, double, bool, Text, TextBox, Date, Combo>;

/// Value types in the order they are offered in the editor.
enum class ValueType { Bool,
                       Int,
                       Double,
                       Text,
                       TextBox,
                       Combo,
                       Date };

inline constexpr ValueType kAllValueTypes[] = {ValueType::Bool, ValueType::Int, ValueType::Double, ValueType::Text,
                                               ValueType::TextBox, ValueType::Combo, ValueType::Date};

/// Editor label of a type ("BOOL", "INT", ... like the legacy editor).
std::string_view valueTypeName(ValueType type);
ValueType valueTypeOf(const Value &value);
/// Empty value of the given type.
Value defaultValue(ValueType type);

struct Task {
  Value value;
  std::string name;
  std::string comment;
  std::vector<std::string> attachments;
  std::string user;
  std::string timestamp;
  /// Name of a registered expression that fills the value automatically.
  std::string expression;
  std::string tooltip;
  /// printf style number format for doubles, e.g. "%.2f€".
  std::string fmt;

  /// Creates a task with a name and an initial value.
  static Task make(std::string name, Value value) {
    Task t;
    t.name = std::move(name);
    t.value = std::move(value);
    return t;
  }

  bool isDone() const { return !timestamp.empty(); }
  bool operator==(const Task &) const = default;
};

struct TaskGroup {
  bool singleTick = false;
  std::string name;
  std::vector<Task> tasks;
  bool operator==(const TaskGroup &) const = default;
};

struct TaskTable {
  std::string name;
  std::vector<TaskGroup> rows;
  /// Column definition. New rows are copies of this row.
  TaskGroup templateRow;
  /// Last structural change (row added/removed/moved).
  std::string timestamp;
  bool operator==(const TaskTable &) const = default;
};

using Item = std::variant<TaskGroup, Task, TaskTable>;

/// Message shown after a section has been finished.
struct Popup {
  std::string title;
  std::string header;
  std::string text;

  bool isComplete() const { return !title.empty() && !header.empty() && !text.empty(); }
  bool operator==(const Popup &) const = default;
};

struct Section {
  std::string name;
  std::vector<Item> items;
  /// Working time in the legacy "h.mm" notation (1.30 = 1 hour 30 minutes).
  double time = 0.0;
  std::string user;
  std::string timestamp;
  bool finished = false;
  /// User groups allowed to work on this section.
  std::vector<std::string> permissions;
  /// Name of a registered section action executed when the section is finished.
  std::string postAction;
  std::string postActionArgs;
  Popup postPopup;
  /// Section may appear in the record attached by "E-Mail erstellen".
  bool emailExport = true;

  bool operator==(const Section &) const = default;
};

struct Checklist {
  std::string version;
  std::string user;
  std::string timestamp;
  std::vector<std::string> permissions;
  bool finished = false;
  std::vector<Section> sections;

  Section *findSection(std::string_view name);
  const Section *findSection(std::string_view name) const;
  bool operator==(const Checklist &) const = default;
};

/// Calls `fn(Task&)` for every task of the section, including all table cells.
template <typename Fn>
void forEachTask(Section &section, Fn &&fn) {
  for (auto &item : section.items) {
    std::visit([&](auto &value) {
      using T = std::decay_t<decltype(value)>;
      if constexpr (std::is_same_v<T, Task>) {
        fn(value);
      } else if constexpr (std::is_same_v<T, TaskGroup>) {
        for (auto &task : value.tasks)
          fn(task);
      } else if constexpr (std::is_same_v<T, TaskTable>) {
        for (auto &row : value.rows)
          for (auto &task : row.tasks)
            fn(task);
      }
    },
               item);
  }
}

template <typename Fn>
void forEachTask(const Section &section, Fn &&fn) {
  forEachTask(const_cast<Section &>(section), [&](Task &task) { fn(static_cast<const Task &>(task)); });
}

/// Normalises a "h.mm" time: 1.75 -> 2.15 (minutes above 59 roll over).
double normalizeHours(double hours);

/// Sum of all section times in "h.mm" notation.
double totalHours(const Checklist &checklist);

/// Makes every table row match the column layout of the template row.
/// Cells keep their value when the column type did not change. Returns true if
/// anything was changed.
bool alignTableRows(TaskTable &table);

} // namespace st::checklist
