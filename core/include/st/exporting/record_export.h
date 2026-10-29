#pragma once

#include "st/checklist/export.h"
#include "st/translate/translator.h"
#include "st/util/fs.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

/// Service record export ("Record erstellen", "E-Mail erstellen"): optional translation,
/// HTML rendering and printing to PDF with a Chromium based browser in headless mode.
/// Runs without UI, so the whole pipeline is unit testable with fakes.
namespace st::exporting {

/// Command line arguments (without the executable) that print `html` to `pdf` with
/// Edge/Chrome. The browser's own header/footer is switched off; the record brings its
/// own (HtmlOptions::pageHeaderFooter). Headless mode runs with a temporary profile,
/// apart from a browser window the user has open. No --user-data-dir: with a fresh
/// profile folder Chromium keeps running and never writes the PDF.
std::vector<std::string> browserPrintArguments(const util::fs::path &html, const util::fs::path &pdf);

/// "file:///C:/a%20b/x.html" – percent encoded, forward slashes.
std::string fileUrl(const util::fs::path &path);

/// Quotes one argument for a Windows command line (CommandLineToArgvW rules).
std::string quoteWindowsArgument(std::string_view argument);

/// Possible browser executables below the given program folders (e.g.
/// %ProgramFiles(x86)%, %ProgramFiles%, %LocalAppData%): all msedge.exe first, then
/// chrome.exe as fallback.
std::vector<util::fs::path> browserCandidates(const std::vector<util::fs::path> &programDirs);

/// Prints an HTML file into a PDF file.
using PdfPrinter = std::function<util::Status(const util::fs::path &html, const util::fs::path &pdf)>;

struct RecordRequest {
  /// Sections to print (already selected, e.g. recordSubset/emailSubset).
  checklist::Checklist checklist;
  checklist::HtmlOptions html;
  /// Output path without extension; ".html" / ".pdf" is appended. Old files are
  /// replaced; if one is still open (locked), " (2)", " (3)" ... is appended.
  util::fs::path outputBase;
  bool translate = false;
  bool pdf = true;
};

struct RecordResult {
  /// The finished record (PDF, or HTML if no PDF could be created).
  util::fs::path file;
  bool isPdf = false;
  /// Problems that did not stop the export (e.g. PDF not possible -> HTML).
  std::vector<std::string> warnings;
  /// Lines translated by the translation service (not from the cache).
  std::size_t translatedLines = 0;
};

/// Creates the record. A failing translation is an error (a half translated record is
/// useless); a failing PDF print falls back to the HTML file with a warning.
/// `translator` is needed when `request.translate` is set, `printer` when `request.pdf`.
util::Result<RecordResult> createRecord(RecordRequest request, translate::Translator *translator, const PdfPrinter &printer);

} // namespace st::exporting
