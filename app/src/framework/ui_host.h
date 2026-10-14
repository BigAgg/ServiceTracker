#pragma once

#include <functional>
#include <string>


namespace st::app {

/// Registers windows, menus and popups at cpp_app's WindowControl.
///
/// WindowControl only accepts plain function pointers; UiHost maps std::function
/// callbacks (lambdas with captures) onto a fixed pool of generated trampolines,
/// so modules can register member functions comfortably.
///
/// Window names are part of the docking layout (imgui.ini). Keep them stable!
class UiHost {
public:
  using DrawFn = std::function<void()>;

  /// Dockable window. `flags` are ImGuiWindowFlags.
  void addWindow(const std::string &name, DrawFn draw, bool openByDefault = true, int flags = 0);
  /// Entry in the main menu bar; `draw` renders the menu content.
  void addMenu(const std::string &name, DrawFn draw);
  /// Global popup that can be opened from any window with ImGui::OpenPopup(name).
  void addPopup(const std::string &name, DrawFn draw, bool modal = true);

  /// Shows the "unsaved" dot in the window tab (visual hint only).
  void setAttention(const std::string &window, bool attention);
  void setOpen(const std::string &window, bool open);
  /// Brings a window to the front (call inside the frame).
  void focus(const std::string &window);

  /// Number of free callback slots (diagnostics).
  static std::size_t freeSlots();
};

} // namespace st::app
