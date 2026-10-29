#include "st/translate/libretranslate.h"

#include "st/util/json.h"

namespace st::translate {

using util::json::Json;

std::string translateEndpoint(std::string_view baseUrl) {
  std::string url(baseUrl);
  while (!url.empty() && url.back() == '/')
    url.pop_back();
  return url + "/translate";
}

std::string buildTranslateRequest(const std::vector<std::string> &texts, const LibreTranslateConfig &config) {
  Json j;
  j["q"] = texts;
  j["source"] = config.source;
  j["target"] = config.target;
  j["format"] = "text";
  if (!config.apiKey.empty())
    j["api_key"] = config.apiKey;
  return j.dump(-1, ' ', false, Json::error_handler_t::replace);
}

util::Result<std::vector<std::string>> parseTranslateResponse(const HttpResponse &response, std::size_t expected) {
  auto j = util::json::parse(response.body);
  if (response.status < 200 || response.status >= 300) {
    std::string message = "Übersetzungsdienst meldet HTTP " + std::to_string(response.status);
    if (j && j->is_object())
      if (const auto error = util::json::str(*j, "error"); !error.empty())
        message += ": " + error;
    return std::unexpected(util::Error{message});
  }
  if (!j || !j->is_object())
    return std::unexpected(util::Error{"Ungültige Antwort des Übersetzungsdienstes."});
  const auto it = j->find("translatedText");
  if (it == j->end())
    return std::unexpected(util::Error{"Antwort des Übersetzungsdienstes enthält keine Übersetzung."});
  std::vector<std::string> out;
  if (it->is_string()) {
    out.push_back(it->get<std::string>());
  } else if (it->is_array()) {
    for (const auto &value : *it)
      out.push_back(value.is_string() ? value.get<std::string>() : std::string());
  }
  if (out.size() != expected)
    return std::unexpected(util::Error{"Übersetzungsdienst lieferte " + std::to_string(out.size()) + " statt " +
                                       std::to_string(expected) + " Übersetzungen."});
  return out;
}

util::Result<std::vector<std::string>> LibreTranslateBackend::translate(const std::vector<std::string> &texts) {
  if (texts.empty())
    return std::vector<std::string>{};
  if (!mPost)
    return std::unexpected(util::Error{"Kein HTTP-Zugang für den Übersetzungsdienst."});
  const auto endpoint = translateEndpoint(mConfig.url);
  auto response = mPost(endpoint, buildTranslateRequest(texts, mConfig));
  if (!response)
    return std::unexpected(util::Error{"Übersetzungsdienst " + mConfig.url + " nicht erreichbar: " + response.error().message});
  return parseTranslateResponse(*response, texts.size());
}

} // namespace st::translate
