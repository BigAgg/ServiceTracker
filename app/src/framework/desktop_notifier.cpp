#include "framework/desktop_notifier.h"

#if defined(_WIN32) && !defined(ST_APP_COMPILE_CHECK)

#include "framework/platform.h"

#include <st/util/log.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// windows.h must come first (separate block, so clang-format does not sort it)
#include <windows.h>

#include <shellapi.h>
#include <shobjidl.h>

#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Notifications.h>

namespace st::app {
namespace {

constexpr UINT kTrayMessage = WM_APP + 42;
constexpr UINT kTrayId = 1;
constexpr wchar_t kWindowClass[] = L"ServiceTrackerNotifier";
constexpr wchar_t kBadgeDescription[] = L"Neue Benachrichtigungen";
/// Posted to the hidden window when a toast was clicked (toast events run on a worker thread).
constexpr UINT kToastActivated = WM_APP + 43;
/// AppUserModelID of the toasts, registered per user under
/// HKCU\Software\Classes\AppUserModelId (no shortcut or installer change needed).
constexpr wchar_t kAppId[] = L"ServiceTracker.Desktop";

namespace toast = winrt::Windows::UI::Notifications;

std::wstring widen(const std::string &utf8) {
  if (utf8.empty())
    return {};
  const int size = static_cast<int>(utf8.size());
  const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), size, nullptr, 0);
  std::wstring out(static_cast<std::size_t>(len), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), size, out.data(), len);
  return out;
}

/// Escapes text for the toast XML.
std::wstring xmlEscape(const std::wstring &text) {
  std::wstring out;
  out.reserve(text.size());
  for (wchar_t c : text) {
    switch (c) {
    case L'&': out += L"&amp;"; break;
    case L'<': out += L"&lt;"; break;
    case L'>': out += L"&gt;"; break;
    case L'"': out += L"&quot;"; break;
    default: out += c;
    }
  }
  return out;
}

/// Toast tag and group are limited to 64 characters. Tag = notification key (replaces
/// the older toast with the same key), group = channel (withdrawn together).
std::wstring toastId(const std::string &text) { return widen(text).substr(0, 64); }

/// Registers the AppUserModelID with name and icon (shown in the toast header and the
/// Windows notification settings). Written on every start so a moved installation
/// keeps a valid icon path.
bool registerAppId() {
  const std::wstring path = std::wstring(L"Software\\Classes\\AppUserModelId\\") + kAppId;
  HKEY key = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
    return false;
  const auto setString = [key](const wchar_t *name, const std::wstring &value) {
    RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(value.c_str()),
                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
  };
  setString(L"DisplayName", L"ServiceTracker");
  const auto icon = platform::executableDir() / "icon.png";
  if (util::pathExists(icon))
    setString(L"IconUri", icon.wstring());
  RegCloseKey(key);
  return true;
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

    try {
      if (registerAppId())
        mToastNotifier = toast::ToastNotificationManager::CreateToastNotifier(kAppId);
    } catch (const winrt::hresult_error &e) {
      log::warning("Windows-Benachrichtigungen (Toast) nicht verfügbar, verwende Infobereich: {}", winrt::to_string(e.message()));
    }
  }

  ~WindowsNotifier() override {
    {
      // Revoke the click handlers before anything they use is destroyed
      std::lock_guard lock(mToastMutex);
      mToasts.clear();
    }
    mToastNotifier = nullptr;
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

  void show(const alerts::Notification &notification, bool persistent) override {
    ensureTray(); // tooltip with the current state + click opens the program
    if (showToast(notification, persistent))
      return;
    showBalloon(notification);
  }

  void withdraw(const std::string &channel) override {
    {
      std::lock_guard lock(mToastMutex);
      if (std::erase_if(mToasts, [&](const auto &entry) { return entry.second.channel == channel; }) == 0)
        return;
    }
    try {
      toast::ToastNotificationManager::History().RemoveGroup(toastId(channel), kAppId);
    } catch (const winrt::hresult_error &) {
      // already dismissed by the user
    }
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
    if (SetForegroundWindow(main) && GetForegroundWindow() == main)
      return;
    // Windows may refuse the focus change; attaching to the input of the current
    // foreground window is allowed right after the user clicked the notification.
    const DWORD foregroundThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    const DWORD ownThread = GetCurrentThreadId();
    if (foregroundThread != 0 && foregroundThread != ownThread && AttachThreadInput(ownThread, foregroundThread, TRUE)) {
      BringWindowToTop(main);
      SetForegroundWindow(main);
      AttachThreadInput(ownThread, foregroundThread, FALSE);
    }
  }

private:
  /// Modern Windows notification. `persistent` uses the "reminder" scenario: the toast
  /// stays on screen until it is clicked or closed (requires at least one button).
  bool showToast(const alerts::Notification &notification, bool persistent) {
    if (!mToastNotifier)
      return false;
    try {
      std::wstring xml = L"<toast launch=\"open\"";
      if (persistent)
        xml += L" scenario=\"reminder\"";
      xml += L"><visual><binding template=\"ToastGeneric\"><text>" + xmlEscape(widen(notification.title)) +
             L"</text><text>" + xmlEscape(widen(notification.body)) + L"</text></binding></visual>";
      if (persistent)
        xml += L"<actions><action content=\"Öffnen\" arguments=\"open\"/>"
               L"<action content=\"\" arguments=\"dismiss\" activationType=\"system\"/></actions>";
      xml += L"</toast>";

      winrt::Windows::Data::Xml::Dom::XmlDocument doc;
      doc.LoadXml(xml);
      toast::ToastNotification item(doc);
      item.Tag(toastId(notification.key)); // a newer state replaces the old toast
      item.Group(toastId(notification.channel));
      const Activation activation{notification.channel, notification.key};
      auto revoker = item.Activated(winrt::auto_revoke, [this, activation](const toast::ToastNotification &, const winrt::Windows::Foundation::IInspectable &) {
        // Worker thread: hand over to the UI thread
        {
          std::lock_guard lock(mToastMutex);
          mToastActivation = activation;
        }
        PostMessageW(mWindow, kToastActivated, 0, 0);
      });
      mToastNotifier.Show(item);
      std::lock_guard lock(mToastMutex);
      mToasts.insert_or_assign(notification.key, ShownToast{notification.channel, std::move(item), std::move(revoker)});
      return true;
    } catch (const winrt::hresult_error &e) {
      log::warning("Toast-Benachrichtigung fehlgeschlagen, verwende Infobereich: {}", winrt::to_string(e.message()));
      return false;
    }
  }

  /// Fallback: tray balloon (Windows hides it after a few seconds).
  void showBalloon(const alerts::Notification &notification) {
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
      mLastBalloon = Activation{notification.channel, notification.key};
    else
      log::warning("Windows-Benachrichtigung konnte nicht angezeigt werden");
  }

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
    if (message == kToastActivated) {
      {
        std::lock_guard lock(mToastMutex);
        mActivation = std::exchange(mToastActivation, {});
      }
      activateApp();
      return true;
    }
    if (message == kTrayMessage) {
      switch (LOWORD(lParam)) {
      case NIN_BALLOONUSERCLICK:
        mActivation = mLastBalloon;
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
  Activation mLastBalloon;
  std::optional<Activation> mActivation;

  struct ShownToast {
    std::string channel;
    toast::ToastNotification toast;
    toast::ToastNotification::Activated_revoker revoker;
  };
  toast::ToastNotifier mToastNotifier{nullptr};
  std::mutex mToastMutex;
  std::map<std::string, ShownToast> mToasts; // by notification key
  Activation mToastActivation;
};

} // namespace

std::unique_ptr<DesktopNotifier> DesktopNotifier::create() { return std::make_unique<WindowsNotifier>(); }

} // namespace st::app

#else

namespace st::app {

std::unique_ptr<DesktopNotifier> DesktopNotifier::create() { return std::make_unique<RecordingNotifier>(); }

} // namespace st::app

#endif
