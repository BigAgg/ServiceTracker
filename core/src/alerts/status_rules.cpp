#include "st/alerts/status_rules.h"

#include "st/util/json.h"
#include "st/util/strings.h"
#include "st/util/suggestions.h"

#include <algorithm>

namespace st::alerts {

using util::json::Json;

std::vector<StatusRule> defaultStatusRules() {
  return {
      {"versendet", {"TECHSUPPORT"}},
      {"extern abgeschlossen", {"TECHSUPPORT"}},
      {"im Versand", {"SALES"}},
      {"Rechnung erstellen", {"SALES"}},
      {"Rechnung versendet", {"TECHSUPPORT"}},
  };
}

std::string statusChannelId(std::string_view status) { return "status." + util::foldCase(status); }

std::vector<StatusRule> normalizeStatusRules(std::vector<StatusRule> rules) {
  std::vector<StatusRule> out;
  for (auto &rule : rules) {
    rule.status = util::trim(rule.status);
    std::erase_if(rule.groups, [](const std::string &g) { return util::trim(g).empty(); });
    if (rule.status.empty() || rule.groups.empty())
      continue;
    const std::string key = util::foldCase(rule.status);
    auto existing = std::find_if(out.begin(), out.end(), [&](const StatusRule &r) { return util::foldCase(r.status) == key; });
    if (existing == out.end()) {
      out.push_back(std::move(rule));
      existing = std::prev(out.end());
    } else {
      existing->groups.insert(existing->groups.end(), rule.groups.begin(), rule.groups.end());
    }
    std::sort(existing->groups.begin(), existing->groups.end());
    existing->groups.erase(std::unique(existing->groups.begin(), existing->groups.end()), existing->groups.end());
  }
  return out;
}

util::Status StatusRuleStore::load() {
  mLastWrite = util::lastWriteTime(mFile);
  if (!util::pathExists(mFile)) {
    mRules = defaultStatusRules();
    return {};
  }
  auto j = util::json::load(mFile);
  if (!j)
    return std::unexpected(j.error());
  if (!j->is_array())
    return std::unexpected(util::Error{"Falsches Format (Liste erwartet): " + util::pathToUtf8(mFile)});
  std::vector<StatusRule> rules;
  for (const auto &entry : *j)
    rules.push_back({util::json::str(entry, "status"), util::json::strings(entry, "groups")});
  mRules = normalizeStatusRules(std::move(rules));
  return {};
}

util::Status StatusRuleStore::save(std::vector<StatusRule> rules) {
  rules = normalizeStatusRules(std::move(rules));
  Json j = Json::array();
  for (const auto &rule : rules)
    j.push_back({{"status", rule.status}, {"groups", rule.groups}});
  if (auto s = util::json::save(mFile, j); !s)
    return s;
  mRules = std::move(rules);
  mLastWrite = util::lastWriteTime(mFile);
  return {};
}

} // namespace st::alerts
