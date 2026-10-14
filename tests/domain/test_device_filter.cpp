#include <doctest/doctest.h>

#include "st/service/device_filter.h"
#include "test_helpers.h"

using namespace st;
using namespace st::service;

namespace {
devices::DeviceRegistry registry() {
  devices::DeviceRegistry r;
  for (const char *name : {"0601-1", "0601-2", "0601-3", "0471-1", "0471-2"})
    r.add(devices::Device{name, "", 0, {}});
  return r;
}

std::vector<ServiceRecord> records() {
  std::vector<ServiceRecord> out(5);
  out[0].name = "0601-1";
  out[0].status = "Technik";
  out[1].name = "0601-2";
  out[1].status = "erwartet";
  out[2].name = "0471-1";
  out[2].status = "erledigt";
  out[3].name = "0471-2";
  out[3].status = "Technik";
  out[4].name = "0999-9";
  out[4].status = "Versand";
  out[4].path = "Y:/x/0999-9";
  return out;
}
} // namespace

TEST_CASE("status index") {
  const auto index = StatusIndex::fromRecords(records());
  CHECK(index.statusOf("0601-1") == "Technik");
  CHECK(index.statusOf("0601-3").empty());
  CHECK(index.statuses() == std::vector<std::string>{"Technik", "Versand", "erledigt", "erwartet"});
  CHECK(index.count("Technik") == 2);
}

TEST_CASE("device filter like Reader Control") {
  auto reg = registry();
  const auto index = StatusIndex::fromRecords(records());

  DeviceFilter f;
  CHECK(filterDevices(reg, index, f) == std::vector<std::string>{"0471-2", "0601-1", "0999-9"});
  f.showClosed = true;
  CHECK(filterDevices(reg, index, f).size() == 5);

  f = {};
  f.text = "0601";
  CHECK(filterDevices(reg, index, f) == std::vector<std::string>{"0601-1", "0601-2", "0601-3"});
  f = {};
  f.project = "0471";
  CHECK(filterDevices(reg, index, f) == std::vector<std::string>{"0471-1", "0471-2"});
  f = {};
  f.status = "Technik";
  CHECK(filterDevices(reg, index, f) == std::vector<std::string>{"0471-2", "0601-1"});
  f.project = "0601";
  CHECK(filterDevices(reg, index, f) == std::vector<std::string>{"0601-1"});
  f.text = "nichts";
  CHECK(filterDevices(reg, index, f).empty());
}

TEST_CASE("devices of the service list are added when the folder is not accessible") {
  auto reg = registry();
  CHECK(addMissingDevices(reg, records()) == 1);
  REQUIRE(reg.find("0999-9"));
  CHECK(reg.find("0999-9")->path == st::util::pathFromUtf8("Y:/x/0999-9"));
  CHECK(addMissingDevices(reg, records()) == 0);
}
