#include "framework/platform.h"

#include <st/util/log.h>

#include <utils/filedialog.h>

#include <iterator>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <cstdlib>
#endif

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

} // namespace st::app::platform
