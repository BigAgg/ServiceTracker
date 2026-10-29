#include "framework/http.h"

#if defined(_WIN32) && !defined(ST_APP_COMPILE_CHECK)
#include <windows.h>
#include <winhttp.h>

#include <iterator>
#include <memory>

// WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY (Windows 8.1+) honours the system proxy incl. bypass list
#ifdef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define ST_WINHTTP_ACCESS WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#else
#define ST_WINHTTP_ACCESS WINHTTP_ACCESS_TYPE_DEFAULT_PROXY
#endif
#endif

namespace st::app::http {

#if defined(_WIN32) && !defined(ST_APP_COMPILE_CHECK)

namespace {

struct HandleCloser {
  void operator()(HINTERNET h) const {
    if (h)
      WinHttpCloseHandle(h);
  }
};
using Handle = std::unique_ptr<void, HandleCloser>;

std::wstring widen(const std::string &utf8) {
  if (utf8.empty())
    return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  std::wstring out(static_cast<std::size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
  return out;
}

util::Error lastError(const char *what) {
  return util::Error{std::string(what) + " (Fehler " + std::to_string(GetLastError()) + ")"};
}

} // namespace

util::Result<translate::HttpResponse> postJson(const std::string &url, const std::string &jsonBody, int timeoutMs) {
  const std::wstring wurl = widen(url);
  URL_COMPONENTS parts{};
  parts.dwStructSize = sizeof(parts);
  wchar_t host[256] = {};
  wchar_t path[2048] = {};
  parts.lpszHostName = host;
  parts.dwHostNameLength = static_cast<DWORD>(std::size(host));
  parts.lpszUrlPath = path;
  parts.dwUrlPathLength = static_cast<DWORD>(std::size(path));
  if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts))
    return std::unexpected(util::Error{"Ungültige Adresse: " + url});

  Handle session(WinHttpOpen(L"ServiceTracker", ST_WINHTTP_ACCESS, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
  if (!session)
    return std::unexpected(lastError("HTTP nicht verfügbar"));
  WinHttpSetTimeouts(session.get(), timeoutMs, timeoutMs, timeoutMs, timeoutMs);
  Handle connection(WinHttpConnect(session.get(), host, parts.nPort, 0));
  if (!connection)
    return std::unexpected(lastError("Verbindung fehlgeschlagen"));
  const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
  Handle request(WinHttpOpenRequest(connection.get(), L"POST", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
  if (!request)
    return std::unexpected(lastError("Anfrage fehlgeschlagen"));
  const wchar_t *headers = L"Content-Type: application/json\r\nAccept: application/json\r\n";
  if (!WinHttpSendRequest(request.get(), headers, static_cast<DWORD>(-1L), const_cast<char *>(jsonBody.data()),
                          static_cast<DWORD>(jsonBody.size()), static_cast<DWORD>(jsonBody.size()), 0) ||
      !WinHttpReceiveResponse(request.get(), nullptr))
    return std::unexpected(lastError("Keine Antwort"));

  translate::HttpResponse response;
  DWORD status = 0;
  DWORD size = sizeof(status);
  WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
                      WINHTTP_NO_HEADER_INDEX);
  response.status = static_cast<int>(status);
  while (true) {
    DWORD available = 0;
    if (!WinHttpQueryDataAvailable(request.get(), &available))
      return std::unexpected(lastError("Antwort unvollständig"));
    if (available == 0)
      break;
    std::string chunk(available, '\0');
    DWORD read = 0;
    if (!WinHttpReadData(request.get(), chunk.data(), available, &read))
      return std::unexpected(lastError("Antwort unvollständig"));
    response.body.append(chunk.data(), read);
  }
  return response;
}

#else

util::Result<translate::HttpResponse> postJson(const std::string &url, const std::string &, int) {
  return std::unexpected(util::Error{"HTTP ist nur unter Windows verfügbar (" + url + ")"});
}

#endif

} // namespace st::app::http
