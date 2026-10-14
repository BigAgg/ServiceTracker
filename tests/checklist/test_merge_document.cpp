#include <doctest/doctest.h>

#include "st/checklist/actions.h"
#include "st/checklist/document.h"
#include "st/checklist/merge.h"
#include "test_helpers.h"

using namespace st;
using namespace st::checklist;

namespace {
Checklist sample() {
  Checklist c;
  Section s;
  s.name = "Technik";
  Task single;
  single.name = "Sichtprüfung";
  single.value = false;
  s.items.emplace_back(single);
  TaskGroup group;
  group.tasks = {Task::make("A", 0), Task::make("B", 0)};
  s.items.emplace_back(group);
  TaskTable table;
  table.name = "Teile";
  table.templateRow.tasks = {Task::make("Teil", Text{}), Task::make("Preis", 0.0)};
  table.rows = {table.templateRow};
  s.items.emplace_back(table);
  c.sections.push_back(s);
  return c;
}

Task &single(Checklist &c) { return std::get<Task>(c.sections[0].items[0]); }
TaskGroup &group(Checklist &c) { return std::get<TaskGroup>(c.sections[0].items[1]); }
TaskTable &table(Checklist &c) { return std::get<TaskTable>(c.sections[0].items[2]); }
} // namespace

TEST_CASE("merge: newer task wins, older is ignored") {
  Checklist local = sample();
  Checklist remote = sample();
  single(remote).value = true;
  single(remote).timestamp = "2026-01-01 10:00:00";
  CHECK(mergeNewer(local, remote) == 1);
  CHECK(std::get<bool>(single(local).value));

  single(local).value = false;
  single(local).timestamp = "2026-01-01 11:00:00";
  CHECK(mergeNewer(local, remote) == 0);
  CHECK_FALSE(std::get<bool>(single(local).value));
}

TEST_CASE("merge: concurrent edits of different tasks are combined") {
  Checklist base = sample();
  Checklist userA = base;
  Checklist userB = base;
  group(userA).tasks[0].value = 5;
  group(userA).tasks[0].timestamp = "2026-01-01 10:00:00";
  group(userB).tasks[1].value = 7;
  group(userB).tasks[1].timestamp = "2026-01-01 10:00:01";
  CHECK(mergeNewer(userA, userB) == 1);
  CHECK(std::get<int>(group(userA).tasks[0].value) == 5);
  CHECK(std::get<int>(group(userA).tasks[1].value) == 7);
}

TEST_CASE("merge: section finish state follows the newer timestamp") {
  Checklist local = sample();
  Checklist remote = sample();
  remote.sections[0].finished = true;
  remote.sections[0].user = "b";
  remote.sections[0].timestamp = "2026-01-02 00:00:00";
  CHECK(mergeNewer(local, remote) == 1);
  CHECK(local.sections[0].finished);
  CHECK(local.sections[0].user == "b");
}

TEST_CASE("merge: checklist finish state") {
  Checklist local = sample();
  Checklist remote = sample();
  remote.finished = true;
  remote.timestamp = "2026-01-02 00:00:00";
  remote.user = "x";
  CHECK(mergeNewer(local, remote) == 1);
  CHECK(local.finished);
}

TEST_CASE("merge: tables") {
  Checklist local = sample();
  Checklist remote = sample();

  SUBCASE("remote added rows more recently: rows are taken, newer local cells kept") {
    table(local).rows[0].tasks[0].value = Text{"lokal"};
    table(local).rows[0].tasks[0].timestamp = "2026-01-01 12:00:00";
    table(remote).rows.push_back(table(remote).templateRow);
    table(remote).rows[1].tasks[0].value = Text{"neu"};
    table(remote).rows[1].tasks[0].timestamp = "2026-01-01 11:00:00";
    table(remote).timestamp = "2026-01-01 11:00:00";
    CHECK(mergeNewer(local, remote) > 0);
    REQUIRE(table(local).rows.size() == 2);
    CHECK(std::get<Text>(table(local).rows[0].tasks[0].value).value == "lokal");
    CHECK(std::get<Text>(table(local).rows[1].tasks[0].value).value == "neu");
    CHECK(table(local).timestamp == "2026-01-01 11:00:00");
  }
  SUBCASE("local structure newer: remote cells merged by index") {
    table(local).timestamp = "2026-01-01 12:00:00";
    table(local).rows.push_back(table(local).templateRow);
    table(remote).rows[0].tasks[1].value = 9.5;
    table(remote).rows[0].tasks[1].timestamp = "2026-01-01 11:00:00";
    CHECK(mergeNewer(local, remote) == 1);
    CHECK(table(local).rows.size() == 2);
    CHECK(std::get<double>(table(local).rows[0].tasks[1].value) == 9.5);
  }
}

TEST_CASE("merge: different layouts are not merged item by item") {
  Checklist local = sample();
  Checklist remote = sample();
  remote.sections[0].items.pop_back();
  single(remote).timestamp = "2030-01-01 00:00:00";
  CHECK(mergeNewer(local, remote) == 0);
}

TEST_CASE("merge: sections are matched by name") {
  Checklist local = sample();
  Checklist remote = sample();
  Section extra;
  extra.name = "Nur remote";
  extra.timestamp = "2030-01-01 00:00:00";
  remote.sections.insert(remote.sections.begin(), extra);
  auto &remoteTask = std::get<Task>(remote.sections[1].items[0]);
  remoteTask.timestamp = "2030-01-01 00:00:00";
  remoteTask.value = true;
  CHECK(mergeNewer(local, remote) == 1);
  CHECK(local.sections.size() == 1);
  CHECK(std::get<bool>(single(local).value));
}

TEST_CASE("document: two users editing the same file") {
  test::TempDir dir;
  for (const char *name : {"Checklist.txt", "Checklist.json"}) {
    CAPTURE(name);
    const auto path = dir / name;
    ChecklistDocument seed(path, sample());
    REQUIRE(seed.saveAs(path));

    auto a = ChecklistDocument::load(path);
    auto b = ChecklistDocument::load(path);
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());

    test::TestSession sa;
    test::TestSession sb;
    sb.advance(5);

    single(a->data()).value = true;
    stamp(single(a->data()), sa.session);
    group(b->data()).tasks[1].value = 3;
    stamp(group(b->data()).tasks[1], sb.session);

    REQUIRE(a->save());
    REQUIRE(b->save()); // merges the change of user A before writing

    auto reloaded = ChecklistDocument::load(path);
    REQUIRE(reloaded.has_value());
    CHECK(std::get<bool>(single(reloaded->data()).value));
    CHECK(std::get<int>(group(reloaded->data()).tasks[1].value) == 3);

    // user A pulls the change of user B
    CHECK(a->changedOnDisk());
    auto merged = a->refresh();
    REQUIRE(merged.has_value());
    CHECK(*merged == 1);
    CHECK(std::get<int>(group(a->data()).tasks[1].value) == 3);
    CHECK_FALSE(a->changedOnDisk());
  }
}

TEST_CASE("document: saving unchanged data does not touch the file") {
  test::TempDir dir;
  const auto path = dir / "Checklist.json";
  ChecklistDocument doc(path, sample());
  REQUIRE(doc.saveAs(path));
  const auto before = st::util::lastWriteTime(path);
  auto loaded = ChecklistDocument::load(path);
  REQUIRE(loaded.has_value());
  REQUIRE(loaded->save());
  CHECK(st::util::lastWriteTime(path) == before);
}

TEST_CASE("document: load errors") {
  test::TempDir dir;
  CHECK_FALSE(ChecklistDocument::load(dir / "missing.json").has_value());
  test::writeText(dir / "broken.json", "{");
  CHECK_FALSE(ChecklistDocument::load(dir / "broken.json").has_value());
  ChecklistDocument closed;
  CHECK_FALSE(closed.save().has_value());
}

TEST_CASE("document: attachments folder lives next to the file") {
  ChecklistDocument doc("/x/y/Checklist.json", {});
  CHECK(doc.attachmentsDir() == std::filesystem::path("/x/y/attachments"));
}
