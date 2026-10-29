#include "st/checklist/model.h"

#include <algorithm>
#include <cmath>

namespace st::checklist {

std::string_view valueTypeName(ValueType type) {
  switch (type) {
  case ValueType::Bool: return "BOOL";
  case ValueType::Int: return "INT";
  case ValueType::Double: return "DOUBLE";
  case ValueType::Text: return "STRING";
  case ValueType::TextBox: return "TEXTBOX";
  case ValueType::Combo: return "COMBO";
  case ValueType::Date: return "DATEPICKER";
  }
  return "";
}

ValueType valueTypeOf(const Value &value) {
  return std::visit([](const auto &v) {
    using T = std::decay_t<decltype(v)>;
    if constexpr (std::is_same_v<T, bool>)
      return ValueType::Bool;
    else if constexpr (std::is_same_v<T, int>)
      return ValueType::Int;
    else if constexpr (std::is_same_v<T, double>)
      return ValueType::Double;
    else if constexpr (std::is_same_v<T, Text>)
      return ValueType::Text;
    else if constexpr (std::is_same_v<T, TextBox>)
      return ValueType::TextBox;
    else if constexpr (std::is_same_v<T, Combo>)
      return ValueType::Combo;
    else
      return ValueType::Date;
  },
                    value);
}

Value defaultValue(ValueType type) {
  switch (type) {
  case ValueType::Bool: return false;
  case ValueType::Int: return 0;
  case ValueType::Double: return 0.0;
  case ValueType::Text: return Text{};
  case ValueType::TextBox: return TextBox{};
  case ValueType::Combo: return Combo{};
  case ValueType::Date: return Date{};
  }
  return 0;
}

Section *Checklist::findSection(std::string_view name) {
  auto it = std::find_if(sections.begin(), sections.end(), [&](const Section &s) { return s.name == name; });
  return it == sections.end() ? nullptr : &*it;
}

const Section *Checklist::findSection(std::string_view name) const {
  return const_cast<Checklist *>(this)->findSection(name);
}

double normalizeHours(double hours) {
  // Work in whole minutes to avoid floating point drift (1.30 = 1h 30min).
  const bool negative = hours < 0.0;
  const double absHours = std::abs(hours);
  long long whole = static_cast<long long>(absHours);
  long long minutes = std::llround((absHours - static_cast<double>(whole)) * 100.0);
  whole += minutes / 60;
  minutes %= 60;
  const double result = static_cast<double>(whole) + static_cast<double>(minutes) / 100.0;
  return negative ? -result : result;
}

double totalHours(const Checklist &checklist) {
  long long minutes = 0;
  for (const auto &section : checklist.sections) {
    const double normalized = normalizeHours(section.time);
    const long long whole = static_cast<long long>(normalized);
    minutes += whole * 60 + std::llround((normalized - static_cast<double>(whole)) * 100.0);
  }
  return static_cast<double>(minutes / 60) + static_cast<double>(minutes % 60) / 100.0;
}

bool alignTableRows(TaskTable &table) {
  bool changed = false;
  const auto &columns = table.templateRow.tasks;
  for (auto &row : table.rows) {
    std::vector<Task> aligned;
    aligned.reserve(columns.size());
    std::vector<bool> used(row.tasks.size(), false);
    for (std::size_t c = 0; c < columns.size(); ++c) {
      const Task &column = columns[c];
      // Prefer the cell with the same name, fall back to the same position.
      std::size_t match = row.tasks.size();
      for (std::size_t i = 0; i < row.tasks.size(); ++i) {
        if (!used[i] && row.tasks[i].name == column.name) {
          match = i;
          break;
        }
      }
      if (match == row.tasks.size() && c < row.tasks.size() && !used[c] && valueTypeOf(row.tasks[c].value) == valueTypeOf(column.value))
        match = c;
      if (match < row.tasks.size() && valueTypeOf(row.tasks[match].value) == valueTypeOf(column.value)) {
        used[match] = true;
        Task cell = row.tasks[match];
        cell.name = column.name;
        cell.tooltip = column.tooltip;
        cell.expression = column.expression;
        cell.fmt = column.fmt;
        if (auto *combo = std::get_if<Combo>(&cell.value)) {
          combo->options = std::get<Combo>(column.value).options;
          if (std::find(combo->options.begin(), combo->options.end(), combo->selected) == combo->options.end())
            combo->selected.clear();
        }
        aligned.push_back(std::move(cell));
      } else {
        aligned.push_back(column);
      }
    }
    if (aligned != row.tasks) {
      row.tasks = std::move(aligned);
      changed = true;
    }
  }
  return changed;
}

std::string tableLayoutKey(const TaskTable &table) {
  return "st.checklist.table/" + table.name + "/" + std::to_string(table.templateRow.tasks.size());
}

} // namespace st::checklist
