#include "st/exporting/record_export.h"

#include "st/checklist/translation.h"
#include "st/util/strings.h"

#include <format>

namespace st::exporting {

namespace {

bool isUnreserved(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
         c == '~' || c == '/' || c == ':';
}

util::fs::path withExtension(const util::fs::path &base, const char *extension) {
  util::fs::path out = base;
  out += extension;
  return out;
}

} // namespace

std::vector<std::string> browserPrintArguments(const util::fs::path &html, const util::fs::path &pdf) {
  return {
      "--headless",
      "--disable-gpu",
      "--no-first-run",
      "--no-default-browser-check",
      "--disable-extensions",
      "--run-all-compositor-stages-before-draw",
      "--no-pdf-header-footer",
      "--print-to-pdf=" + util::pathToUtf8(pdf.lexically_normal().make_preferred()),
      fileUrl(html),
  };
}

std::string fileUrl(const util::fs::path &path) {
  std::string p = util::pathToUtf8(path);
  util::replaceAll(p, "\\", "/");
  std::string prefix;
  if (p.starts_with("//"))
    prefix = "file:"; // UNC: file://server/share
  else if (p.starts_with("/"))
    prefix = "file://";
  else
    prefix = "file:///";
  std::string encoded;
  for (const char ch : p) {
    const auto c = static_cast<unsigned char>(ch);
    if (isUnreserved(c))
      encoded += ch;
    else
      encoded += std::format("%{:02X}", c);
  }
  return prefix + encoded;
}

std::string quoteWindowsArgument(std::string_view argument) {
  if (!argument.empty() && argument.find_first_of(" \t\n\v\"") == std::string_view::npos)
    return std::string(argument);
  std::string out = "\"";
  std::size_t backslashes = 0;
  for (const char c : argument) {
    if (c == '\\') {
      ++backslashes;
      continue;
    }
    if (c == '"')
      out.append(backslashes * 2 + 1, '\\');
    else
      out.append(backslashes, '\\');
    backslashes = 0;
    out += c;
  }
  out.append(backslashes * 2, '\\'); // keep them before the closing quote
  return out + "\"";
}

std::vector<util::fs::path> browserCandidates(const std::vector<util::fs::path> &programDirs) {
  std::vector<util::fs::path> out;
  for (const auto &dir : programDirs)
    if (!dir.empty())
      out.push_back(dir / "Microsoft" / "Edge" / "Application" / "msedge.exe");
  for (const auto &dir : programDirs)
    if (!dir.empty())
      out.push_back(dir / "Google" / "Chrome" / "Application" / "chrome.exe");
  return out;
}

util::Result<RecordResult> createRecord(RecordRequest request, translate::Translator *translator, const PdfPrinter &printer) {
  RecordResult result;
  if (request.translate) {
    if (!translator)
      return std::unexpected(util::Error{"Kein Übersetzer verfügbar."});
    if (auto s = checklist::translateChecklist(request.checklist, *translator); !s)
      return std::unexpected(util::Error{"Übersetzung fehlgeschlagen: " + s.error().message});
    result.translatedLines = translator->lastBackendLines();
  }

  // a record still open in a viewer cannot be replaced: use "<name> (2)" etc. instead
  util::fs::path base = request.outputBase;
  for (int n = 2; n < 50; ++n) {
    if (util::removeAll(withExtension(base, ".pdf")) && util::removeAll(withExtension(base, ".html")))
      break;
    base = request.outputBase;
    base += " (" + std::to_string(n) + ")";
  }
  const auto htmlFile = withExtension(base, ".html");
  const auto pdfFile = withExtension(base, ".pdf");
  const bool wantPdf = request.pdf && printer;
  if (request.pdf && !printer)
    result.warnings.push_back("PDF-Erstellung nicht verfügbar – der Record wird als HTML erstellt.");
  if (request.html.documentName.empty())
    request.html.documentName = util::pathToUtf8((wantPdf ? pdfFile : htmlFile).filename());
  request.html.pageHeaderFooter = request.html.pageHeaderFooter || wantPdf;

  if (auto s = util::writeFileAtomic(htmlFile, checklist::exportHtml(request.checklist, request.html)); !s)
    return std::unexpected(s.error());
  result.file = htmlFile;
  if (!wantPdf)
    return result;

  if (auto s = printer(htmlFile, pdfFile); !s) {
    result.warnings.push_back("PDF konnte nicht erstellt werden (" + s.error().message + ") – der Record wird als HTML geöffnet.");
    return result;
  }
  if (!util::pathExists(pdfFile)) {
    result.warnings.push_back("Der Browser hat kein PDF erzeugt – der Record wird als HTML geöffnet.");
    return result;
  }
  (void)util::removeAll(htmlFile);
  result.file = pdfFile;
  result.isPdf = true;
  return result;
}

} // namespace st::exporting
