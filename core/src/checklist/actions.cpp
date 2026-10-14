#include "st/checklist/actions.h"

#include "st/util/log.h"

namespace st::checklist {

void ExpressionRegistry::add(std::string name, ExpressionFn fn, std::string description) {
  mEntries[std::move(name)] = Entry{std::move(fn), std::move(description)};
}

void ExpressionRegistry::remove(std::string_view name) {
  if (auto it = mEntries.find(name); it != mEntries.end())
    mEntries.erase(it);
}

bool ExpressionRegistry::contains(std::string_view name) const { return mEntries.find(name) != mEntries.end(); }

bool ExpressionRegistry::resolve(std::string_view name, Task &task) const {
  const auto it = mEntries.find(name);
  if (it == mEntries.end() || !it->second.fn) {
    log::warning("Unbekannte Expression \"{}\" in Aufgabe \"{}\"", name, task.name);
    return false;
  }
  it->second.fn(task);
  return true;
}

std::vector<std::string> ExpressionRegistry::names() const {
  std::vector<std::string> out;
  for (const auto &[name, entry] : mEntries)
    out.push_back(name);
  return out;
}

std::string ExpressionRegistry::description(std::string_view name) const {
  const auto it = mEntries.find(name);
  return it == mEntries.end() ? std::string() : it->second.description;
}

void SectionActionRegistry::add(std::string name, SectionActionFn fn, std::string description) {
  mEntries[std::move(name)] = Entry{std::move(fn), std::move(description)};
}

void SectionActionRegistry::remove(std::string_view name) {
  if (auto it = mEntries.find(name); it != mEntries.end())
    mEntries.erase(it);
}

bool SectionActionRegistry::contains(std::string_view name) const { return mEntries.find(name) != mEntries.end(); }

bool SectionActionRegistry::run(std::string_view name, SectionActionContext &context) const {
  const auto it = mEntries.find(name);
  if (it == mEntries.end() || !it->second.fn) {
    log::warning("Unbekannte Post-Action \"{}\" in Arbeitsgang \"{}\"", name, context.section.name);
    return false;
  }
  it->second.fn(context);
  return true;
}

std::vector<std::string> SectionActionRegistry::names() const {
  std::vector<std::string> out;
  for (const auto &[name, entry] : mEntries)
    out.push_back(name);
  return out;
}

std::string SectionActionRegistry::description(std::string_view name) const {
  const auto it = mEntries.find(name);
  return it == mEntries.end() ? std::string() : it->second.description;
}

void stamp(Task &task, const util::Session &session) {
  task.user = session.user();
  task.timestamp = session.timestamp();
}

std::size_t resolveExpressions(Section &section, const ExpressionRegistry &expressions, const util::Session &session) {
  std::size_t count = 0;
  forEachTask(section, [&](Task &task) {
    if (task.expression.empty())
      return;
    if (expressions.resolve(task.expression, task)) {
      stamp(task, session);
      ++count;
    }
  });
  return count;
}

void registerBuiltinActions(SectionActionRegistry &actions, const ExpressionRegistry &expressions) {
  actions.add(
      std::string(kAutoFinishAction), [&expressions](SectionActionContext &ctx) { resolveExpressions(ctx.section, expressions, ctx.session); },
      "Füllt alle Aufgaben mit Expression automatisch aus und meldet den Arbeitsgang beim Abschluss der Checkliste fertig");
}

std::vector<std::string> unfinishedTasks(const Section &section) {
  std::vector<std::string> out;
  for (const auto &item : section.items) {
    if (const auto *task = std::get_if<Task>(&item)) {
      if (!task->isDone())
        out.push_back(task->name);
    } else if (const auto *group = std::get_if<TaskGroup>(&item)) {
      for (const auto &t : group->tasks)
        if (!t.isDone())
          out.push_back(t.name);
    }
  }
  return out;
}

std::vector<std::string> prepareSectionFinish(Section &section, const ExpressionRegistry &expressions, const util::Session &session) {
  resolveExpressions(section, expressions, session);
  return unfinishedTasks(section);
}

void finishSection(Checklist &checklist, Section &section, const SectionActionRegistry &actions, const util::Session &session) {
  section.user = session.user();
  section.timestamp = session.timestamp();
  section.finished = true;
  if (!section.postAction.empty()) {
    SectionActionContext ctx{checklist, section, session};
    actions.run(section.postAction, ctx);
  }
}

void reopenSection(Section &section, const util::Session &session) {
  section.finished = false;
  section.user = session.user();
  section.timestamp = session.timestamp();
}

void finishChecklist(Checklist &checklist, const ExpressionRegistry &expressions, const SectionActionRegistry &actions, const util::Session &session) {
  checklist.user = session.user();
  checklist.timestamp = session.timestamp();
  checklist.finished = true;
  for (auto &section : checklist.sections) {
    if (section.postAction != kAutoFinishAction)
      continue;
    resolveExpressions(section, expressions, session);
    if (!section.finished)
      finishSection(checklist, section, actions, session);
  }
}

} // namespace st::checklist
