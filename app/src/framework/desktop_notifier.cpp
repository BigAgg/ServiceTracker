#include "framework/desktop_notifier.h"

#if defined(_WIN32) && !defined(ST_APP_COMPILE_CHECK)

#include "framework/platform.h"

#include <st/util/log.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <string_view>
#include <vector>

// windows.h must come first (separate block, so clang-format does not sort it)
#include <windows.h>

#include <shellapi.h>
#include <shobjidl.h>

namespace st::app {
namespace {

constexpr UINT kTrayMessage = WM_APP + 42;
constexpr UINT kTrayId = 1;
constexpr wchar_t kWindowClass[] = L"ServiceTrackerNotifier";
constexpr wchar_t kBadgeDescription[] = L"Neue Benachrichtigungen";

std::wstring widen(const std::string &utf8) {
  if (utf8.empty())
    return {};
  const int size = static_cast<int>(utf8.size());
  const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), size, nullptr, 0);
  std::wstring out(static_cast<std::size_t>(len), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), size, out.data(), len);
  return out;
}

/// Copies into a fixed NOTIFYICONDATA buffer, truncating if necessary.
template <std::size_t N>
void copyTruncated(wchar_t (&target)[N], const std::wstring &source) {
  const std::size_t n = std::min(source.size(), N - 1);
  std::copy_n(source.data(), n, target);
  target[n] = L'\0';
}

/// Red dot with a thin white ring (taskbar overlay), anti-aliased 32 bit icon.
HICON createDotIcon(int size) {
  BITMAPV5HEADER header{};
  header.bV5Size = sizeof(header);
  header.bV5Width = size;
  header.bV5Height = -size; // top-down
  header.bV5Planes = 1;
  header.bV5BitCount = 32;
  header.bV5Compression = BI_BITFIELDS;
  header.bV5RedMask = 0x00FF0000;
  header.bV5GreenMask = 0x0000FF00;
  header.bV5BlueMask = 0x000000FF;
  header.bV5AlphaMask = 0xFF000000;

  void *bits = nullptr;
  HDC screen = GetDC(nullptr);
  HBITMAP color = CreateDIBSection(screen, reinterpret_cast<BITMAPINFO *>(&header), DIB_RGB_COLORS, &bits, nullptr, 0);
  ReleaseDC(nullptr, screen);
  if (!color || !bits)
    return nullptr;

  const double center = (size - 1) / 2.0;
  const double outer = size / 2.0 - 0.5;
  const double inner = outer - std::max(1.0, size / 12.0);
  auto *pixels = static_cast<std::uint32_t *>(bits);
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const double dist = std::hypot(x - center, y - center);
      const double alpha = std::clamp(outer + 0.5 - dist, 0.0, 1.0);
      const double red = std::clamp(inner + 0.5 - dist, 0.0, 1.0); // 1 = red, 0 = white ring
      const auto mix = [&](double redChannel) { return static_cast<std::uint32_t>(std::lround(255.0 + (redChannel - 255.0) * red)); };
      const std::uint32_t r = mix(0xE8);
      const std::uint32_t g = mix(0x11);
      const std::uint32_t b = mix(0x23);
      const auto a = static_cast<std::uint32_t>(std::lround(alpha * 255.0));
      pixels[y * size + x] = (a << 24) | (r << 16) | (g << 8) | b;
    }
  }

  std::vector<BYTE> maskBits(static_cast<std::size_t>(((size + 15) / 16) * 2 * size), 0);
  HBITMAP mask = CreateBitmap(size, size, 1, 1, maskBits.data());
  ICONINFO info{};
  info.fIcon = TRUE;
  info.hbmMask = mask;
  info.hbmColor = color;
  HICON icon = CreateIconIndirect(&info);
  DeleteObject(mask);
  DeleteObject(color);
  return icon;
}

struct MainWindowSearch {
  DWORD process = 0;
  HWND exclude = nullptr;
  HWND glfw = nullptr;
  HWND any = nullptr;
};

BOOL CALLBACK findMainWindow(HWND hwnd, LPARAM param) {
  auto *search = reinterpret_cast<MainWindowSearch *>(param);
  DWORD process = 0;
  GetWindowThreadProcessId(hwnd, &process);
  if (process != search->process || hwnd == search->exclude || GetWindow(hwnd, GW_OWNER) || !IsWindowVisible(hwnd))
    return TRUE;
  wchar_t className[64] = {};
  GetClassNameW(hwnd, className, static_cast<int>(std::size(className)));
  if (std::wstring_view(className) == L"GLFW30") {
    search->glfw = hwnd;
    return FALSE;
  }
  if (!search->any)
    search->any = hwnd;
  return TRUE;
}

class WindowsNotifier final : public DesktopNotifier {
public:
  WindowsNotifier() {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    mComInitialized = SUCCEEDED(com);
    if (SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&mTaskbar)))) {
      if (FAILED(mTaskbar->HrInit())) {
        mTaskbar->Release();
        mTaskbar = nullptr;
      }
    }
    if (!mTaskbar)
      log::warning("Taskleisten-Symbol (roter Punkt) nicht verfügbar");

    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &WindowsNotifier::windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);
    // Hidden top-level window (not HWND_MESSAGE): only top-level windows receive the
    // "TaskbarCreated" broadcast after an Explorer restart.
    mWindow = CreateWindowExW(WS_EX_TOOLWINDOW, kWindowClass, L"ServiceTracker Benachrichtigungen", WS_POPUP,
                              0, 0, 0, 0, nullptr, nullptr, instance, this);
    mTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    const auto iconFile = (platform::executableDir() / "icon.ico").wstring();
    mTrayIcon = loadIcon(iconFile, GetSystemMetrics(SM_CXSMICON));
    if (!mTrayIcon) // icon.ico missing: standard application icon (IDI_APPLICATION)
      mTrayIcon = CopyIcon(LoadIconW(nullptr, MAKEINTRESOURCEW(32512)));
    mBalloonIcon = loadIcon(iconFile, GetSystemMetrics(SM_CXICON));
    mDotIcon = createDotIcon(GetSystemMetrics(SM_CXSMICON));
  }

  ~WindowsNotifier() override {
    removeTray();
    if (mTaskbar) {
      if (HWND main = mainWindow())
        mTaskbar->SetOverlayIcon(main, nullptr, L"");
      mTaskbar->Release();
    }
    if (mWindow)
      DestroyWindow(mWindow);
    UnregisterClassW(kWindowClass, GetModuleHandleW(nullptr));
    for (HICON icon : {mTrayIcon, mBalloonIcon, mDotIcon})
      if (icon)
        DestroyIcon(icon);
    if (mComInitialized)
      CoUninitialize();
  }

  WindowsNotifier(const WindowsNotifier &) = delete;
  WindowsNotifier &operator=(const WindowsNotifier &) = delete;

  void show(const alerts::Notification &notification) override {
    if (!ensureTray())
      return;
    NOTIFYICONDATAW data = baseData();
    data.uFlags = NIF_INFO;
    copyTruncated(data.szInfoTitle, widen(notification.title));
    copyTruncated(data.szInfo, widen(notification.body));
    data.dwInfoFlags = NIIF_RESPECT_QUIET_TIME;
    if (mBalloonIcon) {
      data.dwInfoFlags |= NIIF_USER | NIIF_LARGE_ICON;
      data.hBalloonIcon = mBalloonIcon;
    } else {
      data.dwInfoFlags |= NIIF_INFO;
    }
    if (Shell_NotifyIconW(NIM_MODIFY, &data))
      mLastChannel = notification.channel;
    else
      log::warning("Windows-Benachrichtigung konnte nicht angezeigt werden");
  }

  void setBadge(bool visible) override {
    HWND main = mainWindow();
    if (!mTaskbar || !main || (visible == mBadgeShown && main == mBadgeWindow))
      return;
    if (SUCCEEDED(mTaskbar->SetOverlayIcon(main, visible ? mDotIcon : nullptr, visible ? kBadgeDescription : L""))) {
      mBadgeShown = visible;
      mBadgeWindow = main;
    }
  }

  void setTooltip(const std::string &text) override {
    mTooltip = widen(text);
    if (!mTrayAdded)
      return;
    NOTIFYICONDATAW data = baseData();
    data.uFlags = NIF_TIP | NIF_SHOWTIP;
    copyTruncated(data.szTip, mTooltip);
    Shell_NotifyIconW(NIM_MODIFY, &data);
  }

  std::optional<Activation> takeActivation() override {
    auto result = std::move(mActivation);
    mActivation.reset();
    return result;
  }

  bool appInForeground() const override {
    HWND main = mainWindow();
    return main && GetForegroundWindow() == main && !IsIconic(main);
  }

  void activateApp() override {
    HWND main = mainWindow();
    if (!main)
      return;
    ShowWindow(main, IsIconic(main) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(main);
  }

private:
  static HICON loadIcon(const std::wstring &file, int size) {
    return static_cast<HICON>(LoadImageW(nullptr, file.c_str(), IMAGE_ICON, size, size, LR_LOADFROMFILE));
  }

  static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
      const auto *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    if (auto *self = reinterpret_cast<WindowsNotifier *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA)))
      if (self->handle(message, lParam))
        return 0;
    return DefWindowProcW(hwnd, message, wParam, lParam);
  }

  /// Returns true if the message was handled.
  bool handle(UINT message, LPARAM lParam) {
    if (message == kTrayMessage) {
      switch (LOWORD(lParam)) {
      case NIN_BALLOONUSERCLICK:
        mActivation = Activation{mLastChannel};
        activateApp();
        return true;
      case NIN_SELECT:
      case NIN_KEYSELECT:
        mActivation = Activation{};
        activateApp();
        return true;
      default:
        return false;
      }
    }
    if (mTaskbarCreated != 0 && message == mTaskbarCreated) {
      // Explorer was restarted: tray icon and overlay are gone
      const bool hadTray = mTrayAdded;
      mTrayAdded = false;
      mBadgeShown = false;
      mBadgeWindow = nullptr;
      if (hadTray)
        ensureTray();
      return true;
    }
    return false;
  }

  NOTIFYICONDATAW baseData() const {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = mWindow;
    data.uID = kTrayId;
    return data;
  }

  bool ensureTray() {
    if (mTrayAdded)
      return true;
    if (!mWindow)
      return false;
    NOTIFYICONDATAW data = baseData();
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = kTrayMessage;
    data.hIcon = mTrayIcon;
    copyTruncated(data.szTip, mTooltip);
    if (!Shell_NotifyIconW(NIM_ADD, &data)) {
      log::warning("Infobereich-Symbol konnte nicht angelegt werden");
      return false;
    }
    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
    mTrayAdded = true;
    return true;
  }

  void removeTray() {
    if (!mTrayAdded)
      return;
    NOTIFYICONDATAW data = baseData();
    Shell_NotifyIconW(NIM_DELETE, &data);
    mTrayAdded = false;
  }

  /// The raylib/GLFW window of this process (searched again if it was recreated).
  HWND mainWindow() const {
    if (mMainWindow && IsWindow(mMainWindow))
      return mMainWindow;
    MainWindowSearch search;
    search.process = GetCurrentProcessId();
    search.exclude = mWindow;
    EnumWindows(&findMainWindow, reinterpret_cast<LPARAM>(&search));
    mMainWindow = search.glfw ? search.glfw : search.any;
    return mMainWindow;
  }

  bool mComInitialized = false;
  ITaskbarList3 *mTaskbar = nullptr;
  HWND mWindow = nullptr;
  mutable HWND mMainWindow = nullptr;
  UINT mTaskbarCreated = 0;
  HICON mTrayIcon = nullptr;
  HICON mBalloonIcon = nullptr;
  HICON mDotIcon = nullptr;
  bool mTrayAdded = false;
  bool mBadgeShown = false;
  HWND mBadgeWindow = nullptr;
  std::wstring mTooltip = L"ServiceTracker";
  std::string mLastChannel;
  std::optional<Activation> mActivation;
};

} // namespace

std::unique_ptr<DesktopNotifier> DesktopNotifier::create() { return std::make_unique<WindowsNotifier>(); }

} // namespace st::app

#else

namespace st::app {

std::unique_ptr<DesktopNotifier> DesktopNotifier::create() { return std::make_unique<RecordingNotifier>(); }

} // namespace st::app

#endif
