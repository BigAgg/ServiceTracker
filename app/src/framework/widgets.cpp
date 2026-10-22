#include "framework/widgets.h"

#include <st/service/records.h>
#include <st/util/strings.h>
#include <st/util/suggestions.h>

#include <misc/cpp/imgui_stdlib.h>

#include <imgui_internal.h>

#include <algorithm>

namespace st::app::ui {

StripedRow::StripedRow(std::size_t index)
    : mDrawList(ImGui::GetWindowDrawList()), mStart(ImGui::GetCursorScreenPos()), mHighlighted(index % 2 == 1) {
  // Content goes to channel 1, the stripe is added to channel 0 (behind) once the
  // height of the (possibly expanded) entry is known.
  mSplitter.Split(mDrawList, 2);
  mSplitter.SetCurrentChannel(mDrawList, 1);
}

StripedRow::~StripedRow() {
  const ImGuiStyle &style = ImGui::GetStyle();
  const float halfGap = style.ItemSpacing.y * 0.5f;
  const float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
  const ImVec2 min(mStart.x - style.FramePadding.x, mStart.y - halfGap);
  const ImVec2 max(right, ImGui::GetCursorScreenPos().y - halfGap);
  mSplitter.SetCurrentChannel(mDrawList, 0);
  if (mHighlighted)
    mDrawList->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_Header, 0.35f), style.FrameRounding);
  mDrawList->AddLine(ImVec2(min.x, max.y), max, ImGui::GetColorU32(ImGuiCol_Separator, 0.5f));
  mSplitter.Merge(mDrawList);
}

bool searchField(const char *id, std::string &text, const char *hint) {
  ScopedId scope(id);
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F))
    ImGui::SetKeyboardFocusHere();
  const float clearWidth = text.empty() ? 0.0f : ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
  ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - clearWidth));
  bool changed = ImGui::InputTextWithHint("##search", hint, &text);
  if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape) && !text.empty()) {
    text.clear();
    changed = true;
  }
  if (!text.empty()) {
    ImGui::SameLine();
    if (ImGui::Button("x", ImVec2(ImGui::GetFrameHeight(), 0.0f))) {
      text.clear();
      changed = true;
    }
    ImGui::SetItemTooltip("Suche leeren (Esc)");
  }
  return changed;
}

bool autocompleteInput(const char *label, std::string &value, const std::vector<std::string> &candidates, AutocompleteState &st) {
  const auto matches = util::matchSuggestions(value, candidates, 8);

  struct CallbackData {
    AutocompleteState *state;
    const std::vector<std::string_view> *matches;
  } data{&st, &matches};
  const auto callback = [](ImGuiInputTextCallbackData *d) -> int {
    auto &c = *static_cast<CallbackData *>(d->UserData);
    if (d->EventFlag == ImGuiInputTextFlags_CallbackHistory && !c.matches->empty()) {
      const int n = static_cast<int>(c.matches->size());
      const int step = d->EventKey == ImGuiKey_DownArrow ? 1 : n - 1;
      c.state->selected = c.state->selected < 0 ? 0 : (c.state->selected + step) % n;
    } else if (d->EventFlag == ImGuiInputTextFlags_CallbackCompletion && c.state->selected >= 0 &&
               c.state->selected < static_cast<int>(c.matches->size())) {
      const auto s = (*c.matches)[static_cast<std::size_t>(c.state->selected)];
      d->DeleteChars(0, d->BufTextLen);
      d->InsertChars(0, s.data(), s.data() + s.size());
      c.state->dismissed = true;
    } else if (d->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
      c.state->dismissed = false;
      c.state->selected = -1;
    }
    return 0;
  };

  if (st.refocus) {
    ImGui::SetKeyboardFocusHere();
    st.refocus = false;
  }
  bool changed = ImGui::InputText(label, &value,
                                  ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackEdit,
                                  callback, &data);
  const bool active = ImGui::IsItemActive();
  if (!matches.empty() && st.selected < 0)
    st.selected = 0;
  if (st.selected >= static_cast<int>(matches.size()))
    st.selected = matches.empty() ? -1 : 0;

  const bool listOpen = !st.dismissed && !matches.empty();
  if (listOpen && ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGuiKey_Enter) && st.selected >= 0) {
    value = std::string(matches[static_cast<std::size_t>(st.selected)]);
    changed = true;
    st.dismissed = true;
    st.refocus = true;
  }
  if (active && ImGui::IsKeyPressed(ImGuiKey_Escape))
    st.dismissed = true;

  const bool popupHovered = st.popupMax.x > st.popupMin.x && ImGui::IsMouseHoveringRect(st.popupMin, st.popupMax, false);
  const bool show = listOpen && (active || popupHovered) && !(matches.size() == 1 && matches[0] == value);
  st.popupMin = st.popupMax = ImVec2();
  if (!show)
    return changed;

  ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y));
  ImGui::SetNextWindowSize(ImVec2(ImGui::GetItemRectSize().x, 0.0f));
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoDocking |
                                      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;
  // Window names are global: derive a unique one from the input's ID
  const std::string window = "##autocomplete" + std::to_string(ImGui::GetItemID());
  if (ImGui::Begin(window.c_str(), nullptr, kFlags)) {
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    for (std::size_t i = 0; i < matches.size(); ++i) {
      const std::string suggestion(matches[i]);
      if (ImGui::Selectable(suggestion.c_str(), static_cast<int>(i) == st.selected)) {
        value = suggestion;
        changed = true;
        st.dismissed = true;
        st.refocus = true;
      }
    }
    const ImRect rect = ImGui::GetCurrentWindow()->Rect();
    st.popupMin = rect.Min;
    st.popupMax = rect.Max;
  }
  ImGui::End();
  return changed;
}

void help(const char *text) {
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::BeginItemTooltip()) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

void errorText(std::string_view text) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.35f, 0.35f, 1.0f));
  ImGui::TextWrapped("%.*s", static_cast<int>(text.size()), text.data());
  ImGui::PopStyleColor();
}

void hintText(std::string_view text) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextWrapped("%.*s", static_cast<int>(text.size()), text.data());
  ImGui::PopStyleColor();
}

bool confirmButton(const char *label, const char *question, bool disabled) {
  ScopedId scope(label);
  {
    Disabled d(disabled);
    if (ImGui::Button(label))
      ImGui::OpenPopup("##confirm");
  }
  bool confirmed = false;
  if (ImGui::BeginPopup("##confirm")) {
    ImGui::TextUnformatted(question);
    ImGui::Separator();
    if (ImGui::Button("Ja")) {
      confirmed = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Abbrechen") || ImGui::IsKeyPressed(ImGuiKey_Escape))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
  return confirmed;
}

ImVec4 statusColor(std::string_view status) {
  namespace s = service::status;
  if (status == s::Done || status == s::DoneExternal)
    return ImVec4(0.45f, 0.75f, 0.45f, 1.0f);
  if (status == s::Expected)
    return ImVec4(0.60f, 0.70f, 0.95f, 1.0f);
  if (status == s::PlannedExternal || status == s::External)
    return ImVec4(0.80f, 0.60f, 0.95f, 1.0f);
  if (status == s::ExternFinished)
    return ImVec4(0.55f, 0.80f, 0.90f, 1.0f);
  if (status.empty())
    return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
  return ImVec4(0.95f, 0.75f, 0.30f, 1.0f);
}

void statusBadge(std::string_view status) {
  ImGui::PushStyleColor(ImGuiCol_Text, statusColor(status));
  if (status.empty())
    ImGui::TextUnformatted("nicht in Liste");
  else
    ImGui::TextUnformatted(status.data(), status.data() + status.size());
  ImGui::PopStyleColor();
}

bool inputMultiline(const char *label, std::string &text, float minLines, ImGuiInputTextFlags flags) {
  const float lines = std::max(minLines, static_cast<float>(std::count(text.begin(), text.end(), '\n') + 2));
  const ImVec2 size(-FLT_MIN, std::min(lines, 20.0f) * ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f);
  return ImGui::InputTextMultiline(label, &text, size, flags);
}

bool todayButton(const char *id, std::string &date, const std::string &today) {
  ScopedId scope(id);
  ImGui::SameLine();
  if (ImGui::SmallButton("Heute")) {
    date = today;
    return true;
  }
  ImGui::SetItemTooltip("Heutiges Datum eintragen");
  return false;
}

void copyContextMenu(const char *id, const std::string &text) {
  if (ImGui::BeginPopupContextItem(id)) {
    if (ImGui::MenuItem("Kopieren"))
      ImGui::SetClipboardText(text.c_str());
    ImGui::EndPopup();
  }
}

bool listBox(const char *id, const std::vector<std::string> &items, std::string &selected, float heightInItems) {
  bool changed = false;
  const ImVec2 size(-FLT_MIN, heightInItems * ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.0f);
  if (ImGui::BeginListBox(id, size)) {
    for (const auto &item : items) {
      const bool isSelected = item == selected;
      if (ImGui::Selectable(item.c_str(), isSelected)) {
        selected = item;
        changed = true;
      }
      if (isSelected && ImGui::IsWindowAppearing())
        ImGui::SetScrollHereY();
    }
    ImGui::EndListBox();
  }
  return changed;
}

} // namespace st::app::ui
