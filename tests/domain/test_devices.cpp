#include <doctest/doctest.h>

#include "st/devices/device_registry.h"
#include "test_helpers.h"

#include <thread>

using namespace st;
using namespace st::devices;

TEST_CASE("device names") {
  CHECK(isValidDeviceName("0601-1234"));
  CHECK(isValidDeviceName("0471-A1"));
  CHECK_FALSE(isValidDeviceName("601-1234"));
  CHECK_FALSE(isValidDeviceName("06011-1234"));
  CHECK_FALSE(isValidDeviceName("0601"));
  CHECK_FALSE(isValidDeviceName("0601-"));
  CHECK(projectOf("0601-1234") == "0601");
  CHECK(serialOf("0601-1234") == "1234");
  CHECK(projectOfFolder("0601 FLUOstar Omega") == "0601");
  CHECK(projectOfFolder("Archiv").empty());
}

TEST_CASE("flags") {
  CHECK(flagLabel(static_cast<ConfigFlags>(ConfigFlag::EXT_INC)) == "65°");
  CHECK(flagLabel(0) == "None");
  CHECK(flagFromKey("TRF_LASER") == ConfigFlag::TRF_LASER);
  CHECK_FALSE(flagFromKey("nope").has_value());
  CHECK(hasFlag(0x41, ConfigFlag::FP));
  CHECK_FALSE(hasFlag(0x41, ConfigFlag::ABS));
}

TEST_CASE("device info parsing") {
  const auto info = parseDeviceInfo(
      "Reader active time: 1.234:56 h\r\n"
      "Executed testruns: 12.345\n"
      "Processed wells: 99\n"
      "Performed flashes (flash lamp): 1.000.000\n"
      "Pump 2 strokes: kaputt\n"
      "Unknown: 5\n");
  CHECK(info.readerActiveTime == "1234:56 h");
  CHECK(info.executedTestruns == 12345);
  CHECK(info.processedWells == 99);
  CHECK(info.performedFlashesLamp == 1000000);
  CHECK(info.pump2Strokes == 0);
}

TEST_CASE("eeprom flags") {
  DeviceConfig config;
  config.project = "0601";
  config.addresses = {AddressRule{100, static_cast<ConfigFlags>(ConfigFlag::LUM), 1, {}},
                      AddressRule{200, static_cast<ConfigFlags>(ConfigFlag::TRF), 3, {}}};
  const std::string eeprom =
      "E 100 1 ;Luminescence\n"
      "E 200 2\"comment ;TRF wrong value\n"
      "E 300 1 ;not configured\n"
      "garbage line\n";
  CHECK(parseEepromFlags(eeprom, config) == static_cast<ConfigFlags>(ConfigFlag::LUM));
  CHECK(parseEepromFlags("E 200 3\"x;y", config) == static_cast<ConfigFlags>(ConfigFlag::TRF));
}

TEST_CASE("base flags: legacy defaults and configuration") {
  DeviceConfig c;
  c.project = "0601";
  CHECK(c.effectiveBaseFlags() == static_cast<ConfigFlags>(ConfigFlag::ABS));
  c.project = "0472";
  CHECK(c.effectiveBaseFlags() == static_cast<ConfigFlags>(ConfigFlag::FP));
  c.baseFlags = static_cast<ConfigFlags>(ConfigFlag::LTS);
  CHECK(c.effectiveBaseFlags() == static_cast<ConfigFlags>(ConfigFlag::LTS));
  CHECK(DeviceConfig::expandName("{reader}_Checklist_{service}.json", "0601-1", "2026.01.02") == "0601-1_Checklist_2026.01.02.json");
}

TEST_CASE("config store keeps unknown keys and the legacy layout") {
  test::TempDir dir;
  const auto file = dir / "configs.json";
  test::writeText(file, R"([
    {"project": "0601", "folders": ["Bilder"], "custom": 42,
     "files": [{"path": "Y:/tpl/Checklist.json", "rename": "{reader}_Checklist.json"}, {"path": "", "rename": "x"}],
     "addresses": [{"address": 100, "flagToSet": 4, "validationvalue": 1, "foldersToAdd": ["LUM"]}, {"address": 0}]},
    {"project": "", "folders": []},
    {"project": "0471", "baseFlags": ["fp", "abs"]}
  ])");
  DeviceConfigStore store(file);
  REQUIRE(store.load());
  REQUIRE(store.configs().size() == 2);
  const DeviceConfig *c = store.find("0601");
  REQUIRE(c);
  CHECK(c->files.size() == 1);
  CHECK(c->addresses.size() == 1);
  CHECK(c->addresses[0].folders == std::vector<std::string>{"LUM"});
  CHECK(store.find("0471")->effectiveBaseFlags() == (static_cast<ConfigFlags>(ConfigFlag::FP) | static_cast<ConfigFlags>(ConfigFlag::ABS)));
  CHECK(store.projects() == std::vector<std::string>{"0471", "0601"});

  DeviceConfig edited = *c;
  edited.folders.push_back("Messungen");
  REQUIRE(store.upsert(edited));
  const auto saved = util::json::load(file);
  REQUIRE(saved.has_value());
  CHECK((*saved)[0]["custom"] == 42);
  CHECK((*saved)[0]["folders"].size() == 2);
  CHECK((*saved)[0]["addresses"][0]["validationvalue"] == 1);
  CHECK_FALSE((*saved)[0].contains("baseFlags"));
  CHECK((*saved)[1]["baseFlags"].size() == 2);

  // Extern templates: optional key, falls back to the normal templates
  CHECK_FALSE((*saved)[0].contains("filesExtern"));
  CHECK(&c->templatesFor(true) == &c->files);
  edited.filesExtern = {TemplateFile{"Y:/tpl/Checklist_extern.json", "{reader}_Checklist_extern.json"}};
  REQUIRE(store.upsert(edited));
  DeviceConfigStore withExtern(file);
  REQUIRE(withExtern.load());
  const DeviceConfig *e = withExtern.find("0601");
  REQUIRE(e);
  CHECK(e->templatesFor(true) == edited.filesExtern);
  CHECK(e->templatesFor(false) == e->files);
  edited.filesExtern.clear();
  REQUIRE(store.upsert(edited));
  CHECK_FALSE((*util::json::load(file))[0].contains("filesExtern"));

  REQUIRE(store.remove("0471"));
  DeviceConfigStore again(file);
  REQUIRE(again.load());
  CHECK(again.projects() == std::vector<std::string>{"0601"});
  CHECK_FALSE(store.upsert(DeviceConfig{}).has_value());
}

TEST_CASE("registry scans the device folder structure") {
  test::TempDir dir;
  REQUIRE(util::createDirectories(dir / "0601 FLUOstar/0601-100"));
  REQUIRE(util::createDirectories(dir / "0601 FLUOstar/0601-200"));
  REQUIRE(util::createDirectories(dir / "0601 FLUOstar/Archiv"));
  REQUIRE(util::createDirectories(dir / "0471 PHERAstar/0471-1"));
  REQUIRE(util::createDirectories(dir / "Sonstiges/0999-1"));
  test::writeText(dir / "0601 FLUOstar/0601-300", "a file, not a device");

  DeviceRegistry registry;
  REQUIRE(registry.scan(dir.path()));
  CHECK(registry.hasAccess());
  CHECK(registry.size() == 3);
  CHECK(registry.projects() == std::vector<std::string>{"0471", "0601"});
  CHECK(registry.devicesOf("0601") == std::vector<std::string>{"0601-100", "0601-200"});
  CHECK(registry.search("200") == std::vector<std::string>{"0601-200"});
  CHECK(registry.search("", "0471") == std::vector<std::string>{"0471-1"});
  REQUIRE(registry.find("0601-100"));
  CHECK(registry.find("0601-100")->path == dir / "0601 FLUOstar/0601-100");

  CHECK_FALSE(registry.add(*registry.find("0601-100")));
  CHECK(registry.add(Device{"0601-999", "", 0, {}}));

  DeviceRegistry noAccess;
  CHECK_FALSE(noAccess.scan(dir / "missing"));
  CHECK_FALSE(noAccess.hasAccess());
}

TEST_CASE("populateDevice reads the newest eeprom and info files") {
  test::TempDir dir;
  Device device{"0601-1", dir.path(), 0, {}};
  test::writeText(dir / "Service_2025.01.01/EEPROM_old.btc", "E 100 0;\n");
  test::writeText(dir / "Service_2025.01.01/Reader_Info.txt", "Processed wells: 1\n");
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  test::writeText(dir / "Service_2026.01.01/sub/eeprom_new.BTC", "E 100 1;\n");
  test::writeText(dir / "Service_2026.01.01/info_2026.txt", "Processed wells: 2\n");

  DeviceConfig config;
  config.project = "0601";
  config.addresses = {AddressRule{100, static_cast<ConfigFlags>(ConfigFlag::LUM), 1, {}}};
  populateDevice(device, &config);
  CHECK(hasFlag(device.flags, ConfigFlag::ABS)); // legacy base flag of 0601
  CHECK(hasFlag(device.flags, ConfigFlag::LUM));
  CHECK(device.info.processedWells == 2);
  CHECK_FALSE(device.info.fileDate.empty());

  device.flags = 0x100;
  populateDevice(device, nullptr); // without config flags stay untouched
  CHECK(device.flags == 0x100);
}
