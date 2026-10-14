// Minimal replacements for the Windows-only parts of cpp_app so the complete UI can
// be rendered headless on Linux (st_app_smoke). Only used by the smoke test.
#include <app/app.h>
#include <utils/filedialog.h>
#include <utils/logging.h>
#include <utils/stringconverter.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <vector>

namespace smoke {
/// Files the next HasDropfiles/GetDropfiles call returns (set by the test).
std::vector<std::string> pendingDrops;
} // namespace smoke

namespace app {
bool HasDropfiles() { return !smoke::pendingDrops.empty(); }
std::vector<std::string> GetDropfiles(const std::string &) {
  auto files = std::move(smoke::pendingDrops);
  smoke::pendingDrops.clear();
  return files;
}
} // namespace app

std::string OpenFileDialog(const std::vector<std::string> &, std::string) { return {}; }
std::string SaveFileDialog(const std::vector<std::string> &, std::string) { return {}; }
std::string OpenDirectoryDialog() { return {}; }
void OpenPath(const std::string &) {}

std::pair<std::string, std::string> split_at(const std::string &input, const std::string &delimiter) {
  const auto pos = input.find(delimiter);
  if (pos == std::string::npos)
    return {input, ""};
  return {input.substr(0, pos), input.substr(pos + delimiter.size())};
}

std::string to_lower(const std::string &s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

namespace logging {
void log(const std::string &type, const std::string &msg, const std::source_location &) {
  if (type != "[INFO]")
    std::fprintf(stderr, "%s %s\n", type.c_str(), msg.c_str());
}
} // namespace logging
