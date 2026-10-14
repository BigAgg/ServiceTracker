#include "st/exporting/xlsx.h"

#include "st/util/strings.h"

#include <algorithm>
#include <format>

#ifdef ST_WITH_XLSX
#include <xlsxwriter.h>
#endif

namespace st::exporting {

bool xlsxAvailable() {
#ifdef ST_WITH_XLSX
  return true;
#else
  return false;
#endif
}

std::string sanitizeSheetName(std::string_view name, const std::vector<std::string> &existing) {
  std::string out;
  for (const char c : name) {
    if (std::string_view("[]:*?/\\").find(c) != std::string_view::npos)
      out += '_';
    else
      out += c;
  }
  out = util::trim(out);
  if (out.empty())
    out = "Tabelle";
  // Excel counts characters; cutting at 31 bytes may split a UTF-8 sequence, so
  // cut at a character boundary.
  auto cut = [](std::string s, std::size_t maxBytes) {
    if (s.size() <= maxBytes)
      return s;
    std::size_t n = maxBytes;
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80)
      --n;
    return s.substr(0, n);
  };
  out = cut(out, 31);
  const std::string base = out;
  for (int i = 2; std::find_if(existing.begin(), existing.end(), [&](const std::string &e) { return util::toLower(e) == util::toLower(out); }) != existing.end(); ++i) {
    const std::string suffix = std::format(" ({})", i);
    out = cut(base, 31 - suffix.size()) + suffix;
  }
  return out;
}

#ifdef ST_WITH_XLSX
util::Status writeXlsx(const util::fs::path &file, const std::vector<Sheet> &sheets) {
  const std::string path = util::pathToUtf8(file);
  lxw_workbook *workbook = workbook_new(path.c_str());
  if (!workbook)
    return std::unexpected(util::Error{"Excel-Datei kann nicht erstellt werden: " + path});
  lxw_format *bold = workbook_add_format(workbook);
  format_set_bold(bold);
  lxw_format *wrap = workbook_add_format(workbook);
  format_set_text_wrap(wrap);
  format_set_align(wrap, LXW_ALIGN_VERTICAL_TOP);

  std::vector<std::string> names;
  for (const auto &sheet : sheets) {
    const std::string name = sanitizeSheetName(sheet.name, names);
    names.push_back(name);
    lxw_worksheet *ws = workbook_add_worksheet(workbook, name.c_str());
    if (!ws)
      continue;
    std::vector<std::size_t> widths;
    for (std::size_t r = 0; r < sheet.rows.size(); ++r) {
      const bool isBold = std::find(sheet.boldRows.begin(), sheet.boldRows.end(), r) != sheet.boldRows.end();
      std::size_t lines = 1;
      const auto &row = sheet.rows[r];
      if (widths.size() < row.size())
        widths.resize(row.size(), 0);
      for (std::size_t c = 0; c < row.size(); ++c) {
        const auto rr = static_cast<lxw_row_t>(r);
        const auto cc = static_cast<lxw_col_t>(c);
        std::visit([&](const auto &v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, double>) {
            worksheet_write_number(ws, rr, cc, v, isBold ? bold : nullptr);
            widths[c] = std::max(widths[c], std::format("{:.2f}", v).size());
          } else if constexpr (std::is_same_v<T, long long>) {
            worksheet_write_number(ws, rr, cc, static_cast<double>(v), isBold ? bold : nullptr);
            widths[c] = std::max(widths[c], std::to_string(v).size());
          } else if constexpr (std::is_same_v<T, bool>) {
            worksheet_write_boolean(ws, rr, cc, v, isBold ? bold : nullptr);
            widths[c] = std::max<std::size_t>(widths[c], 5);
          } else if constexpr (std::is_same_v<T, std::string>) {
            const bool multiline = v.find('\n') != std::string::npos;
            worksheet_write_string(ws, rr, cc, v.c_str(), isBold ? bold : (multiline ? wrap : nullptr));
            for (const auto &line : util::splitLines(v))
              widths[c] = std::max(widths[c], line.size());
            lines = std::max<std::size_t>(lines, util::splitLines(v).size());
          }
        },
                   row[c]);
      }
      if (lines > 1)
        worksheet_set_row(ws, static_cast<lxw_row_t>(r), 15.0 * static_cast<double>(lines), nullptr);
    }
    for (std::size_t c = 0; c < widths.size(); ++c)
      worksheet_set_column(ws, static_cast<lxw_col_t>(c), static_cast<lxw_col_t>(c), std::clamp(static_cast<double>(widths[c]) + 2.0, 8.0, 80.0), nullptr);
  }
  const lxw_error err = workbook_close(workbook);
  if (err != LXW_NO_ERROR)
    return std::unexpected(util::Error{std::string("Excel-Export fehlgeschlagen: ") + lxw_strerror(err)});
  return {};
}
#else
util::Status writeXlsx(const util::fs::path &, const std::vector<Sheet> &) {
  return std::unexpected(util::Error{"Excel-Export ist in diesem Build nicht verfügbar"});
}
#endif

} // namespace st::exporting
