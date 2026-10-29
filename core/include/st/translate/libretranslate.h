#pragma once

#include "st/translate/translator.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

/// Client for a LibreTranslate server (https://libretranslate.com, self hosted).
/// The HTTP transport is injected so the protocol is testable without a server; the
/// application passes a WinHTTP based implementation.
namespace st::translate {

struct LibreTranslateConfig {
  /// Base URL of the server, e.g. "http://localhost:5000".
  std::string url = "http://localhost:5000";
  /// Only needed if the server was started with API keys.
  std::string apiKey;
  std::string source = "de";
  std::string target = "en";

  bool operator==(const LibreTranslateConfig &) const = default;
};

struct HttpResponse {
  int status = 0;
  std::string body;
};

/// POSTs a JSON body to the URL. Returns an error only for transport problems
/// (server not reachable, timeout); HTTP error codes are returned as response.
using HttpPost = std::function<util::Result<HttpResponse>(const std::string &url, const std::string &jsonBody)>;

/// "<url>/translate" (tolerates a trailing slash in the configured URL).
std::string translateEndpoint(std::string_view baseUrl);

/// Request body for POST /translate with several texts at once.
std::string buildTranslateRequest(const std::vector<std::string> &texts, const LibreTranslateConfig &config);

/// Translations from a /translate response. HTTP errors and the server's "error"
/// message become a German error text.
util::Result<std::vector<std::string>> parseTranslateResponse(const HttpResponse &response, std::size_t expected);

class LibreTranslateBackend final : public Backend {
public:
  LibreTranslateBackend(LibreTranslateConfig config, HttpPost post) : mConfig(std::move(config)), mPost(std::move(post)) {}

  util::Result<std::vector<std::string>> translate(const std::vector<std::string> &texts) override;

private:
  LibreTranslateConfig mConfig;
  HttpPost mPost;
};

} // namespace st::translate
