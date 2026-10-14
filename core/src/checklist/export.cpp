#include "st/checklist/export.h"

#include "st/util/strings.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <sstream>

namespace st::checklist {

using util::htmlEscape;

namespace {

std::string groupThousands(std::string digits) {
  std::string out;
  const std::size_t n = digits.size();
  for (std::size_t i = 0; i < n; ++i) {
    if (i > 0 && (n - i) % 3 == 0)
      out += '.';
    out += digits[i];
  }
  return out;
}

const char *kDefaultHead = R"(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
@page { size: A4; margin: 15mm 15mm 15mm 15mm; }
@media print {
  body { background: white; }
  .section { page-break-inside: avoid; break-inside: avoid; }
  .table-wrapper { page-break-inside: avoid; break-inside: avoid; }
  a { color: black; text-decoration: none; }
}
body { font-family: 'Segoe UI', 'Helvetica Neue', Arial, sans-serif; font-size: 10pt; line-height: 1.4; color: #333; background: #f5f5f5; padding: 10px; }
.header { background: #1a1a1a; color: white; padding: 20px 15px; margin-bottom: 20px; text-align: center; border-radius: 4px; }
.header-title { font-size: 24pt; font-weight: bold; letter-spacing: 1px; }
.metadata { background: white; border-left: 4px solid #1a1a1a; padding: 12px 15px; margin-bottom: 20px; font-size: 9pt; line-height: 1.6; border-radius: 2px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
.metadata-item { display: inline-block; margin-right: 20px; }
.metadata-label { font-weight: 600; color: #1a1a1a; }
.section { background: white; margin-bottom: 18px; border-radius: 4px; box-shadow: 0 1px 4px rgba(0,0,0,0.08); overflow: hidden; page-break-inside: avoid; }
.section-header { background: linear-gradient(135deg, #4a4a4a 0%, #5a5a5a 100%); color: white; padding: 12px 15px; border-bottom: 3px solid #ddd; }
.section-title { font-size: 13pt; font-weight: bold; margin-bottom: 6px; letter-spacing: 0.5px; }
.section-meta { font-size: 8.5pt; opacity: 0.9; line-height: 1.5; }
.section-content { padding: 12px 15px; }
.table-title { font-size: 11pt; font-weight: 600; color: #1a1a1a; margin: 12px 0 8px 0; padding-bottom: 6px; border-bottom: 2px solid #ddd; }
.table-wrapper { margin-bottom: 12px; page-break-inside: avoid; }
table { width: 100%; border-collapse: collapse; background: white; font-size: 9pt; }
th { background: #6b4a9f; color: white; font-weight: 600; padding: 8px 6px; text-align: left; border: 1px solid #1a1a1a; word-wrap: break-word; }
td { padding: 6px 6px; border: 1px solid #ccc; vertical-align: top; word-wrap: break-word; }
tr:nth-child(even) { background: #f9f9f9; }
tr:nth-child(odd) { background: white; }
.multiline { white-space: pre-wrap; word-break: break-word; max-width: 500px; }
table.info-table { margin-bottom: 10px; }
table.info-table th { width: 30%; background: #6b4a9f; }
table.info-table td { width: 70%; }
.footer { margin-top: 30px; padding-top: 15px; border-top: 2px solid #ddd; text-align: center; font-size: 8pt; color: #666; }
</style>
</head>
<body>
)";

void renderGroup(std::ostringstream &out, const TaskGroup &group) {
  std::vector<const Task *> done;
  for (const auto &task : group.tasks)
    if (task.isDone())
      done.push_back(&task);
  if (done.empty())
    return;
  out << "<div class='table-wrapper'>";
  if (!group.name.empty())
    out << "<div class='table-title'>" << htmlEscape(group.name) << "</div>";
  out << "<table><tr>";
  for (const auto *task : done)
    out << "<th>" << htmlEscape(task->name) << "</th>";
  out << "</tr><tr>";
  for (const auto *task : done)
    out << "<td class='multiline'>" << htmlEscape(displayValue(*task)) << "</td>";
  out << "</tr></table></div>\n";
}

void renderTable(std::ostringstream &out, const TaskTable &table) {
  if (table.rows.empty())
    return;
  out << "<div class='table-wrapper'><div class='table-title'>" << htmlEscape(table.name) << "</div><table><tr>";
  for (const auto &column : table.templateRow.tasks)
    out << "<th>" << htmlEscape(column.name) << "</th>";
  out << "</tr>";
  for (const auto &row : table.rows) {
    out << "<tr>";
    for (const auto &cell : row.tasks)
      out << "<td class='multiline'>" << htmlEscape(displayValue(cell)) << "</td>";
    out << "</tr>";
  }
  out << "</table></div>\n";
}

void renderTask(std::ostringstream &out, const Task &task) {
  if (!task.isDone())
    return;
  const std::string value = displayValue(task);
  if (value.empty())
    return;
  out << "<div class='table-wrapper'><table class='info-table'><tr><th>" << htmlEscape(task.name)
      << "</th><td class='multiline'>" << htmlEscape(value) << "</td></tr></table></div>\n";
}

void renderSection(std::ostringstream &out, const Section &section) {
  out << "<div class='section'><div class='section-header'>";
  out << "<div class='section-title'>" << htmlEscape(section.name) << "</div>";
  out << "<div class='section-meta'>⏱️ Time: " << std::format("{:.2f}", section.time) << " hours";
  if (!section.user.empty())
    out << " | 👤 " << htmlEscape(section.user);
  if (!section.timestamp.empty())
    out << " | 📅 " << htmlEscape(section.timestamp);
  out << "</div></div><div class='section-content'>\n";
  for (const auto &item : section.items) {
    std::visit([&](const auto &value) {
      using T = std::decay_t<decltype(value)>;
      if constexpr (std::is_same_v<T, Task>)
        renderTask(out, value);
      else if constexpr (std::is_same_v<T, TaskGroup>)
        renderGroup(out, value);
      else
        renderTable(out, value);
    },
               item);
  }
  out << "</div></div>\n";
}

std::string csvField(std::string_view value) {
  // Quote fields that would break the ';' separated layout
  if (value.find_first_of(";\"\n\r") == std::string_view::npos)
    return std::string(value);
  return "\"" + util::replaced(std::string(value), "\"", "\"\"") + "\"";
}

void csvGroup(std::ostringstream &out, const TaskGroup &group, bool values, bool headers, bool name) {
  if (name && !group.name.empty())
    out << csvField(group.name) << ";\n";
  if (headers) {
    for (const auto &task : group.tasks)
      out << csvField(task.name) << ";";
    out << "\n";
  }
  if (values) {
    for (const auto &task : group.tasks)
      out << csvField(plainValue(task.value)) << ";";
    out << "\n";
  }
}

} // namespace

std::string formatNumber(double value, std::string_view fmt) {
  int precision = 2;
  std::string suffix;
  if (const auto dot = fmt.find('.'); dot != std::string_view::npos) {
    if (const auto f = fmt.find('f', dot); f != std::string_view::npos)
      precision = util::parseInt(fmt.substr(dot + 1, f - dot - 1)).value_or(2);
  }
  if (const auto f = fmt.find('f'); f != std::string_view::npos)
    suffix = std::string(fmt.substr(f + 1));
  precision = std::clamp(precision, 0, 10);
  std::string fixed = std::format("{:.{}f}", value, precision);
  bool negative = !fixed.empty() && fixed.front() == '-';
  if (negative)
    fixed.erase(0, 1);
  if (fixed.find_first_not_of("0.") == std::string::npos)
    negative = false; // "-0,00" -> "0,00"
  std::string whole = fixed;
  std::string fraction;
  if (const auto dot = fixed.find('.'); dot != std::string::npos) {
    whole = fixed.substr(0, dot);
    fraction = fixed.substr(dot + 1);
  }
  std::string out = negative ? "-" : "";
  out += groupThousands(whole);
  if (!fraction.empty())
    out += "," + fraction;
  return out + suffix;
}

std::string formatInteger(long long value) {
  const std::string digits = std::to_string(value < 0 ? -value : value);
  return (value < 0 ? "-" : "") + groupThousands(digits);
}

std::string displayValue(const Task &task) {
  return std::visit([&](const auto &v) -> std::string {
    using T = std::decay_t<decltype(v)>;
    if constexpr (std::is_same_v<T, double>)
      return formatNumber(v, task.fmt);
    else if constexpr (std::is_same_v<T, int>)
      return formatInteger(v);
    else if constexpr (std::is_same_v<T, bool>)
      return v ? "✅" : "❌";
    else if constexpr (std::is_same_v<T, Combo>)
      return v.selected;
    else
      return v.value;
  },
                    task.value);
}

std::string plainValue(const Value &value) {
  return std::visit([](const auto &v) -> std::string {
    using T = std::decay_t<decltype(v)>;
    if constexpr (std::is_same_v<T, double>)
      return util::formatDoubleLegacy(v);
    else if constexpr (std::is_same_v<T, int>)
      return std::to_string(v);
    else if constexpr (std::is_same_v<T, bool>)
      return v ? "1" : "0";
    else if constexpr (std::is_same_v<T, Combo>)
      return v.selected;
    else
      return v.value;
  },
                    value);
}

std::string retrieveValue(const Checklist &checklist, std::string_view sectionName, std::string_view taskName) {
  const Section *section = checklist.findSection(sectionName);
  if (!section)
    return {};
  for (const auto &item : section->items) {
    std::string out;
    if (const auto *task = std::get_if<Task>(&item)) {
      if (task->name == taskName)
        out = plainValue(task->value);
    } else if (const auto *group = std::get_if<TaskGroup>(&item)) {
      for (const auto &t : group->tasks)
        if (t.name == taskName) {
          out = plainValue(t.value);
          break;
        }
    } else if (const auto *table = std::get_if<TaskTable>(&item)) {
      std::vector<std::string> values;
      for (const auto &row : table->rows)
        for (const auto &cell : row.tasks)
          if (cell.name == taskName)
            values.push_back(plainValue(cell.value));
      out = util::join(values, "\n");
    }
    if (!out.empty())
      return out;
  }
  return {};
}

double tableColumnTotal(const Checklist &checklist, std::string_view sectionName, std::string_view tableName, std::string_view columnName) {
  double total = 0.0;
  for (const auto &section : checklist.sections) {
    if (section.name != sectionName)
      continue;
    for (const auto &item : section.items) {
      const auto *table = std::get_if<TaskTable>(&item);
      if (!table || table->name != tableName)
        continue;
      for (const auto &row : table->rows)
        for (const auto &cell : row.tasks) {
          if (cell.name != columnName)
            continue;
          if (const auto *d = std::get_if<double>(&cell.value))
            total += *d;
          else if (const auto *i = std::get_if<int>(&cell.value))
            total += *i;
        }
    }
  }
  return total;
}

std::string exportHtml(const Checklist &checklist, const HtmlOptions &options) {
  std::ostringstream out;
  if (options.customHead.empty()) {
    out << kDefaultHead;
    out << "<div class='header'><div class='header-title'>📋 " << htmlEscape(options.title) << "</div></div>\n";
  } else {
    out << options.customHead;
  }
  out << "<div class='metadata'>";
  out << "<div class='metadata-item'><span class='metadata-label'>📄 Document:</span> " << htmlEscape(options.documentName) << "</div>";
  if (!checklist.user.empty())
    out << "<div class='metadata-item'><span class='metadata-label'>👤 Technician:</span> " << htmlEscape(checklist.user) << "</div>";
  if (!checklist.timestamp.empty())
    out << "<div class='metadata-item'><span class='metadata-label'>📅 Date:</span> " << htmlEscape(checklist.timestamp) << "</div>";
  out << "<div class='metadata-item'><span class='metadata-label'>⏱️ Total Time:</span> " << std::format("{:.2f}", totalHours(checklist)) << " hours</div>";
  out << "<div class='metadata-item'><span class='metadata-label'>Versions</span> App: " << htmlEscape(options.appVersion)
      << " | Checklist: " << htmlEscape(checklist.version) << "</div>";
  out << "</div>\n";
  for (const auto &section : checklist.sections)
    if (section.finished || options.includeUnfinished)
      renderSection(out, section);
  out << "<div class='footer'><p>" << htmlEscape(options.footer) << "</p></div>\n</body>\n</html>\n";
  return out.str();
}

std::string exportCsv(const Checklist &checklist, std::string_view documentPath) {
  std::ostringstream out;
  out << "\xEF\xBB\xBF"; // UTF-8 BOM
  out << "Path:;" << csvField(documentPath) << ";Version:;" << csvField(checklist.version) << ";\n";
  if (checklist.finished)
    out << "Finished:;TRUE;" << csvField(checklist.user) << ";" << csvField(checklist.timestamp) << ";\n";
  else
    out << "Finished:;FALSE;\n";
  for (const auto &section : checklist.sections) {
    out << "\nSection:;" << csvField(section.name) << ";Time Spent:;" << util::formatDoubleLegacy(section.time) << ";";
    out << "User:;" << csvField(section.user) << ";Timestamp:;" << csvField(section.timestamp) << ";\n";
    for (const auto &item : section.items) {
      if (const auto *task = std::get_if<Task>(&item)) {
        out << csvField(task->name) << ":;" << csvField(plainValue(task->value)) << ";";
      } else if (const auto *group = std::get_if<TaskGroup>(&item)) {
        csvGroup(out, *group, true, true, true);
      } else if (const auto *table = std::get_if<TaskTable>(&item)) {
        out << csvField(table->name) << ";\n";
        csvGroup(out, table->templateRow, false, true, true);
        for (const auto &row : table->rows)
          csvGroup(out, row, true, false, false);
      }
    }
  }
  return out.str();
}

Checklist recordSubset(const Checklist &checklist, const std::vector<std::string> &sectionNames) {
  Checklist copy;
  copy.version = checklist.version;
  copy.user = checklist.user;
  copy.timestamp = checklist.timestamp;
  for (const auto &name : sectionNames) {
    if (const Section *section = checklist.findSection(name)) {
      copy.sections.push_back(*section);
      copy.sections.back().finished = true;
    }
  }
  return copy;
}

} // namespace st::checklist
