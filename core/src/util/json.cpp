#include "st/util/json.h"

#include "st/util/strings.h"

namespace st::util::json {

std::string str(const Json &j, const char *key, std::string fallback) {
  if (!j.is_object())
    return fallback;
  const auto it = j.find(key);
  if (it == j.end())
    return fallback;
  if (it->is_string())
    return it->get<std::string>();
  if (it->is_number_integer())
    return std::to_string(it->get<long long>());
  return fallback;
}

bool boolean(const Json &j, const char *key, bool fallback) {
  if (!j.is_object())
    return fallback;
  const auto it = j.find(key);
  if (it == j.end())
    return fallback;
  if (it->is_boolean())
    return it->get<bool>();
  if (it->is_number())
    return it->get<double>() != 0.0;
  return fallback;
}

double number(const Json &j, const char *key, double fallback) {
  if (!j.is_object())
    return fallback;
  const auto it = j.find(key);
  if (it == j.end())
    return fallback;
  if (it->is_number())
    return it->get<double>();
  if (it->is_string())
    return parseDouble(it->get<std::string>()).value_or(fallback);
  return fallback;
}

long long integer(const Json &j, const char *key, long long fallback) {
  if (!j.is_object())
    return fallback;
  const auto it = j.find(key);
  if (it == j.end())
    return fallback;
  if (it->is_number_integer())
    return it->get<long long>();
  if (it->is_number())
    return static_cast<long long>(it->get<double>());
  if (it->is_string())
    return parseInt64(it->get<std::string>()).value_or(fallback);
  return fallback;
}

unsigned long long uinteger(const Json &j, const char *key, unsigned long long fallback) {
  if (!j.is_object())
    return fallback;
  const auto it = j.find(key);
  if (it == j.end())
    return fallback;
  if (it->is_number_unsigned())
    return it->get<unsigned long long>();
  if (it->is_number_integer())
    return static_cast<unsigned long long>(it->get<long long>());
  if (it->is_string())
    return parseUInt64(it->get<std::string>()).value_or(fallback);
  return fallback;
}

std::vector<std::string> strings(const Json &j, const char *key) {
  std::vector<std::string> out;
  for (const auto &e : array(j, key))
    if (e.is_string())
      out.push_back(e.get<std::string>());
  return out;
}

const Json &array(const Json &j, const char *key) {
  static const Json empty = Json::array();
  if (!j.is_object())
    return empty;
  const auto it = j.find(key);
  if (it == j.end() || !it->is_array())
    return empty;
  return *it;
}

std::string dump(const Json &j) {
  return j.dump(4, ' ', false, Json::error_handler_t::replace);
}

Result<Json> parse(std::string_view text) {
  Json j = Json::parse(text, nullptr, false);
  if (j.is_discarded())
    return std::unexpected(Error{"Ungültiges JSON"});
  return j;
}

Result<Json> load(const fs::path &path) {
  auto content = readFile(path);
  if (!content)
    return std::unexpected(content.error());
  auto j = parse(*content);
  if (!j)
    return std::unexpected(Error{"Ungültiges JSON in \"" + pathToUtf8(path) + "\""});
  return j;
}

Status save(const fs::path &path, const Json &j) {
  return writeFileAtomic(path, dump(j));
}

} // namespace st::util::json
