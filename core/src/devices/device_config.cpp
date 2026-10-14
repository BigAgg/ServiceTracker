#include "st/devices/device_config.h"

#include "st/util/log.h"
#include "st/util/strings.h"

#include <algorithm>

namespace st::devices {

using util::json::Json;

ConfigFlags DeviceConfig::effectiveBaseFlags() const {
  if (baseFlags)
    return *baseFlags;
  if (project == "0601")
    return static_cast<ConfigFlags>(ConfigFlag::ABS);
  if (project == "0471" || project == "0472")
    return static_cast<ConfigFlags>(ConfigFlag::FP);
  return 0;
}

std::string DeviceConfig::expandName(std::string_view pattern, std::string_view deviceName, std::string_view serviceDate) {
  std::string out(pattern);
  util::replaceAll(out, "{reader}", deviceName);
  util::replaceAll(out, "{service}", serviceDate);
  return out;
}

ConfigFlags parseEepromFlags(std::string_view content, const DeviceConfig &config) {
  ConfigFlags flags = 0;
  for (const auto &line : util::splitLines(content)) {
    const std::string values = util::splitAt(line, ";").first;
    auto [address, value] = util::splitAt(util::splitAt(values, " ").second, " ");
    if (value.find('"') != std::string::npos)
      value = util::splitAt(value, "\"").first;
    std::erase(address, ' ');
    std::erase(value, ' ');
    const auto adr = util::parseInt(address);
    const auto val = util::parseInt(value);
    if (!adr || !val)
      continue;
    for (const auto &rule : config.addresses)
      if (rule.address == *adr && rule.validationValue == *val)
        flags |= rule.flag;
  }
  return flags;
}

DeviceConfig DeviceConfigStore::fromJson(const Json &j) {
  DeviceConfig config;
  config.project = util::json::str(j, "project");
  config.folders = util::json::strings(j, "folders");
  for (const auto &file : util::json::array(j, "files")) {
    TemplateFile tf{util::json::str(file, "path"), util::json::str(file, "rename")};
    if (tf.valid())
      config.files.push_back(std::move(tf));
  }
  for (const auto &a : util::json::array(j, "addresses")) {
    AddressRule rule;
    rule.address = static_cast<int>(util::json::integer(a, "address", 0));
    rule.flag = static_cast<ConfigFlags>(util::json::integer(a, "flagToSet", 0));
    rule.validationValue = static_cast<int>(util::json::integer(a, "validationvalue", -1));
    rule.folders = util::json::strings(a, "foldersToAdd");
    if (rule.valid())
      config.addresses.push_back(std::move(rule));
  }
  if (j.contains("baseFlags")) {
    ConfigFlags base = 0;
    for (const auto &key : util::json::strings(j, "baseFlags"))
      if (auto flag = flagFromKey(key))
        base |= static_cast<ConfigFlags>(*flag);
    config.baseFlags = base;
  }
  return config;
}

Json DeviceConfigStore::toJson(const DeviceConfig &config, Json base) {
  Json j = base.is_object() ? std::move(base) : Json::object();
  j["project"] = config.project;
  j["folders"] = config.folders;
  j["files"] = Json::array();
  for (const auto &file : config.files)
    if (file.valid())
      j["files"].push_back({{"path", file.path}, {"rename", file.rename}});
  j["addresses"] = Json::array();
  for (const auto &rule : config.addresses) {
    if (!rule.valid())
      continue;
    j["addresses"].push_back({{"address", rule.address},
                              {"flagToSet", rule.flag},
                              {"validationvalue", rule.validationValue},
                              {"foldersToAdd", rule.folders}});
  }
  if (config.baseFlags) {
    Json keys = Json::array();
    for (const auto &info : kFlags)
      if (hasFlag(*config.baseFlags, info.flag))
        keys.push_back(info.key);
    j["baseFlags"] = keys;
  } else {
    j.erase("baseFlags");
  }
  return j;
}

util::Status DeviceConfigStore::load() {
  mConfigs.clear();
  mRaw = Json::array();
  auto j = util::json::load(mFile);
  if (!j)
    return std::unexpected(j.error());
  if (!j->is_array())
    return std::unexpected(util::Error{"configs.json hat ein falsches Format"});
  mRaw = *j;
  for (const auto &entry : mRaw) {
    DeviceConfig config = fromJson(entry);
    if (config.valid())
      mConfigs.push_back(std::move(config));
  }
  log::info("{} Gerätekonfigurationen geladen", mConfigs.size());
  return {};
}

util::Status DeviceConfigStore::save() const {
  Json out = Json::array();
  for (const auto &config : mConfigs) {
    Json base;
    for (const auto &raw : mRaw)
      if (util::json::str(raw, "project") == config.project)
        base = raw;
    out.push_back(toJson(config, base));
  }
  return util::json::save(mFile, out);
}

const DeviceConfig *DeviceConfigStore::find(std::string_view project) const {
  auto it = std::find_if(mConfigs.begin(), mConfigs.end(), [&](const DeviceConfig &c) { return c.project == project; });
  return it == mConfigs.end() ? nullptr : &*it;
}

std::vector<std::string> DeviceConfigStore::projects() const {
  std::vector<std::string> out;
  for (const auto &c : mConfigs)
    out.push_back(c.project);
  std::sort(out.begin(), out.end());
  return out;
}

util::Status DeviceConfigStore::upsert(DeviceConfig config) {
  if (!config.valid())
    return std::unexpected(util::Error{"Projekt darf nicht leer sein"});
  auto it = std::find_if(mConfigs.begin(), mConfigs.end(), [&](const DeviceConfig &c) { return c.project == config.project; });
  if (it != mConfigs.end())
    *it = std::move(config);
  else
    mConfigs.push_back(std::move(config));
  return save();
}

util::Status DeviceConfigStore::remove(std::string_view project) {
  std::erase_if(mConfigs, [&](const DeviceConfig &c) { return c.project == project; });
  return save();
}

} // namespace st::devices
