#pragma once

#include "st/util/fs.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

/// Minimal Excel writer (libxlsxwriter). Column widths and row heights are fitted
/// to the content automatically.
namespace st::exporting {

using Cell = std::variant<std::monostate, double, long long, bool, std::string>;
using Row = std::vector<Cell>;

struct Sheet {
  std::string name;
  std::vector<Row> rows;
  /// Rows printed in bold (e.g. table headers).
  std::vector<std::size_t> boldRows;
};

/// False if the program was built without Excel support (ST_WITH_XLSX=OFF).
bool xlsxAvailable();

/// Excel restricts sheet names (31 chars, no []:*?/\); invalid names are fixed,
/// duplicates get a suffix.
std::string sanitizeSheetName(std::string_view name, const std::vector<std::string> &existing);

/// Writes a workbook.
util::Status writeXlsx(const util::fs::path &file, const std::vector<Sheet> &sheets);

} // namespace st::exporting
