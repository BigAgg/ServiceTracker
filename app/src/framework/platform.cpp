#include "framework/platform.h"

#include <st/exporting/record_export.h>
#include <st/util/log.h>

#include <utils/filedialog.h>

#include <iterator>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif
#include <cstdlib>

namespace st::app::platform {

void openPath(const util::fs::path &path) {
  if (path.empty())
    return;
  if (!util::pathExists(path)) {
    log::warning("Pfad existiert nicht: {}", util::pathToUtf8(path));
    return;
  }
#if defined(ST_APP_COMPILE_CHECK)
  log::info("openPath: {}", util::pathToUtf8(path)); // headless smoke test
#elif defined(_WIN32)
  ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
#else
  const std::string cmd = "xdg-open \"" + util::pathToUtf8(path) + "\" >/dev/null 2>&1 &";
  [[maybe_unused]] const int rc = std::system(cmd.c_str());
#endif
}

void revealInExplorer(const util::fs::path &path) {
#if defined(_WIN32) && !defined(ST_APP_COMPILE_CHECK)
  const std::wstring args = L"/select,\"" + path.wstring() + L"\"";
  ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWDEFAULT);
#else
  openPath(path.parent_path());
#endif
}

std::string openFileDialog(const std::vector<std::string> &filters) {
  return filters.empty() ? OpenFileDialog() : OpenFileDialog(filters);
}

std::string saveFileDialog(const std::vector<std::string> &filters) {
  return filters.empty() ? SaveFileDialog() : SaveFileDialog(filters);
}

std::string openDirectoryDialog() { return OpenDirectoryDialog(); }

util::fs::path executableDir() {
#ifdef _WIN32
  wchar_t buffer[MAX_PATH * 4];
  const DWORD len = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
  if (len > 0)
    return util::fs::path(std::wstring(buffer, len)).parent_path();
#else
  std::error_code ec;
  const auto self = util::fs::read_symlink("/proc/self/exe", ec);
  if (!ec)
    return self.parent_path();
#endif
  return util::fs::current_path();
}

void openUrl(const std::string &url) {
#if defined(ST_APP_COMPILE_CHECK)
  log::info("openUrl: {}", url);
#elif defined(_WIN32)
  ShellExecuteW(nullptr, L"open", util::pathFromUtf8(url).wstring().c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
#else
  const std::string cmd = "xdg-open '" + url + "' >/dev/null 2>&1 &";
  [[maybe_unused]] const int rc = std::system(cmd.c_str());
#endif
}

util::Result<int> runProcess(const util::fs::path &executable, const std::vector<std::string> &arguments, std::chrono::milliseconds timeout) {
#if defined(_WIN32) && !defined(ST_APP_COMPILE_CHECK)
  std::string commandLine = exporting::quoteWindowsArgument(util::pathToUtf8(executable));
  for (const auto &argument : arguments)
    commandLine += " " + exporting::quoteWindowsArgument(argument);
  std::wstring wide = util::pathFromUtf8(commandLine).wstring();
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(executable.wstring().c_str(), wide.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
    return std::unexpected(util::Error{"Programm konnte nicht gestartet werden: " + util::pathToUtf8(executable) +
                                       " (Fehler " + std::to_string(GetLastError()) + ")"});
  CloseHandle(process.hThread);
  const DWORD waited = WaitForSingleObject(process.hProcess, static_cast<DWORD>(timeout.count()));
  if (waited != WAIT_OBJECT_0) {
    TerminateProcess(process.hProcess, 1);
    CloseHandle(process.hProcess);
    return std::unexpected(util::Error{"Zeitüberschreitung: " + util::pathToUtf8(executable.filename())});
  }
  DWORD exitCode = 0;
  GetExitCodeProcess(process.hProcess, &exitCode);
  CloseHandle(process.hProcess);
  return static_cast<int>(exitCode);
#else
  (void)arguments;
  (void)timeout;
  return std::unexpected(util::Error{"Externe Programme werden nur unter Windows gestartet: " + util::pathToUtf8(executable)});
#endif
}

util::fs::path findPdfBrowser(const std::string &configured) {
  if (!configured.empty()) {
    const auto path = util::pathFromUtf8(configured);
    if (util::pathExists(path))
      return path;
    log::warning("Browser für PDF nicht gefunden: {}", configured);
  }
#if defined(_WIN32) && !defined(ST_APP_COMPILE_CHECK)
  // Edge registers itself under App Paths, independent of the install folder
  wchar_t buffer[MAX_PATH * 2];
  DWORD size = sizeof(buffer);
  if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\msedge.exe", nullptr, RRF_RT_REG_SZ,
                   nullptr, buffer, &size) == ERROR_SUCCESS) {
    const util::fs::path path(buffer);
    if (util::pathExists(path))
      return path;
  }
  std::vector<util::fs::path> dirs;
  for (const wchar_t *name : {L"ProgramFiles(x86)", L"ProgramFiles", L"LOCALAPPDATA"}) {
    wchar_t value[MAX_PATH * 2];
    const DWORD n = GetEnvironmentVariableW(name, value, static_cast<DWORD>(std::size(value)));
    if (n > 0 && n < std::size(value))
      dirs.emplace_back(std::wstring(value, n));
  }
  for (const auto &candidate : exporting::browserCandidates(dirs))
    if (util::pathExists(candidate))
      return candidate;
#endif
  return {};
}

util::Status printHtmlToPdf(const util::fs::path &browser, const util::fs::path &html, const util::fs::path &pdf) {
  if (browser.empty())
    return std::unexpected(util::Error{"Microsoft Edge wurde nicht gefunden"});
  auto exitCode = runProcess(browser, exporting::browserPrintArguments(html, pdf), std::chrono::seconds(60));
  if (!exitCode)
    return std::unexpected(exitCode.error());
  if (!util::pathExists(pdf))
    return std::unexpected(util::Error{"Browser beendet mit Code " + std::to_string(*exitCode) + ", kein PDF erzeugt"});
  return {};
}

} // namespace st::app::platform
