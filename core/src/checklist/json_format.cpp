#include "st/checklist/formats.h"

#include "st/util/json.h"
#include "st/util/strings.h"

namespace st::checklist {

using json = util::json::Json;

namespace {

using util::json::array;
using util::json::boolean;
using util::json::number;
using util::json::str;
using util::json::strings;

Task taskFromJson(const json &j) {
  Task task;
  task.name = str(j, "name");
  task.comment = str(j, "comment");
  task.user = str(j, "user");
  task.timestamp = str(j, "timestamp");
  task.expression = str(j, "expression");
  task.tooltip = str(j, "tooltip");
  task.fmt = str(j, "fmt");
  task.attachments = strings(j, "attachments");

  const std::string type = str(j, "type");
  const json value = j.contains("value") ? j["value"] : json();
  if (type == "bool") {
    task.value = value.is_boolean() ? value.get<bool>() : (value.is_number() && value.get<double>() != 0.0);
  } else if (type == "int") {
    task.value = value.is_number() ? static_cast<int>(value.get<double>()) : 0;
  } else if (type == "double") {
    task.value = value.is_number() ? value.get<double>() : 0.0;
  } else if (type == "string") {
    const std::string text = value.is_string() ? value.get<std::string>() : std::string();
    if (boolean(j, "multiline"))
      task.value = TextBox{text};
    else
      task.value = Text{text};
  } else if (type == "textbox") {
    task.value = TextBox{value.is_string() ? value.get<std::string>() : std::string()};
  } else if (type == "combo") {
    Combo combo;
    if (value.is_object()) {
      combo.selected = str(value, "selected");
      combo.options = strings(value, "options");
    }
    task.value = std::move(combo);
  } else if (type == "datepicker") {
    task.value = Date{value.is_string() ? value.get<std::string>() : std::string()};
  }
  return task;
}

TaskGroup groupFromJson(const json &j) {
  TaskGroup group;
  if (!j.is_object())
    return group;
  group.name = str(j, "name");
  group.singleTick = boolean(j, "singleTick");
  for (const auto &item : array(j, "items"))
    group.tasks.push_back(taskFromJson(item));
  return group;
}

TaskTable tableFromJson(const json &j) {
  TaskTable table;
  table.name = str(j, "name");
  table.timestamp = str(j, "timestamp");
  if (j.contains("templateLine"))
    table.templateRow = groupFromJson(j["templateLine"]);
  for (const auto &row : array(j, "rows"))
    table.rows.push_back(groupFromJson(row));
  return table;
}

Section sectionFromJson(const json &j) {
  Section section;
  section.name = str(j, "name");
  section.time = number(j, "time");
  section.user = str(j, "user");
  section.timestamp = str(j, "timestamp");
  section.finished = boolean(j, "finished");
  section.postAction = normalizePostAction(str(j, "postAction"));
  section.postActionArgs = str(j, "postActionArgs");
  section.permissions = strings(j, "permissions");
  section.emailExport = boolean(j, "emailExport", true);
  if (j.contains("postPopup") && j["postPopup"].is_object()) {
    const auto &p = j["postPopup"];
    section.postPopup = {str(p, "title"), str(p, "header"), str(p, "text")};
  }
  for (const auto &item : array(j, "items")) {
    if (!item.is_object() || !item.contains("data"))
      continue;
    const std::string kind = str(item, "kind");
    const auto &data = item["data"];
    if (kind == "info")
      section.items.emplace_back(taskFromJson(data));
    else if (kind == "line")
      section.items.emplace_back(groupFromJson(data));
    else if (kind == "table")
      section.items.emplace_back(tableFromJson(data));
  }
  return section;
}

json toJson(const Task &task) {
  json j;
  j["name"] = task.name;
  j["comment"] = task.comment;
  j["attachments"] = task.attachments;
  j["user"] = task.user;
  j["timestamp"] = task.timestamp;
  j["expression"] = task.expression;
  j["tooltip"] = task.tooltip;
  j["fmt"] = task.fmt;
  std::visit([&](const auto &v) {
    using T = std::decay_t<decltype(v)>;
    if constexpr (std::is_same_v<T, bool>) {
      j["type"] = "bool";
      j["value"] = v;
    } else if constexpr (std::is_same_v<T, int>) {
      j["type"] = "int";
      j["value"] = v;
    } else if constexpr (std::is_same_v<T, double>) {
      j["type"] = "double";
      j["value"] = v;
    } else if constexpr (std::is_same_v<T, Text>) {
      j["type"] = "string";
      j["value"] = v.value;
    } else if constexpr (std::is_same_v<T, TextBox>) {
      j["type"] = "textbox";
      j["value"] = v.value;
    } else if constexpr (std::is_same_v<T, Combo>) {
      j["type"] = "combo";
      j["value"] = {{"selected", v.selected}, {"options", v.options}};
    } else if constexpr (std::is_same_v<T, Date>) {
      j["type"] = "datepicker";
      j["value"] = v.value;
    }
  },
             task.value);
  return j;
}

json toJson(const TaskGroup &group) {
  json j;
  j["name"] = group.name;
  j["singleTick"] = group.singleTick;
  j["items"] = json::array();
  for (const auto &task : group.tasks)
    j["items"].push_back(toJson(task));
  return j;
}

json toJson(const TaskTable &table) {
  json j;
  j["name"] = table.name;
  j["timestamp"] = table.timestamp;
  // UI state of the legacy program, kept for compatibility
  j["enteradd"] = false;
  j["cfocus"] = -1;
  j["rfocus"] = -1;
  j["templateLine"] = toJson(table.templateRow);
  j["rows"] = json::array();
  for (const auto &row : table.rows)
    j["rows"].push_back(toJson(row));
  return j;
}

json toJson(const Section &section) {
  json j;
  j["name"] = section.name;
  j["time"] = section.time;
  j["user"] = section.user;
  j["timestamp"] = section.timestamp;
  j["finished"] = section.finished;
  j["permissions"] = section.permissions;
  j["postAction"] = section.postAction;
  j["postActionArgs"] = section.postActionArgs;
  j["postPopup"] = {{"title", section.postPopup.title}, {"header", section.postPopup.header}, {"text", section.postPopup.text}};
  j["emailExport"] = section.emailExport;
  j["items"] = json::array();
  for (const auto &item : section.items) {
    std::visit([&](const auto &value) {
      using T = std::decay_t<decltype(value)>;
      if constexpr (std::is_same_v<T, Task>)
        j["items"].push_back({{"kind", "info"}, {"data", toJson(value)}});
      else if constexpr (std::is_same_v<T, TaskGroup>)
        j["items"].push_back({{"kind", "line"}, {"data", toJson(value)}});
      else
        j["items"].push_back({{"kind", "table"}, {"data", toJson(value)}});
    },
               item);
  }
  return j;
}

} // namespace

util::Result<Checklist> parseJson(std::string_view content) {
  json j = json::parse(content, nullptr, false);
  if (j.is_discarded() || !j.is_object())
    return std::unexpected(util::Error{"Checkliste ist kein gültiges JSON"});
  Checklist checklist;
  checklist.version = str(j, "version");
  checklist.user = str(j, "user");
  checklist.timestamp = str(j, "timestamp");
  checklist.finished = boolean(j, "finished");
  checklist.permissions = strings(j, "permissions");
  for (const auto &section : array(j, "sections"))
    if (section.is_object())
      checklist.sections.push_back(sectionFromJson(section));
  return checklist;
}

std::string writeJson(const Checklist &checklist, std::string_view attachmentsDir, std::int64_t lastEdit) {
  json j;
  j["version"] = checklist.version;
  j["user"] = checklist.user;
  j["timestamp"] = checklist.timestamp;
  j["attachmentsDir"] = std::string(attachmentsDir);
  j["finished"] = checklist.finished;
  j["lastedit"] = lastEdit;
  j["permissions"] = checklist.permissions;
  j["sections"] = json::array();
  for (const auto &section : checklist.sections)
    j["sections"].push_back(toJson(section));
  return util::json::dump(j);
}

Format formatForPath(const util::fs::path &path) {
  return util::toLower(util::pathToUtf8(path.extension())) == ".json" ? Format::Json : Format::LegacyText;
}

util::Result<Checklist> parse(std::string_view content, Format format) {
  if (format == Format::Json)
    return parseJson(content);
  return parseLegacyText(content);
}

std::string write(const Checklist &checklist, Format format) {
  if (format == Format::Json)
    return writeJson(checklist);
  return writeLegacyText(checklist);
}

} // namespace st::checklist
