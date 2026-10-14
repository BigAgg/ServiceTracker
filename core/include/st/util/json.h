#pragma once

#include "st/util/fs.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

/// Tolerant accessors for JSON files written by other (older) program versions.
/// Missing keys or wrong types never throw; the fallback value is returned instead.
namespace st::util::json {

using Json = nlohmann::json;

std::string str(const Json &j, const char *key, std::string fallback = {});
bool boolean(const Json &j, const char *key, bool fallback = false);
double number(const Json &j, const char *key, double fallback = 0.0);
long long integer(const Json &j, const char *key, long long fallback = 0);
unsigned long long uinteger(const Json &j, const char *key, unsigned long long fallback = 0);
std::vector<std::string> strings(const Json &j, const char *key);
/// Returns the array stored under `key` or an empty array.
const Json &array(const Json &j, const char *key);

/// Serialises with 4 spaces indentation like the legacy program. Invalid UTF-8
/// is replaced instead of throwing.
std::string dump(const Json &j);

/// Parses text, returns an error instead of throwing.
Result<Json> parse(std::string_view text);

/// Reads and parses a JSON file (with retries for locked files).
Result<Json> load(const fs::path &path);

/// Atomically writes a JSON file.
Status save(const fs::path &path, const Json &j);

} // namespace st::util::json
