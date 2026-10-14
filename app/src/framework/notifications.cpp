#include "framework/notifications.h"

#include <st/util/log.h>

#include <imgui.h>

namespace st::app {

namespace {
constexpr std::size_t kMaxToasts = 6;

std::chrono::seconds lifetime(Notifications::Kind kind) {
  return kind == Notifications::Kind::Error ? std::chrono::seconds(10) : std::chrono::seconds(4);
}

ImVec4 colorOf(Notifications::Kind kind) {
  switch (kind) {
  case Notifications::Kind::Success: return ImVec4(0.30f, 0.80f, 0.40f, 1.0f);
  case Notifications::Kind::Warning: return ImVec4(0.95f, 0.75f, 0.20f, 1.0f);
  case Notifications::Kind::Error: return ImVec4(0.95f, 0.30f, 0.30f, 1.0f);
  case Notifications::Kind::Info: break;
  }
  return ImVec4(0.55f, 0.75f, 1.0f, 1.0f);
}
} // namespace

void Notifications::push(Kind kind, std::string text) {
  switch (kind) {
  case Kind::Error: log::error("{}", text); break;
  case Kind::Warning: log::warning("{}", text); break;
  default: log::info("{}", text); break;
  }
  // Do not stack identical messages
  if (!mToasts.empty() && mToasts.back().text == text && mToasts.back().kind == kind) {
    mToasts.back().created = std::chrono::steady_clock::now();
    return;
  }
  mToasts.push_back(Toast{kind, std::move(text), std::chrono::steady_clock::now()});
  while (mToasts.size() > kMaxToasts)
    mToasts.pop_front();
}

void Notifications::draw() {
  const auto now = std::chrono::steady_clock::now();
  std::erase_if(mToasts, [&](const Toast &t) { return now - t.created > lifetime(t.kind); });

  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  const float pad = 12.0f;
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking;

  float y = viewport->WorkPos.y + viewport->WorkSize.y - pad;
  if (!mStatusLine.empty()) {
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - pad, y), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.55f);
    if (ImGui::Begin("##st_statusline", nullptr, flags | ImGuiWindowFlags_NoInputs)) {
      ImGui::TextDisabled("%s", mStatusLine.c_str());
      y -= ImGui::GetWindowHeight() + 6.0f;
    }
    ImGui::End();
  }

  int index = 0;
  for (auto it = mToasts.rbegin(); it != mToasts.rend(); ++it, ++index) {
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - pad, y), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.92f);
    const std::string id = "##st_toast" + std::to_string(index);
    bool dismissed = false;
    if (ImGui::Begin(id.c_str(), nullptr, flags)) {
      ImGui::PushStyleColor(ImGuiCol_Text, colorOf(it->kind));
      ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
      ImGui::TextUnformatted(it->text.c_str());
      ImGui::PopTextWrapPos();
      ImGui::PopStyleColor();
      if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        dismissed = true;
      if (ImGui::IsWindowHovered())
        it->created = now; // keep visible while hovered
      y -= ImGui::GetWindowHeight() + 6.0f;
    }
    ImGui::End();
    if (dismissed) {
      mToasts.erase(std::next(it).base());
      break;
    }
  }
}

} // namespace st::app
