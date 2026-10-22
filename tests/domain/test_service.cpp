#include <doctest/doctest.h>

#include "st/checklist/actions.h"
#include "st/checklist/document.h"
#include "st/checklist/formats.h"
#include "st/service/service_manager.h"
#include "test_helpers.h"

using namespace st;
using namespace st::service;

namespace {

/// Complete test environment: device folder, sync drive, configs and a template checklist.
struct Fixture {
  test::TempDir dir;
  test::TestSession ts;
  devices::DeviceConfigStore configs{dir / "configs/configs.json"};
  devices::Device device;
  std::unique_ptr<ServiceManager> manager;

  Fixture() {
    REQUIRE(util::createDirectories(dir / "devices/0601 FLUOstar/0601-1234"));
    test::writeText(dir / "templates/Checklist.json", checklist::writeJson(templateChecklist()));
    test::writeText(dir / "configs/configs.json", "[]");
    REQUIRE(configs.load());
    devices::DeviceConfig config;
    config.project = "0601";
    config.folders = {"Bilder"};
    config.addresses = {devices::AddressRule{100, static_cast<devices::ConfigFlags>(devices::ConfigFlag::LUM), 1, {"LUM-Messung"}}};
    config.files = {devices::TemplateFile{util::pathToUtf8(dir / "templates/Checklist.json"), "{reader}_Checklist_{service}.json"}};
    REQUIRE(configs.upsert(config));

    device.name = "0601-1234";
    device.path = dir / "devices/0601 FLUOstar/0601-1234";
    device.flags = static_cast<devices::ConfigFlags>(devices::ConfigFlag::LUM);

    ServiceEnvironment env;
    env.syncTempDir = dir / "sync/temp";
    env.externLocalDir = dir / "local/extern";
    env.syncStore = RecordStore(dir / "sync/Sync.json");
    env.externStore = RecordStore(dir / "sync/extern.json");
    env.localExternStore = RecordStore(dir / "local/extern/extern_local.json");
    env.history = HistoryStore(dir / "sync/History.json");
    env.externHistory = HistoryStore(dir / "sync/History_extern.json");
    env.configs = &configs;
    REQUIRE(env.syncStore.ensureExists());
    REQUIRE(env.history.ensureExists());
    REQUIRE(env.externHistory.ensureExists());
    manager = std::make_unique<ServiceManager>(env);
  }

  static checklist::Checklist templateChecklist() {
    checklist::Checklist c;
    c.version = "1.0";
    checklist::Section s;
    s.name = "Preisliste";
    s.finished = true;
    checklist::TaskTable t;
    t.name = "Preisliste (Rabatt bereits berechnet)";
    t.templateRow.tasks = {checklist::Task::make("Preis", 0.0)};
    t.rows = {t.templateRow, t.templateRow};
    t.rows[0].tasks[0].value = 100.0;
    t.rows[1].tasks[0].value = 50.5;
    s.items.emplace_back(t);
    c.sections.push_back(s);
    return c;
  }
};

} // namespace

TEST_CASE("records: read-modify-write keeps other entries and unknown keys") {
  test::TempDir dir;
  RecordStore store(dir / "Sync.json");
  test::writeText(store.file(), R"([{"name":"0601-1","status":"erwartet","custom":true},{"name":"0601-2","status":"Technik"}])");
  ServiceRecord r = **store.find("0601-1");
  CHECK(r.status == "erwartet");
  CHECK(r.extra["custom"] == true);
  r.status = "Wareneingang";
  REQUIRE(store.upsert(r));
  auto all = store.loadAll();
  REQUIRE(all.has_value());
  REQUIRE(all->size() == 2);
  CHECK((*all)[0].status == "Wareneingang");
  CHECK((*all)[0].extra["custom"] == true);
  CHECK((*all)[1].status == "Technik");

  REQUIRE(store.remove("0601-2"));
  REQUIRE(store.remove("does-not-exist"));
  CHECK(store.loadAll()->size() == 1);
  CHECK_FALSE((*store.find("0601-2")).has_value());

  test::writeText(store.file(), "{}");
  CHECK_FALSE(store.loadAll().has_value());
}

TEST_CASE("status helpers") {
  CHECK(isClosedStatus(""));
  CHECK(isClosedStatus("erledigt"));
  CHECK(isClosedStatus("erledigt extern"));
  CHECK_FALSE(isClosedStatus("Wareneingang"));
  CHECK_FALSE(isClosedStatus("extern abgeschlossen")); // still has to be finished in house
  CHECK(isExternalWorkStatus("geplant extern"));
  CHECK(isExternalWorkStatus("extern"));
  CHECK_FALSE(isExternalWorkStatus("extern abgeschlossen"));
  CHECK(ServiceManager::serviceFolderName("2026.01.02") == "Service_2026.01.02");
  CHECK(ServiceManager::serviceDate("Service_2026.01.02") == "2026.01.02");
}

TEST_CASE("in house service: complete workflow") {
  Fixture f;
  auto &m = *f.manager;

  // expect the device
  auto expected = m.expect(f.device, "Kunde: Test");
  REQUIRE(expected.has_value());
  CHECK(expected->status == "erwartet");
  CHECK_FALSE(m.expect(f.device, "again").has_value()); // already in the list

  // create the service
  auto created = m.createService(f.device, f.ts.session);
  REQUIRE(created.has_value());
  CHECK(created->status == "Wareneingang");
  CHECK(created->activeService == "Service_2026.03.15");
  CHECK(created->additionalInfo == "Kunde: Test");
  CHECK(created->services == std::vector<std::string>{"Service_2026.03.15"});
  const auto serviceFolder = f.device.path / "Service_2026.03.15";
  CHECK(util::isDirectory(serviceFolder / "Bilder"));
  CHECK(util::isDirectory(serviceFolder / "LUM-Messung"));
  const auto syncFolder = m.syncFolderFor(f.device, "Service_2026.03.15");
  CHECK(util::pathExists(syncFolder / "0601-1234_Checklist_2026.03.15.json"));
  CHECK_FALSE(m.createService(f.device, f.ts.session).has_value()); // already running

  // checklist is found in the sync folder
  auto checklistPath = m.findChecklist(f.device, created->activeService, *created);
  REQUIRE(checklistPath.has_value());
  CHECK(checklistPath->parent_path() == syncFolder);

  // status changes while working
  REQUIRE(m.setStatus(f.device, "Technik").has_value());
  CHECK((*m.recordFor(f.device)).status == "Technik");

  // finish 3 days later
  f.ts.advance(3 * 24 * 3600);
  test::writeText(syncFolder / "notes.txt", "Arbeitsnotiz");
  const auto checklist = Fixture::templateChecklist();
  checklist::HtmlOptions html;
  html.appVersion = "2.0.0";
  auto finished = m.finishService(f.device, &checklist, html, f.ts.session);
  REQUIRE(finished.has_value());
  CHECK(finished->record.status == "erledigt");
  CHECK(finished->record.duration == 3);
  CHECK(finished->record.activeService.empty());
  CHECK(finished->record.syncFolder.empty());
  CHECK(util::pathExists(serviceFolder / "notes.txt"));
  CHECK(util::pathExists(serviceFolder / "0601-1234_Checklist_2026.03.15.json"));
  CHECK(util::pathExists(serviceFolder / "0601-1234_info.json"));
  CHECK(util::pathExists(serviceFolder / "0601-1234_SR_2026-03-15.html"));
  CHECK(util::pathExists(f.device.path / "0601-1234_SR_2026-03-15.html"));

  // history
  auto history = loadHistoryRows(f.dir / "sync/History.json");
  REQUIRE(history.has_value());
  REQUIRE(history->size() == 1);
  CHECK((*history)[0].device == "0601-1234");
  CHECK((*history)[0].record.activeService == "Service_2026.03.15");
  CHECK((*history)[0].totalPrice == doctest::Approx(150.5));
  CHECK((*history)[0].endDate == "2026.03.18");

  // after finishing, the checklist is opened from the device folder
  const auto afterFinish = m.findChecklist(f.device, "Service_2026.03.15", finished->record);
  REQUIRE(afterFinish.has_value());
  CHECK(afterFinish->parent_path() == serviceFolder);

  // sync list still contains the device as "erledigt"
  CHECK((*m.recordFor(f.device)).status == "erledigt");
  CHECK_FALSE(m.finishService(f.device, nullptr, html, f.ts.session).has_value());

  // remove from list
  REQUIRE(m.setStatus(f.device, ""));
  CHECK((*m.recordFor(f.device)).status.empty());
}

TEST_CASE("external service: plan, work locally, finish extern, finish service") {
  Fixture f;
  auto &m = *f.manager;
  test::writeText(f.device.path / "Service_2020.01.01/alt.txt", "alter Service");

  CHECK_FALSE(m.createExternService(f.device, "kein datum", "", f.ts.session).has_value());
  auto planned = m.createExternService(f.device, "2026.04.01", "Kunde: Extern", f.ts.session);
  REQUIRE(planned.has_value());
  CHECK(planned->status == "geplant extern");
  CHECK(planned->additionalInfo == "Geplant für: 2026.04.01\nKunde: Extern");
  const auto serverChecklist = f.device.path / "Service_2026.04.01/0601-1234_Checklist_2026.04.01.json";
  CHECK(util::pathExists(serverChecklist));
  const auto local = m.externCopyFor(f.device);
  CHECK(util::pathExists(local / "Service_2020.01.01/alt.txt")); // complete device copy
  CHECK((*m.environment().externStore.find("0601-1234")).has_value());

  // the checklist of the local copy is used, its mirror is the file on the server
  auto path = m.findChecklist(f.device, "Service_2026.04.01", *planned);
  REQUIRE(path.has_value());
  CHECK(path->parent_path() == local / "Service_2026.04.01");
  CHECK(m.mirrorOf(f.device, *path) == serverChecklist);
  CHECK_FALSE(m.mirrorOf(f.device, f.dir / "anderswo/Checklist.json").has_value());

  // finishing in house is refused while the technician works on the copy
  CHECK_FALSE(m.finishService(f.device, nullptr, {}, f.ts.session).has_value());

  // work at the customer (local) and a colleague in house (server) at the same time
  {
    auto localDoc = checklist::ChecklistDocument::load(*path);
    REQUIRE(localDoc.has_value());
    auto &table = std::get<checklist::TaskTable>(localDoc->data().sections[0].items[0]);
    table.rows[0].tasks[0].value = 200.0;
    checklist::stamp(table.rows[0].tasks[0], f.ts.session);
    REQUIRE(localDoc->save());
  }
  {
    auto serverDoc = checklist::ChecklistDocument::load(serverChecklist);
    REQUIRE(serverDoc.has_value());
    f.ts.advance(10);
    auto &table = std::get<checklist::TaskTable>(serverDoc->data().sections[0].items[0]);
    table.rows[1].tasks[0].value = 75.0;
    checklist::stamp(table.rows[1].tasks[0], f.ts.session);
    REQUIRE(serverDoc->save());
  }
  test::writeText(local / "Service_2026.04.01/bericht.txt", "beim Kunden erstellt");

  auto back = m.finishExternService(f.device);
  REQUIRE(back.has_value());
  CHECK(back->status == "extern abgeschlossen");
  CHECK(util::pathExists(f.device.path / "Service_2026.04.01/bericht.txt"));
  CHECK_FALSE(util::pathExists(local)); // local copy removed
  CHECK((*m.environment().externStore.find("0601-1234")).has_value());
  CHECK((*m.environment().syncStore.find("0601-1234"))->status == "extern abgeschlossen");
  {
    // both changes survive the copy back
    auto merged = checklist::ChecklistDocument::load(serverChecklist);
    REQUIRE(merged.has_value());
    const auto &table = std::get<checklist::TaskTable>(merged->data().sections[0].items[0]);
    CHECK(std::get<double>(table.rows[0].tasks[0].value) == 200.0);
    CHECK(std::get<double>(table.rows[1].tasks[0].value) == 75.0);
  }
  CHECK_FALSE(m.finishExternService(f.device).has_value()); // only once

  // "Service fertigmelden" in house
  f.ts.advance(3 * 24 * 3600);
  const auto server = checklist::ChecklistDocument::load(serverChecklist);
  REQUIRE(server.has_value());
  auto finished = m.finishService(f.device, &server->data(), {}, f.ts.session);
  REQUIRE(finished.has_value());
  CHECK(finished->record.status == "erledigt extern");
  CHECK(util::pathExists(finished->reportFile));
  CHECK_FALSE((*m.environment().externStore.find("0601-1234")).has_value());
  auto externRows = loadHistoryRows(f.dir / "sync/History_extern.json");
  REQUIRE(externRows.has_value());
  REQUIRE(externRows->size() == 1);
  CHECK((*externRows)[0].totalPrice == doctest::Approx(275.0));
  CHECK(loadHistoryRows(f.dir / "sync/History.json")->empty()); // not in the in house history
}

TEST_CASE("external service: device folder not reachable") {
  Fixture f;
  REQUIRE(f.manager->createExternService(f.device, "2026.04.01", "", f.ts.session).has_value());
  util::fs::rename(f.device.path, f.dir / "offline");
  CHECK_FALSE(f.manager->finishExternService(f.device).has_value());
  CHECK(util::pathExists(f.manager->externCopyFor(f.device))); // nothing is lost
}

TEST_CASE("external service: working offline at the customer") {
  Fixture f;
  auto &m = *f.manager;
  auto planned = m.createExternService(f.device, "2026.04.01", "Kunde: Extern", f.ts.session);
  REQUIRE(planned.has_value());
  CHECK(m.online());
  REQUIRE(m.localExternRecords().has_value());
  CHECK(m.localExternRecords()->size() == 1);
  auto marked = m.setStatus(f.device, "extern");
  REQUIRE(marked.has_value());
  CHECK((*m.environment().localExternStore.find("0601-1234"))->status == "extern");

  // network drive gone: sync folder and device folder not reachable
  util::fs::rename(f.dir / "sync", f.dir / "sync_offline");
  util::fs::rename(f.device.path, f.dir / "device_offline");
  CHECK_FALSE(m.online());
  auto record = m.recordFor(f.device);
  REQUIRE(record.has_value());
  CHECK(record->status == "extern");
  CHECK(record->activeService == "Service_2026.04.01");
  CHECK(record->services == std::vector<std::string>{"Service_2026.04.01"}); // from the local copy
  auto checklist = m.findChecklist(f.device, record->activeService, *record);
  REQUIRE(checklist.has_value());
  CHECK(checklist->parent_path() == m.externCopyFor(f.device) / "Service_2026.04.01");
  CHECK_FALSE(m.finishExternService(f.device).has_value()); // only in house

  // back in house
  util::fs::rename(f.dir / "sync_offline", f.dir / "sync");
  util::fs::rename(f.dir / "device_offline", f.device.path);
  REQUIRE(m.finishExternService(f.device).has_value());
  CHECK(m.localExternRecords()->empty()); // removed from the local list
}

TEST_CASE("external service: cancelling removes it from the local list") {
  Fixture f;
  REQUIRE(f.manager->createExternService(f.device, "2026.04.01", "", f.ts.session).has_value());
  REQUIRE(f.manager->cancelExternService(f.device));
  CHECK_FALSE((*f.manager->environment().localExternStore.find("0601-1234")).has_value());
}

TEST_CASE("external service uses the extern templates") {
  Fixture f;
  test::writeText(f.dir / "templates/Checklist_extern.json", checklist::writeJson(Fixture::templateChecklist()));
  auto config = *f.configs.find("0601");
  config.filesExtern = {devices::TemplateFile{util::pathToUtf8(f.dir / "templates/Checklist_extern.json"), "{reader}_Checklist_extern.json"}};
  REQUIRE(f.configs.upsert(config));
  REQUIRE(f.manager->createExternService(f.device, "2026.04.01", "", f.ts.session).has_value());
  CHECK(util::pathExists(f.device.path / "Service_2026.04.01/0601-1234_Checklist_extern.json"));
  CHECK_FALSE(util::pathExists(f.device.path / "Service_2026.04.01/0601-1234_Checklist_2026.04.01.json"));
}

TEST_CASE("external service can be cancelled") {
  Fixture f;
  REQUIRE(f.manager->createExternService(f.device, "2026.04.01", "", f.ts.session).has_value());
  REQUIRE(f.manager->cancelExternService(f.device));
  CHECK((*f.manager->recordFor(f.device)).status.empty());
  CHECK(util::pathExists(f.manager->externCopyFor(f.device))); // local work is kept
}

TEST_CASE("history: legacy lists and statistics") {
  test::TempDir dir;
  test::writeText(dir / "History.json", R"([
    {"name": "0601-1", "services": [
      {"activeservice": "Service_2025.01.01", "additionalinfo": "Grund: Reparatur", "duration": 5, "path": "Y:/d/0601-1",
       "status": "erledigt", "total_price": 500.0, "end_date": "2025.01.06", "services": ["Service_2025.01.01"]},
      {"activeservice": "", "status": "erledigt"}
    ]},
    {"name": "0471-2", "services": [
      {"activeservice": "Service_2025.02.01", "additionalinfo": "Upgrade LUM", "duration": 0, "total_price": 0}
    ]},
    {"name": "", "services": []}
  ])");
  auto rows = loadHistoryRows(dir / "History.json");
  REQUIRE(rows.has_value());
  REQUIRE(rows->size() == 2);
  CHECK((*rows)[0].servicePath() == "Y:/d/0601-1/Service_2025.01.01/");
  CHECK((*rows)[0].record.extra.empty());

  const auto all = computeStatistics(*rows);
  CHECK(all.services == 2);
  CHECK(all.repairs == 1);
  CHECK(all.upgrades == 1);
  CHECK(all.goodwill == 1);
  CHECK(all.revenue == doctest::Approx(500.0));
  CHECK(all.averageDuration == doctest::Approx(5.0));
  CHECK(all.missingDuration == std::vector<std::string>{"0471-2"});
  CHECK(computeStatistics(*rows, "0601").services == 1);
  CHECK(projectsOf(*rows) == std::vector<std::string>{"0471", "0601"});

  // a Sync.json can be shown in the history window as well
  test::writeText(dir / "Sync.json", R"([{"name":"0601-1","status":"Technik","activeservice":"Service_x"},{"name":"0601-2","status":"erledigt"}])");
  auto syncRows = loadHistoryRows(dir / "Sync.json");
  REQUIRE(syncRows.has_value());
  REQUIRE(syncRows->size() == 1);
  CHECK((*syncRows)[0].record.status == "Technik");
}

TEST_CASE("status advance logic of update_status_rinfo") {
  checklist::Checklist c;
  for (const char *name : {"Wareneingang", "Technik", "Versand"}) {
    checklist::Section s;
    s.name = name;
    s.postAction = std::string(kUpdateStatusAction);
    s.postActionArgs = name;
    c.sections.push_back(s);
  }
  c.sections[1].finished = true; // "Technik" is the current status
  CHECK(shouldAdvanceStatus(c, c.sections[2], "Technik"));
  CHECK_FALSE(shouldAdvanceStatus(c, c.sections[0], "Technik"));
  CHECK(shouldAdvanceStatus(c, c.sections[0], "irgendwas"));
  // the external workflow status is never replaced by the checklist
  for (const char *external : {"geplant extern", "extern", "extern abgeschlossen"})
    CHECK_FALSE(shouldAdvanceStatus(c, c.sections[2], external));
}
