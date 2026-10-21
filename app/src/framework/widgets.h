#pragma once

#include <imgui.h>

#include <string>
#include <string_view>
#include <vector>

/// Small reusable UI building blocks. Use them instead of repeating ImGui
/// boilerplate in modules, so the program behaves the same everywhere.
namespace st::app::ui {

/// RAII BeginDisabled/EndDisabled.
class Disabled {
public:
  explicit Disabled(bool disabled) : mDisabled(disabled) {
    if (mDisabled)
      ImGui::BeginDisabled();
  }
  ~Disabled() {
    if (mDisabled)
      ImGui::EndDisabled();
  }
  Disabled(const Disabled &) = delete;
  Disabled &operator=(const Disabled &) = delete;

private:
  bool mDisabled;
};

/// RAII PushID/PopID.
class ScopedId {
public:
  explicit ScopedId(const void *ptr) { ImGui::PushID(ptr); }
  explicit ScopedId(const char *str) { ImGui::PushID(str); }
  explicit ScopedId(int i) { ImGui::PushID(i); }
  ~ScopedId() { ImGui::PopID(); }
  ScopedId(const ScopedId &) = delete;
  ScopedId &operator=(const ScopedId &) = delete;
};

/// Background stripe behind one entry of a list (header plus expanded content):
/// every second entry is highlighted and a thin line separates the entries.
/// Everything drawn while the object lives is placed on top of the stripe, so an
/// expanded entry keeps its color over its full height.
///
///   for (std::size_t i = 0; i < ids.size(); ++i) {
///     ui::StripedRow row(i);
///     if (ImGui::TreeNode(...)) { ...; ImGui::TreePop(); }
///   }
class StripedRow {
public:
  explicit StripedRow(std::size_t index);
  ~StripedRow();
  StripedRow(const StripedRow &) = delete;
  StripedRow &operator=(const StripedRow &) = delete;

private:
  ImDrawListSplitter mSplitter;
  ImDrawList *mDrawList;
  ImVec2 mStart;
  bool mHighlighted;
};

/// Search input with hint, clear button ("x") and Ctrl+F focus when the window is focused.
/// Returns true when the text changed.
bool searchField(const char *id, std::string &text, const char *hint = "suchen... (Strg+F)");

/// Status per inputfield (in Module as member integrated
struct AutocompleteState {
  int selected = -1;
  bool refocus = false;
  bool popupHovered = false;
  bool dismissed = false;
  struct {
    float width = 0.0f;
    struct vec2{
      float x = 0.0f;
      float y = 0.0f;
    };
    vec2 Min = {};
    vec2 Max = {};
  } popupRect;
};

/// Converting UTF-8-String in relative lowercase
/// Handles ASCII aswell as the german characters (Ä/Ö/Ü -> ä/ö/ü)
std::string foldCase(std::string_view s);

// core: matches filter - prefix-matches first then "contains", case-insensitive, max. 'limit'
std::vector<std::string_view> matchSuggestions(std::string_view query, const std::vector<std::string> &candidates, size_t limit);

/// Search input with autocomplete
/// Returns true if the value changes
bool autocompleteInput(const char *label, std::string &value, const std::vector<std::string> &candidates, AutocompleteState &st);

/// "(?)" marker with tooltip.
void help(const char *text);

/// Red hint text (validation messages).
void errorText(std::string_view text);
/// Muted hint text.
void hintText(std::string_view text);

/// Button that asks "Wirklich ...?" in a small popup. Returns true when confirmed.
bool confirmButton(const char *label, const char *question, bool disabled = false);

/// Color used for a device/service status.
ImVec4 statusColor(std::string_view status);
/// Status text in its color.
void statusBadge(std::string_view status);

/// Multiline input that grows with its content.
bool inputMultiline(const char *label, std::string &text, float minLines = 3.0f, ImGuiInputTextFlags flags = 0);

/// Small "Heute" button that writes today's date (format "YYYY.MM.DD") into `date`.
bool todayButton(const char *id, std::string &date, const std::string &today);

/// Right-click context menu "Kopieren" for the last item; copies `text` to the clipboard.
void copyContextMenu(const char *id, const std::string &text);

/// Selectable list of strings. Returns true when `selected` changed.
bool listBox(const char *id, const std::vector<std::string> &items, std::string &selected, float heightInItems = 8.0f);

} // namespace st::app::ui
