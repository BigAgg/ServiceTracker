#pragma once

#include <st/util/fs.h>

#include <chrono>
#include <string>
#include <vector>

/// Operating system helpers (Unicode safe on Windows).
namespace st::app::platform {

/// Opens a file or folder with the default application / Explorer.
void openPath(const util::fs::path &path);

/// Opens Explorer with the file selected.
void revealInExplorer(const util::fs::path &path);

/// Native file dialogs (UTF-8 paths, empty if cancelled).
/// `filters` use the cpp_app syntax: {"Checkliste:json,txt"}.
std::string openFileDialog(const std::vector<std::string> &filters = {});
std::string saveFileDialog(const std::vector<std::string> &filters = {});
std::string openDirectoryDialog();

/// Folder of the running executable.
util::fs::path executableDir();

/// Opens a URL (e.g. "mailto:...") with the registered application.
void openUrl(const std::string &url);

/// Starts `executable` with `arguments` (quoted for Windows), without a console
/// window, and waits until it ends. Returns the exit code; a timeout kills the process.
/// Safe to call from worker threads.
util::Result<int> runProcess(const util::fs::path &executable, const std::vector<std::string> &arguments, std::chrono::milliseconds timeout);

/// Browser for the PDF export: `configured` if it exists, otherwise Microsoft Edge
/// (App Paths registry entry, program folders) and Google Chrome. Empty if none.
util::fs::path findPdfBrowser(const std::string &configured);

/// Prints an HTML file to PDF with the browser (headless). Safe in worker threads.
util::Status printHtmlToPdf(const util::fs::path &browser, const util::fs::path &html, const util::fs::path &pdf);

} // namespace st::app::platform
