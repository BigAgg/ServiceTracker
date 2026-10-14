#include "framework/ui_host.h"

#include <st/util/log.h>

#include <imgui.h>
#include <imgui/ImGui_windowcontrol.h>

#include <array>
#include <stdexcept>
#include <utility>

namespace st::app {

namespace {

constexpr std::size_t kSlotCount = 128;

std::array<UiHost::DrawFn, kSlotCount> &slots() {
  static std::array<UiHost::DrawFn, kSlotCount> s;
  return s;
}

std::size_t &usedSlots() {
  static std::size_t used = 0;
  return used;
}

template <std::size_t I>
void trampoline() {
  if (const auto &fn = slots()[I])
    fn();
}

template <std::size_t... I>
constexpr std::array<Function, sizeof...(I)> makeTable(std::index_sequence<I...>) {
  return {&trampoline<I>...};
}

constexpr auto kTable = makeTable(std::make_index_sequence<kSlotCount>{});

Function bind(UiHost::DrawFn fn) {
  auto &used = usedSlots();
  if (used >= kSlotCount)
    throw std::runtime_error("UiHost: no free callback slots, increase kSlotCount");
  slots()[used] = std::move(fn);
  return kTable[used++];
}

} // namespace

void UiHost::addWindow(const std::string &name, DrawFn draw, bool openByDefault, int flags) {
  WindowControl::Get().RegisterWindow(name, openByDefault, bind(std::move(draw)), flags);
}

void UiHost::addMenu(const std::string &name, DrawFn draw) {
  WindowControl::Get().RegisterMenu(name, bind(std::move(draw)));
}

void UiHost::addPopup(const std::string &name, DrawFn draw, bool modal) {
  WindowControl::Get().RegisterPopup(name, modal, bind(std::move(draw)));
}

void UiHost::setAttention(const std::string &window, bool attention) {
  auto &info = WindowControl::Get().GetWindowInfo(window);
  if (attention)
    info.set_flag(ImGuiWindowFlags_UnsavedDocument);
  else
    info.clear_flag(ImGuiWindowFlags_UnsavedDocument);
}

void UiHost::setOpen(const std::string &window, bool open) {
  WindowControl::Get().SetWindowState(window, open);
}

void UiHost::focus(const std::string &window) {
  setOpen(window, true);
  ImGui::SetWindowFocus(window.c_str());
}

std::size_t UiHost::freeSlots() { return kSlotCount - usedSlots(); }

} // namespace st::app
