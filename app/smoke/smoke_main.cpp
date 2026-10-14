// Headless smoke test of the complete UI (Linux CI, ST_APP_COMPILE_CHECK=ON).
//
// Builds a realistic environment in a temp folder (device folders, sync drive,
// configs, users, orders, change requests), initialises all modules and renders
// frames of every window while walking through the main workflows. Dear ImGui's
// own assertions (Begin/End, ID stack, popups ...) abort the test on misuse, and
// any error message of the program counts as a failure.
#include "framework/app_context.h"
#include "framework/module.h"
#include "modules/changes/changes_module.h"
#include "modules/checklist/checklist_module.h"
#include "modules/configs/configs_module.h"
#include "modules/devices/devices_module.h"
#include "modules/extern/extern_module.h"
#include "modules/general/general_module.h"
#include "modules/history/history_module.h"
#include "modules/orders/orders_module.h"
#include "modules/service/service_module.h"
#include "modules/users/users_module.h"

#include <st/checklist/actions.h>
#include <st/checklist/formats.h>
#include <st/users/user_store.h>
#include <st/util/json.h>
#include <st/util/log.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui/ImGui_windowcontrol.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <random>
#include <set>
#include <thread>

namespace smoke {
extern std::vector<std::string> pendingDrops;
}

using namespace st;
namespace fs = std::filesystem;

namespace {

int gErrors = 0;

#define SMOKE_CHECK(cond)                                                     \
  do {                                                                        \
    if (!(cond)) {                                                            \
      std::fprintf(stderr, "SMOKE CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++gErrors;                                                              \
    }                                                                         \
  } while (0)

void write(const fs::path &file, const std::string &content) {
  if (!util::writeFileAtomic(file, content)) {
    std::fprintf(stderr, "cannot write %s\n", file.string().c_str());
    std::exit(3);
  }
}

checklist::Checklist templateChecklist() {
  using namespace checklist;
  Checklist c;
  c.version = "1.0";
  Section receipt;
  receipt.name = "Wareneingang";
  receipt.postAction = "update_status_rinfo";
  receipt.postActionArgs = "Technik";
  receipt.permissions = {"TECHSUPPORT"};
  receipt.items.emplace_back(Task::make("Verpackung ok", false));
  TaskGroup group;
  group.name = "Zubehör";
  group.tasks = {Task::make("Kabel", 0), Task::make("Handbuch", Combo{{"Ja", "Nein"}, ""}), Task::make("Datum", Date{})};
  receipt.items.emplace_back(group);
  Task serial = Task::make("Seriennummer", Text{});
  serial.expression = "@rinfo.serial";
  receipt.items.emplace_back(serial);
  c.sections.push_back(receipt);

  Section prices;
  prices.name = "Preisliste";
  prices.postAction = std::string(kAutoFinishAction);
  TaskTable table;
  table.name = "Preisliste (Rabatt bereits berechnet)";
  table.templateRow.tasks = {Task::make("Artikel", Text{}), Task::make("Preis", 0.0), Task::make("Notiz", TextBox{})};
  table.rows = {table.templateRow};
  prices.items.emplace_back(table);
  prices.postPopup = {"Fertig", "Preisliste", "Bitte Angebot prüfen"};
  c.sections.push_back(prices);
  return c;
}

} // namespace

int main() {
  st::log::setSink([](st::log::Level level, std::string_view message) {
    if (level == st::log::Level::Error) {
      ++gErrors;
      std::fprintf(stderr, "[ERROR] %.*s\n", static_cast<int>(message.size()), message.data());
    }
  });

  const fs::path root = fs::temp_directory_path() / ("st_smoke_" + std::to_string(std::random_device{}()));
  fs::create_directories(root);

  // --- environment -------------------------------------------------------------
  const fs::path devices = root / "Geraete";
  const fs::path device = devices / "0601 FLUOstar" / "0601-1234";
  fs::create_directories(device / "Service_2024.01.10");
  fs::create_directories(devices / "0471 PHERAstar" / "0471-77");
  write(device / "Service_2024.01.10" / "Reader_Info.txt", "Reader active time: 1.234:00 h\nProcessed wells: 4.200\n");
  write(device / "Service_2024.01.10" / "EEPROM.btc", "E 100 1 ;LUM\n");

  const fs::path sync = root / "SYNC";
  fs::create_directories(sync / "oldhistory");
  write(sync / "Sync.json", R"([{"name":"0471-77","path":"","status":"Technik","activeservice":"","syncfolder":"","additionalinfo":"Kunde: X","services":[],"duration":0}])");
  write(sync / "History.json", R"([{"name":"0471-77","services":[{"activeservice":"Service_2023.05.01","additionalinfo":"Reparatur","duration":4,"total_price":120.0,"end_date":"2023.05.05","status":"erledigt","path":""}]}])");

  const fs::path configs = root / "configs";
  write(root / "templates" / "Checklist.json", checklist::writeJson(templateChecklist()));
  write(configs / "configs.json", util::json::dump(util::json::Json::array({{{"project", "0601"},
                                                                             {"folders", {"Bilder"}},
                                                                             {"files", {{{"path", util::pathToUtf8(root / "templates" / "Checklist.json")}, {"rename", "{reader}_Checklist.json"}}}},
                                                                             {"addresses", {{{"address", 100}, {"flagToSet", 4}, {"validationvalue", 1}, {"foldersToAdd", {"LUM"}}}}}}})));

  users::User tester{"smoketest", {"SERVICE_CREATOR", "TECHSUPPORT", "SALES"}, true};
  write(root / "users.bin", users::encodeUserTable({tester}));
  fs::create_directories(root / "REQUESTS");
  write(root / "orders.json", R"({"Bestellungen":[{"id":42,"Bestellnummer":"ORD-1","Kunde":"BMG UK","Eingangsdatum":"2026.01.01","Gerichtet Datum":"2026.01.02","Letzte Änderung":"2026-01-02 10:00:00"}]})");

  config::Settings settings;
  settings.paths.usersFile = util::pathToUtf8(root / "users.bin");
  settings.paths.requestsDir = util::pathToUtf8(root / "REQUESTS");
  settings.paths.devicesRoot = util::pathToUtf8(devices);
  settings.paths.configsDir = util::pathToUtf8(configs);
  settings.paths.syncDir = util::pathToUtf8(sync);
  settings.paths.externLocalDir = util::pathToUtf8(root / "local" / "extern");
  settings.paths.externFile = util::pathToUtf8(sync / "extern.json");
  settings.paths.ordersFile = util::pathToUtf8(root / "orders.json");
  settings.paths.crashDir = util::pathToUtf8(root / "crashes");
  settings.autosaveDelayMs = 1;
  settings.refreshIntervalMs = 200;

  // --- ImGui without a renderer ------------------------------------------------
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = ImVec2(1600.0f, 1000.0f);
  io.DeltaTime = 1.0f / 60.0f;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  unsigned char *pixels = nullptr;
  int width = 0;
  int height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  {
    app::AppContext ctx(settings, "smoketest", root / "work");
    fs::create_directories(root / "work");
    ctx.initialize();
    SMOKE_CHECK(ctx.isPoweruser());
    SMOKE_CHECK(ctx.deviceRegistry.size() == 2);

    app::ModuleRegistry modules;
    modules.add<app::GeneralModule>();
    modules.add<app::DevicesModule>();
    modules.add<app::ServiceModule>();
    modules.add<app::ChecklistModule>();
    modules.add<app::ExternModule>();
    modules.add<app::HistoryModule>();
    modules.add<app::OrdersModule>();
    auto &configsModule = modules.add<app::ConfigsModule>();
    auto &usersModule = modules.add<app::UsersModule>();
    auto &changesModule = modules.add<app::ChangesModule>();
    modules.initAll(ctx);
    SMOKE_CHECK(modules.activeModules().size() == 10);

    const auto frames = [&](int count) {
      for (int i = 0; i < count; ++i) {
        ctx.update();
        modules.updateAll(ctx);
        ImGui::NewFrame();
        WindowControl::Get().DrawWindows();
        modules.drawOverlays(ctx);
        ctx.notify.draw();
        ImGui::Render();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
    };

    // Moves the mouse over the whole screen (hover states, tooltips) and reports
    // conflicting ImGui IDs (two widgets with the same ID in one window).
    int idConflicts = 0;
    std::set<std::string> hovered;
    const auto sweep = [&](const char *stage) {
      for (float y = 5.0f; y < io.DisplaySize.y; y += 23.0f) {
        for (float x = 5.0f; x < io.DisplaySize.x; x += 61.0f) {
          io.AddMousePosEvent(x, y);
          ctx.update();
          ImGui::NewFrame();
          WindowControl::Get().DrawWindows();
          modules.drawOverlays(ctx);
          ctx.notify.draw();
          ImGui::Render();
          if (std::getenv("SMOKE_VERBOSE") && GImGui->HoveredWindow)
            hovered.insert(GImGui->HoveredWindow->RootWindow->Name);
          if (GImGui->DebugDrawIdConflictsId != 0) {
            std::fprintf(stderr, "ID conflict (%s) at %.0f/%.0f id=%08X\n", stage, x, y, GImGui->DebugDrawIdConflictsId);
            ++idConflicts;
          }
        }
      }
    };

    std::puts("frames: start");
    frames(5);
    // Arrange all docked windows in a grid so the mouse sweep reaches every one
    {
      const char *names[] = {"Reader Control", "Reader Filewalker", "ReaderInfo", "Konfiguration", "Service Control",
                             "Checkliste", "Checklisten Editor", "Externe Services", "Service Historie", "Bestellungen"};
      ImGui::NewFrame();
      int i = 0;
      for (const char *name : names) {
        const float w = io.DisplaySize.x / 4.0f;
        const float h = (io.DisplaySize.y - 30.0f) / 3.0f;
        ImGui::SetWindowPos(name, ImVec2(static_cast<float>(i % 4) * w, 30.0f + static_cast<float>(i / 4) * h));
        ImGui::SetWindowSize(name, ImVec2(w, h));
        ++i;
      }
      ImGui::Render();
    }

    std::puts("frames: device selected");
    ctx.selectDevice("0601-1234");
    SMOKE_CHECK(ctx.activeDevice() != nullptr);
    SMOKE_CHECK(devices::hasFlag(ctx.activeDevice()->flags, devices::ConfigFlag::LUM));
    SMOKE_CHECK(ctx.activeDevice()->info.processedWells == 4200);
    frames(5);

    std::puts("frames: service created");
    auto created = ctx.services().createService(*ctx.activeDevice(), ctx.session());
    SMOKE_CHECK(created.has_value());
    if (created)
      ctx.applyRecord(*created);
    SMOKE_CHECK(ctx.checklist().isOpen());
    frames(5);

    std::puts("frames: checklist edited, autosave");
    if (ctx.checklist().isOpen()) {
      auto &task = std::get<checklist::Task>(ctx.checklist().doc->data().sections[0].items[0]);
      task.value = true;
      checklist::stamp(task, ctx.session());
      ctx.checklist().markDirty();
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      frames(3);
      SMOKE_CHECK(!ctx.checklist().dirty);
      auto reloaded = checklist::ChecklistDocument::load(ctx.checklist().doc->path());
      SMOKE_CHECK(reloaded && std::get<bool>(std::get<checklist::Task>(reloaded->data().sections[0].items[0]).value));
    }

    std::puts("frames: section finished (status action)");
    if (ctx.checklist().isOpen()) {
      auto &doc = *ctx.checklist().doc;
      auto &section = doc.data().sections[0];
      checklist::prepareSectionFinish(section, ctx.expressions, ctx.session());
      checklist::finishSection(doc.data(), section, ctx.sectionActions, ctx.session());
      SMOKE_CHECK(ctx.activeRecord().status == "Technik");
      SMOKE_CHECK(std::get<checklist::Text>(std::get<checklist::Task>(section.items[2]).value).value == "1234");
      ctx.checklist().markDirty();
      frames(5);
      sweep("checklist");
    }

    std::puts("frames: dialogs");
    configsModule.show("0601");
    usersModule.show("smoketest");
    changesModule.showSubmit();
    changesModule.showOverview();
    frames(5);
    sweep("dialogs");

    std::puts("frames: finish service");
    if (ctx.checklist().isOpen()) {
      checklist::finishChecklist(ctx.checklist().doc->data(), ctx.expressions, ctx.sectionActions, ctx.session());
      SMOKE_CHECK(ctx.checklist().doc->save().has_value());
      auto result = ctx.services().finishService(*ctx.activeDevice(), &ctx.checklist().doc->data(), app::ServiceModule::htmlOptions(ctx), ctx.session());
      SMOKE_CHECK(result.has_value());
      if (result)
        ctx.applyRecord(result->record);
      const std::string service = ctx.selectedService();
      ctx.selectService({});
      ctx.selectService(service);
      SMOKE_CHECK(ctx.activeRecord().status == "erledigt");
      SMOKE_CHECK(ctx.checklist().isOpen());
      frames(10);
    }

    std::puts("frames: external service");
    auto planned = ctx.services().createExternService(*ctx.activeDevice(), "2026.12.01", "Kunde: Extern", ctx.session());
    SMOKE_CHECK(planned.has_value());
    if (planned)
      ctx.applyRecord(*planned);
    frames(5);
    sweep("extern");

    std::puts("frames: other device, no device");
    ctx.selectDevice("0471-77");
    frames(5);
    ctx.selectDevice("");
    frames(5);

    modules.shutdownAll(ctx);
    SMOKE_CHECK(idConflicts == 0);
    for (const auto &name : hovered)
      std::printf("hovered window: %s\n", name.c_str());
  }
  ImGui::DestroyContext();
  std::error_code ec;
  fs::remove_all(root, ec);

  if (gErrors > 0) {
    std::fprintf(stderr, "SMOKE TEST FAILED (%d errors)\n", gErrors);
    return 1;
  }
  std::puts("SMOKE TEST OK");
  return 0;
}
