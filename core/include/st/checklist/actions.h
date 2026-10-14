#pragma once

#include "st/checklist/model.h"
#include "st/util/session.h"

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

/// Extension points of the checklist:
///
///  - Expressions fill a task automatically (e.g. "@rinfo.serial" writes the serial
///    number of the active device). They are resolved when a section is finished.
///  - Section actions run after a section has been finished (e.g. "update_status_rinfo"
///    changes the device status).
///
/// Modules register their own expressions/actions at start-up; the checklist editor
/// lists all registered names. See docs/module.md.
namespace st::checklist {

/// Built-in section action that resolves all expressions of the section.
inline constexpr std::string_view kAutoFinishAction = "auto_finish";

using ExpressionFn = std::function<void(Task &)>;

class ExpressionRegistry {
public:
  /// Registers (or replaces) an expression.
  void add(std::string name, ExpressionFn fn, std::string description = {});
  void remove(std::string_view name);
  bool contains(std::string_view name) const;
  /// Runs the expression on `task`. Returns false if no expression with that name exists.
  bool resolve(std::string_view name, Task &task) const;
  /// All registered names, sorted.
  std::vector<std::string> names() const;
  std::string description(std::string_view name) const;

private:
  struct Entry {
    ExpressionFn fn;
    std::string description;
  };
  std::map<std::string, Entry, std::less<>> mEntries;
};

/// Everything a section action may need.
struct SectionActionContext {
  Checklist &checklist;
  Section &section;
  const util::Session &session;
};

using SectionActionFn = std::function<void(SectionActionContext &)>;

class SectionActionRegistry {
public:
  void add(std::string name, SectionActionFn fn, std::string description = {});
  void remove(std::string_view name);
  bool contains(std::string_view name) const;
  /// Runs the action. Returns false if no action with that name exists.
  bool run(std::string_view name, SectionActionContext &context) const;
  std::vector<std::string> names() const;
  std::string description(std::string_view name) const;

private:
  struct Entry {
    SectionActionFn fn;
    std::string description;
  };
  std::map<std::string, Entry, std::less<>> mEntries;
};

/// Sets user and timestamp of a task (marks it as done / changed).
void stamp(Task &task, const util::Session &session);

/// Resolves the expressions of all tasks in `section` and stamps them.
/// Returns the number of resolved tasks.
std::size_t resolveExpressions(Section &section, const ExpressionRegistry &expressions, const util::Session &session);

/// Registers the built-in actions ("auto_finish").
void registerBuiltinActions(SectionActionRegistry &actions, const ExpressionRegistry &expressions);

/// Names of single tasks and group tasks without a timestamp (tables are ignored).
std::vector<std::string> unfinishedTasks(const Section &section);

/// First step of "Arbeitsgang fertigmelden": resolves expressions and returns the
/// tasks that are still open. If the list is not empty the user should confirm.
std::vector<std::string> prepareSectionFinish(Section &section, const ExpressionRegistry &expressions, const util::Session &session);

/// Second step: marks the section as finished and runs its post action.
void finishSection(Checklist &checklist, Section &section, const SectionActionRegistry &actions, const util::Session &session);

/// Re-opens a finished section (editor). The timestamp is updated so other users pick up the change.
void reopenSection(Section &section, const util::Session &session);

/// Finishes the whole checklist: stamps it and finishes all "auto_finish" sections.
void finishChecklist(Checklist &checklist, const ExpressionRegistry &expressions, const SectionActionRegistry &actions, const util::Session &session);

} // namespace st::checklist
