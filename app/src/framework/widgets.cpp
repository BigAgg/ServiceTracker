#include "framework/widgets.h"

#include <st/service/records.h>
#include <st/util/strings.h>

#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <array>
#include "widgets.h"
#include <imgui_internal.h>

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

/// Removes empty/tabs at the start and end of a string
static std::string_view trim(std::string_view s) {
  const auto first = s.find_first_not_of(" \t");
  if (first == std::string_view::npos) return {};
  const auto last = s.find_last_not_of(" \t");
  return s.substr(first, last - first + 1);
}

/// Check if position 'pos' a new word starts after space or typical spacer
static bool isWordStart (std::string_view s, size_t pos) {
  if (pos == 0) return true;
  const char prev = s[pos - 1];
  return prev == ' ' || prev == '\t' || prev == '-' || prev == '_' || prev == '/' || prev == '.' ||
         prev == ',' || prev == '(' || prev == '&';
}

/// Rank the candidates
static int rank (std::string_view hay, std::string_view needle) {
  size_t pos = hay.find(needle);
  if (pos == std::string_view::npos) return -1;
  if (pos == 0) return 0;
  for (; pos != std::string_view::npos; pos = hay.find (needle, pos + 1)) {
    if (isWordStart(hay, pos)) return 1;
  }
  return 2;
}

std::string foldCase(std::string_view s) {
  std::string out(s);
  for (size_t i = 0; i < out.size (); ++i) {
    const auto c = static_cast<unsigned char>(out[i]);
    if (c >= 'A' && c <= 'Z') {
      out[i] = static_cast<char>(c + ('a' - 'A'));
    } else if (c == 0xC3 && i + 1 < out.size()) {
      const auto n = static_cast<unsigned char>(out[i + 1]);
      if (n == 0x84 || n == 0x96 || n == 0x9C) out[i + 1] = static_cast<char>(n + 0x20);
      ++i;
    }
  }
  return out;
}

std::vector<std::string_view> matchSuggestions(std::string_view query, const std::vector<std::string> &candidates, size_t limit) {
  std::vector<std::string_view> result;
  const std::string needle = foldCase(trim(query));
  if (needle.empty() || limit == 0) return result;
  
  std::array<std::vector<std::string_view>, 3 > tiers;
  for (const auto& candidate : candidates) {
    const int r = rank(foldCase(candidate), needle);
    if (r >= 0) tiers[r].emplace_back(candidate);
  }

  for (const auto& tier : tiers) {
    for (auto sv : tier) {
      if (result.size() >= limit) return result;
      result.push_back(sv);
    }
  }
  return result;
}

bool autocompleteInput(const char *label, std::string &value, const std::vector<std::string> &candidates, AutocompleteState &st) {
  auto matches = matchSuggestions(value, candidates, 8);

  struct Ctx {
    AutocompleteState *st;
    const decltype(matches) *m;
  } ctx{&st, &matches};
  auto cb = [](ImGuiInputTextCallbackData *d) -> int {
    auto &c = *static_cast<Ctx *>(d->UserData);
    if (d->EventFlag == ImGuiInputTextFlags_CallbackHistory && !c.m->empty ()) {
      int n = (int)c.m->size();
      c.st->selected = (c.st->selected + (d->EventKey == ImGuiKey_DownArrow ? 1 : n - 1 + (c.st->selected < 0))) % n;
    }
    else if (d->EventFlag == ImGuiInputTextFlags_CallbackCompletion && c.st->selected >= 0) {
      auto s = (*c.m)[c.st->selected];
      d->DeleteChars(0, d->BufTextLen);
      d->InsertChars(0, s.data(), s.data() + s.size());
      c.st->dismissed = true;
    } else if (d->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
      c.st->dismissed = false;
      c.st->selected = -1;
    }
    return 0;
  };

  if (st.refocus) {
    ImGui::SetKeyboardFocusHere();
    st.refocus = false;
  }
  bool changed = ImGui::InputText(label, &value,
                                  ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackEdit,
                                  cb, &ctx);
  
  if (matches.size () >= 1 && st.selected == -1) {
    st.selected = 0;
  }

  if (ImGui::IsKeyPressed (ImGuiKey_Enter) && st.selected >= 0) {
		value = matches[st.selected];
		changed = true;
		st.dismissed = true;
		st.refocus = true;
	}

  const bool active = ImGui::IsItemActive();
  if (active && ImGui::IsKeyPressed(ImGuiKey_Escape)) st.dismissed = true;

  const bool popupHovered = st.popupRect.width > 0.0f &&
                            ImGui::IsMouseHoveringRect(ImVec2(st.popupRect.Min.x, st.popupRect.Min.y),
                                                       ImVec2(st.popupRect.Max.x, st.popupRect.Max.y),
                                                       false);

  const bool show = !matches.empty() && !st.dismissed && (active || popupHovered)
    && !(matches.size() == 1 && matches[0] == value);
  st.popupRect = {};
  if (!show) return changed;

  ImGui::SetNextWindowPos({ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y});
  ImGui::SetNextWindowSize({ImGui::GetItemRectSize().x, 0});
  const ImGuiWindowFlags wf = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                              ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoDocking |
                              ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;
  ImGui::PushID(label);
  if (ImGui::Begin ("##autocomplete", nullptr, wf)) {
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    for (int i = 0; i < (int)matches.size (); ++i) {
      std::string s{matches[i]};
      if (ImGui::Selectable (s.c_str (), i == st.selected)) {
        value = s;
        changed = true;
        st.dismissed = true;
        st.refocus = true;
      }
    }
    auto rect = ImGui::GetCurrentWindow()->Rect();
    st.popupRect.width = rect.GetWidth();
    st.popupRect.Max = {rect.Max.x, rect.Max.y};
    st.popupRect.Min = {rect.Min.x, rect.Min.y};
  }
  ImGui::End();
  ImGui::PopID();
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
