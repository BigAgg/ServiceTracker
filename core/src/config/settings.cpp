#include "st/config/settings.h"

#include "st/util/json.h"

namespace st::config {

using util::json::Json;

namespace {
void read(const Json &j, const char *key, std::string &target) { target = util::json::str(j, key, target); }
void read(const Json &j, const char *key, int &target) { target = static_cast<int>(util::json::integer(j, key, target)); }
} // namespace

util::Result<Settings> loadSettings(const util::fs::path &file) {
  Settings settings;
  if (!util::pathExists(file))
    return settings;
  auto j = util::json::load(file);
  if (!j)
    return std::unexpected(j.error());
  if (!j->is_object())
    return std::unexpected(util::Error{"Einstellungen müssen ein JSON-Objekt sein: " + util::pathToUtf8(file)});

  if (const auto it = j->find("paths"); it != j->end() && it->is_object()) {
    auto &p = settings.paths;
    read(*it, "usersFile", p.usersFile);
    read(*it, "requestsDir", p.requestsDir);
    read(*it, "devicesRoot", p.devicesRoot);
    read(*it, "configsDir", p.configsDir);
    read(*it, "syncDir", p.syncDir);
    read(*it, "externLocalDir", p.externLocalDir);
    read(*it, "externFile", p.externFile);
    read(*it, "ordersFile", p.ordersFile);
    read(*it, "crashDir", p.crashDir);
    read(*it, "translationsFile", p.translationsFile);
  }
  if (const auto it = j->find("translator"); it != j->end() && it->is_object()) {
    auto &t = settings.translator;
    read(*it, "url", t.url);
    read(*it, "apiKey", t.apiKey);
    read(*it, "source", t.source);
    read(*it, "target", t.target);
  }
  read(*j, "translatorTimeoutMs", settings.translatorTimeoutMs);
  read(*j, "pdfBrowser", settings.pdfBrowser);
  read(*j, "updateUrl", settings.updateUrl);
  read(*j, "updateInstaller", settings.updateInstaller);
  read(*j, "bootstrapAdmin", settings.bootstrapAdmin);
  read(*j, "refreshIntervalMs", settings.refreshIntervalMs);
  read(*j, "autosaveDelayMs", settings.autosaveDelayMs);
  read(*j, "recordTitle", settings.recordTitle);
  read(*j, "recordFooter", settings.recordFooter);
  if (j->contains("knownCustomers"))
    settings.knownCustomers = util::json::strings(*j, "knownCustomers");
  return settings;
}

util::Status saveSettings(const util::fs::path &file, const Settings &settings) {
  const auto &p = settings.paths;
  Json j;
  j["paths"] = {
      {"usersFile", p.usersFile},
      {"requestsDir", p.requestsDir},
      {"devicesRoot", p.devicesRoot},
      {"configsDir", p.configsDir},
      {"syncDir", p.syncDir},
      {"externLocalDir", p.externLocalDir},
      {"externFile", p.externFile},
      {"ordersFile", p.ordersFile},
      {"crashDir", p.crashDir},
      {"translationsFile", p.translationsFile},
  };
  j["updateUrl"] = settings.updateUrl;
  j["updateInstaller"] = settings.updateInstaller;
  j["bootstrapAdmin"] = settings.bootstrapAdmin;
  j["refreshIntervalMs"] = settings.refreshIntervalMs;
  j["autosaveDelayMs"] = settings.autosaveDelayMs;
  j["knownCustomers"] = settings.knownCustomers;
  j["recordTitle"] = settings.recordTitle;
  j["recordFooter"] = settings.recordFooter;
  const auto &t = settings.translator;
  j["translator"] = {{"url", t.url}, {"apiKey", t.apiKey}, {"source", t.source}, {"target", t.target}};
  j["translatorTimeoutMs"] = settings.translatorTimeoutMs;
  j["pdfBrowser"] = settings.pdfBrowser;
  return util::json::save(file, j);
}

} // namespace st::config
