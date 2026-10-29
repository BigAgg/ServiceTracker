#pragma once

#include <st/translate/libretranslate.h>

#include <string>

/// Minimal HTTP client (WinHTTP). Thread safe; meant for worker threads.
namespace st::app::http {

/// POSTs `jsonBody` with Content-Type application/json. HTTP error codes are returned
/// as response, only transport errors (no connection, timeout) as error.
util::Result<translate::HttpResponse> postJson(const std::string &url, const std::string &jsonBody, int timeoutMs);

} // namespace st::app::http
