#include "st/checklist/formats.h"

#include "st/checklist/actions.h"
#include "st/util/strings.h"

#include <charconv>
#include <sstream>

namespace st::checklist {

namespace {

using util::parseDouble;
using util::parseInt;
using util::singleLine;

/// Shortest round-trip representation of a double ("1.5", "0", "1234.5678").
std::string formatDouble(double value) {
  char buffer[64];
  const auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);
  if (ec != std::errc())
    return util::formatDoubleLegacy(value);
  return std::string(buffer, ptr);
}

bool startsWith(std::string_view line, std::string_view prefix) { return line.substr(0, prefix.size()) == prefix; }

std::string valueOf(std::string_view line) { return util::splitAt(line, "=").second; }

/// Line cursor over the file content.
class Reader {
public:
  explicit Reader(std::vector<std::string> lines) : mLines(std::move(lines)) {}

  bool next(std::string &line) {
    if (mIndex >= mLines.size())
      return false;
    line = mLines[mIndex++];
    return true;
  }

  /// Reads all lines until `endMarker` and joins them with '\n'.
  std::string readBlock(std::string_view endMarker) {
    std::string out;
    std::string line;
    bool first = true;
    while (next(line)) {
      if (line == endMarker)
        break;
      if (!first)
        out += '\n';
      out += line;
      first = false;
    }
    return out;
  }

  std::vector<std::string> readLines(std::string_view endMarker) {
    std::vector<std::string> out;
    std::string line;
    while (next(line)) {
      if (line == endMarker)
        break;
      out.push_back(line);
    }
    return out;
  }

private:
  std::vector<std::string> mLines;
  std::size_t mIndex = 0;
};

Combo readCombo(Reader &reader) {
  Combo combo;
  std::string line;
  while (reader.next(line)) {
    if (line == "COMBO_END")
      break;
    if (startsWith(line, "SELECTED=")) {
      combo.selected = valueOf(line);
    } else if (line == "OPTIONS_START") {
      for (auto &option : reader.readLines("OPTIONS_END")) {
        if (startsWith(option, "OPTIONS_COUNT="))
          continue;
        combo.options.push_back(std::move(option));
      }
    }
  }
  return combo;
}

Task readTask(Reader &reader) {
  Task task;
  std::string line;
  while (reader.next(line)) {
    if (line == "CHECKINFO_END")
      break;
    if (startsWith(line, "VARIABLE_INT=")) {
      task.value = parseInt(valueOf(line)).value_or(0);
    } else if (startsWith(line, "VARIABLE_DOUBLE=")) {
      task.value = parseDouble(valueOf(line)).value_or(0.0);
    } else if (startsWith(line, "VARIABLE_BOOL=")) {
      task.value = parseInt(valueOf(line)).value_or(0) != 0;
    } else if (startsWith(line, "VARIABLE_STRING=")) {
      task.value = Text{valueOf(line)};
    } else if (startsWith(line, "VARIABLE_DATE=")) {
      task.value = Date{valueOf(line)};
    } else if (line == "TEXTBOX_START") {
      task.value = TextBox{reader.readBlock("TEXTBOX_END")};
    } else if (line == "COMBO_START") {
      task.value = readCombo(reader);
    } else if (startsWith(line, "NAME=")) {
      task.name = valueOf(line);
    } else if (startsWith(line, "EXPRESSION=")) {
      task.expression = valueOf(line);
    } else if (startsWith(line, "FMT=")) {
      task.fmt = valueOf(line);
    } else if (line == "COMMENT_START") {
      task.comment = reader.readBlock("COMMENT_END");
    } else if (line == "ATTACHMENTS_START") {
      task.attachments = reader.readLines("ATTACHMENTS_END");
    } else if (line == "TOOLTIP_START") {
      task.tooltip = reader.readBlock("TOOLTIP_END");
    } else if (startsWith(line, "USER=")) {
      task.user = valueOf(line);
    } else if (startsWith(line, "TIMESTAMP=")) {
      task.timestamp = valueOf(line);
    }
  }
  return task;
}

TaskGroup readGroup(Reader &reader) {
  TaskGroup group;
  std::string line;
  while (reader.next(line)) {
    if (line == "CHECKLINE_END")
      break;
    if (startsWith(line, "SINGLETICK=")) {
      group.singleTick = parseInt(valueOf(line)).value_or(0) != 0;
    } else if (startsWith(line, "NAME=")) {
      group.name = valueOf(line);
    } else if (line == "CHECKINFO_START") {
      group.tasks.push_back(readTask(reader));
    }
  }
  return group;
}

TaskTable readTable(Reader &reader) {
  TaskTable table;
  std::string line;
  while (reader.next(line)) {
    if (line == "CHECKTABLE_END")
      break;
    if (startsWith(line, "NAME=")) {
      table.name = valueOf(line);
    } else if (startsWith(line, "TIMESTAMP=")) {
      table.timestamp = valueOf(line);
    } else if (line == "TEMPLATE_START") {
      // TEMPLATE_START is followed by a CHECKLINE block
      while (reader.next(line) && line != "TEMPLATE_END") {
        if (line == "CHECKLINE_START")
          table.templateRow = readGroup(reader);
      }
    } else if (line == "ROWS_START") {
      while (reader.next(line) && line != "ROWS_END") {
        if (line == "CHECKLINE_START")
          table.rows.push_back(readGroup(reader));
      }
    }
  }
  return table;
}

Section readSection(Reader &reader) {
  Section section;
  std::string line;
  while (reader.next(line)) {
    if (line == "SECTION_END")
      break;
    if (startsWith(line, "NAME=")) {
      section.name = valueOf(line);
    } else if (startsWith(line, "TIME=")) {
      section.time = parseDouble(valueOf(line)).value_or(0.0);
    } else if (startsWith(line, "TIMESTAMP=")) {
      section.timestamp = valueOf(line);
    } else if (startsWith(line, "USER=")) {
      section.user = valueOf(line);
    } else if (startsWith(line, "FINISHED=")) {
      section.finished = parseInt(valueOf(line)).value_or(0) != 0;
    } else if (startsWith(line, "POST_ACTION=")) {
      section.postAction = normalizePostAction(valueOf(line));
    } else if (startsWith(line, "POST_ACTIONARGS=")) {
      section.postActionArgs = valueOf(line);
    } else if (startsWith(line, "POSTPOPUP_TITLE=")) {
      section.postPopup.title = valueOf(line);
    } else if (startsWith(line, "POSTPOPUP_HEADER=")) {
      section.postPopup.header = valueOf(line);
    } else if (line == "POSTPOPUP_TEXT_START") {
      section.postPopup.text = reader.readBlock("POSTPOPUP_TEXT_END");
    } else if (line == "PERMISSIONS_START") {
      section.permissions = reader.readLines("PERMISSIONS_END");
    } else if (startsWith(line, "EMAIL_EXPORT=")) {
      section.emailExport = parseInt(valueOf(line)).value_or(1) != 0;
    } else if (line == "CHECKLINE_START") {
      section.items.emplace_back(readGroup(reader));
    } else if (line == "CHECKINFO_START") {
      section.items.emplace_back(readTask(reader));
    } else if (line == "CHECKTABLE_START") {
      section.items.emplace_back(readTable(reader));
    }
  }
  return section;
}

// ---------------------------------------------------------------------------------
// Writer
// ---------------------------------------------------------------------------------

void writeBlock(std::ostringstream &out, std::string_view start, const std::string &text, std::string_view end) {
  out << start << '\n';
  out << text << '\n';
  out << end << '\n';
}

void writeTask(std::ostringstream &out, const Task &task) {
  out << "CHECKINFO_START\n";
  out << "VERSION=1\n";
  std::visit([&](const auto &v) {
    using T = std::decay_t<decltype(v)>;
    if constexpr (std::is_same_v<T, int>) {
      out << "VARIABLE_INT=" << v << '\n';
    } else if constexpr (std::is_same_v<T, double>) {
      out << "VARIABLE_DOUBLE=" << formatDouble(v) << '\n';
    } else if constexpr (std::is_same_v<T, bool>) {
      out << "VARIABLE_BOOL=" << (v ? 1 : 0) << '\n';
    } else if constexpr (std::is_same_v<T, Text>) {
      out << "VARIABLE_STRING=" << singleLine(v.value) << '\n';
    } else if constexpr (std::is_same_v<T, TextBox>) {
      out << "TEXTBOX_START\n";
      if (!v.value.empty())
        out << v.value << '\n';
      out << "TEXTBOX_END\n";
    } else if constexpr (std::is_same_v<T, Combo>) {
      out << "COMBO_START\n";
      out << "OPTIONS_COUNT=" << v.options.size() << '\n';
      if (!v.selected.empty())
        out << "SELECTED=" << singleLine(v.selected) << '\n';
      out << "OPTIONS_START\n";
      for (const auto &option : v.options)
        out << singleLine(option) << '\n';
      out << "OPTIONS_END\n";
      out << "COMBO_END\n";
    } else if constexpr (std::is_same_v<T, Date>) {
      // Not written by the legacy program (it lost date values); old versions ignore the line.
      out << "VARIABLE_DATE=" << singleLine(v.value) << '\n';
    }
  },
             task.value);
  out << "NAME=" << singleLine(task.name) << '\n';
  if (!task.expression.empty())
    out << "EXPRESSION=" << singleLine(task.expression) << '\n';
  if (!task.fmt.empty())
    out << "FMT=" << singleLine(task.fmt) << '\n';
  if (!task.comment.empty())
    writeBlock(out, "COMMENT_START", task.comment, "COMMENT_END");
  if (!task.attachments.empty()) {
    out << "ATTACHMENTS_COUNT=" << task.attachments.size() << '\n';
    out << "ATTACHMENTS_START\n";
    for (const auto &attachment : task.attachments)
      out << singleLine(attachment) << '\n';
    out << "ATTACHMENTS_END\n";
  }
  if (!task.tooltip.empty())
    writeBlock(out, "TOOLTIP_START", task.tooltip, "TOOLTIP_END");
  if (!task.user.empty())
    out << "USER=" << singleLine(task.user) << '\n';
  if (!task.timestamp.empty())
    out << "TIMESTAMP=" << singleLine(task.timestamp) << '\n';
  out << "CHECKINFO_END\n";
}

void writeGroup(std::ostringstream &out, const TaskGroup &group) {
  out << "CHECKLINE_START\n";
  out << "VERSION=1\n";
  out << "NAME=" << singleLine(group.name) << '\n';
  out << "SINGLETICK=" << (group.singleTick ? 1 : 0) << '\n';
  out << "ITEMS_COUNT=" << group.tasks.size() << '\n';
  for (const auto &task : group.tasks)
    writeTask(out, task);
  out << "CHECKLINE_END\n";
}

void writeTable(std::ostringstream &out, const TaskTable &table) {
  out << "CHECKTABLE_START\n";
  out << "VERSION=1\n";
  out << "NAME=" << singleLine(table.name) << '\n';
  if (!table.timestamp.empty())
    out << "TIMESTAMP=" << singleLine(table.timestamp) << '\n';
  out << "TEMPLATE_START\n";
  writeGroup(out, table.templateRow);
  out << "TEMPLATE_END\n";
  out << "ROWS_COUNT=" << table.rows.size() << '\n';
  if (!table.rows.empty()) {
    out << "ROWS_START\n";
    for (const auto &row : table.rows)
      writeGroup(out, row);
    out << "ROWS_END\n";
  }
  out << "CHECKTABLE_END\n";
}

void writePermissions(std::ostringstream &out, const std::vector<std::string> &permissions) {
  out << "PERMISSIONS_START\n";
  for (const auto &p : permissions)
    out << singleLine(p) << '\n';
  out << "PERMISSIONS_END\n";
}

} // namespace

std::string normalizePostAction(std::string_view name) {
  if (name == "resolve_rinfo")
    return std::string(kAutoFinishAction);
  return std::string(name);
}

Checklist parseLegacyText(std::string_view content) {
  Checklist checklist;
  Reader reader(util::splitLines(content));
  std::string line;
  std::string checklistVersion;
  while (reader.next(line)) {
    if (startsWith(line, "VERSION=")) {
      checklist.version = valueOf(line);
    } else if (startsWith(line, "CHECKLIST_VERSION=")) {
      checklistVersion = valueOf(line);
    } else if (startsWith(line, "USER=")) {
      checklist.user = valueOf(line);
    } else if (startsWith(line, "TIMESTAMP=")) {
      checklist.timestamp = valueOf(line);
    } else if (startsWith(line, "FINISHED=")) {
      checklist.finished = parseInt(valueOf(line)).value_or(0) != 0;
    } else if (line == "PERMISSIONS_START") {
      checklist.permissions = reader.readLines("PERMISSIONS_END");
    } else if (line == "SECTION_START") {
      checklist.sections.push_back(readSection(reader));
    }
  }
  if (!checklistVersion.empty())
    checklist.version = checklistVersion;
  return checklist;
}

std::string writeLegacyText(const Checklist &checklist) {
  std::ostringstream out;
  // The legacy program always writes the format version "1" here.
  out << "VERSION=1\n";
  if (!checklist.version.empty() && checklist.version != "1")
    out << "CHECKLIST_VERSION=" << singleLine(checklist.version) << '\n';
  if (!checklist.user.empty())
    out << "USER=" << singleLine(checklist.user) << '\n';
  if (!checklist.timestamp.empty())
    out << "TIMESTAMP=" << singleLine(checklist.timestamp) << '\n';
  out << "FINISHED=" << (checklist.finished ? 1 : 0) << '\n';
  out << "SECTIONS_COUNT=" << checklist.sections.size() << '\n';
  writePermissions(out, checklist.permissions);
  for (const auto &section : checklist.sections) {
    out << "SECTION_START\n";
    out << "NAME=" << singleLine(section.name) << '\n';
    out << "TIME=" << formatDouble(section.time) << '\n';
    out << "FINISHED=" << (section.finished ? 1 : 0) << '\n';
    if (!section.postPopup.title.empty())
      out << "POSTPOPUP_TITLE=" << singleLine(section.postPopup.title) << '\n';
    if (!section.postPopup.header.empty())
      out << "POSTPOPUP_HEADER=" << singleLine(section.postPopup.header) << '\n';
    if (!section.postPopup.text.empty())
      writeBlock(out, "POSTPOPUP_TEXT_START", section.postPopup.text, "POSTPOPUP_TEXT_END");
    if (!section.user.empty())
      out << "USER=" << singleLine(section.user) << '\n';
    if (!section.timestamp.empty())
      out << "TIMESTAMP=" << singleLine(section.timestamp) << '\n';
    out << "ITEMS_COUNT=" << section.items.size() << '\n';
    if (!section.postAction.empty())
      out << "POST_ACTION=" << singleLine(section.postAction) << '\n';
    if (!section.postActionArgs.empty())
      out << "POST_ACTIONARGS=" << singleLine(section.postActionArgs) << '\n';
    if (!section.permissions.empty())
      writePermissions(out, section.permissions);
    // only written when it differs from the default, so legacy files stay byte identical
    if (!section.emailExport)
      out << "EMAIL_EXPORT=0\n";
    for (const auto &item : section.items) {
      std::visit([&](const auto &value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, Task>)
          writeTask(out, value);
        else if constexpr (std::is_same_v<T, TaskGroup>)
          writeGroup(out, value);
        else
          writeTable(out, value);
      },
                 item);
    }
    out << "SECTION_END\n";
  }
  return out.str();
}

} // namespace st::checklist
