#pragma once

#include <st/util/fs.h>

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

} // namespace st::app::platform
