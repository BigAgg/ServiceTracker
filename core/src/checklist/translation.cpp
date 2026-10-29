#include "st/checklist/translation.h"

namespace st::checklist {

namespace {

void collectTask(Task &task, std::vector<std::string *> &fields) {
  fields.push_back(&task.name);
  std::visit([&](auto &v) {
    using T = std::decay_t<decltype(v)>;
    if constexpr (std::is_same_v<T, Text> || std::is_same_v<T, TextBox>)
      fields.push_back(&v.value);
    else if constexpr (std::is_same_v<T, Combo>)
      fields.push_back(&v.selected);
  },
             task.value);
}

void collectGroup(TaskGroup &group, std::vector<std::string *> &fields) {
  fields.push_back(&group.name);
  for (auto &task : group.tasks)
    collectTask(task, fields);
}

} // namespace

util::Status translateChecklist(Checklist &checklist, translate::Translator &translator) {
  std::vector<std::string *> fields;
  for (auto &section : checklist.sections) {
    fields.push_back(&section.name);
    for (auto &item : section.items) {
      std::visit([&](auto &value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, Task>) {
          collectTask(value, fields);
        } else if constexpr (std::is_same_v<T, TaskGroup>) {
          collectGroup(value, fields);
        } else {
          fields.push_back(&value.name);
          collectGroup(value.templateRow, fields);
          for (auto &row : value.rows)
            collectGroup(row, fields);
        }
      },
                 item);
    }
  }
  std::vector<std::string> texts;
  texts.reserve(fields.size());
  for (const auto *field : fields)
    texts.push_back(*field);
  auto translated = translator.translate(texts);
  if (!translated)
    return std::unexpected(translated.error());
  for (std::size_t i = 0; i < fields.size(); ++i)
    *fields[i] = std::move((*translated)[i]);
  return {};
}

} // namespace st::checklist
