// Headless smoke test of the complete UI (Linux CI, ST_APP_COMPILE_CHECK=ON).
//
// Builds a realistic environment in a temp folder (device folders, sync drive,
// configs, users, orders, change requests), initialises all modules and renders
// frames of every window while walking through the main workflows. Dear ImGui's
// own assertions (Begin/End, ID stack, popups ...) abort the test on misuse, and
// any error message of the program counts as a failure.
#include "framework/app_context.h"
#include "framework/desktop_notifier.h"
#include "framework/events.h"
#include "framework/module.h"
#include "framework/widgets.h"
#include "modules/changes/changes_module.h"
#include "modules/checklist/checklist_module.h"
#include "modules/configs/configs_module.h"
#include "modules/devices/devices_module.h"
#include "modules/extern/extern_module.h"
#include "modules/general/general_module.h"
#include "modules/history/history_module.h"
#include "modules/orders/orders_module.h"
#include "modules/records/records_module.h"
#include "modules/service/service_module.h"
#include "modules/statusalerts/statusalerts_module.h"
#include "modules/users/users_module.h"

#include <st/alerts/status_rules.h>
#include <st/checklist/actions.h>
#include <st/checklist/document.h>
#include <st/checklist/formats.h>
#include <st/checklist/translation.h>
#include <st/exporting/mail.h>
#include <st/orders/order_store.h>
#include <st/service/history.h>
#include <st/users/user_store.h>
#include <st/util/json.h>
#include <st/util/log.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui/ImGui_windowcontrol.h>

#include <algorithm>
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
  settings.paths.translationsFile = util::pathToUtf8(configs / "translations_de-en.json");
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
    auto notifierOwner = std::make_unique<app::RecordingNotifier>();
    auto &notifier = *notifierOwner;
    ctx.alerts.setNotifier(std::move(notifierOwner));
    ctx.initialize();
    SMOKE_CHECK(ctx.isPoweruser());
    SMOKE_CHECK(ctx.deviceRegistry.size() == 2);

    app::ModuleRegistry modules;
    auto &generalModule = modules.add<app::GeneralModule>();
    modules.add<app::DevicesModule>();
    modules.add<app::ServiceModule>();
    modules.add<app::ChecklistModule>();
    modules.add<app::ExternModule>();
    modules.add<app::HistoryModule>();
    modules.add<app::OrdersModule>();
    auto &configsModule = modules.add<app::ConfigsModule>();
    auto &usersModule = modules.add<app::UsersModule>();
    auto &changesModule = modules.add<app::ChangesModule>();
    modules.add<app::StatusAlertsModule>();
    auto &recordsModule = modules.add<app::RecordsModule>();
    modules.initAll(ctx);
    SMOKE_CHECK(modules.activeModules().size() == 12);

    const auto frames = [&](int count) {
      for (int i = 0; i < count; ++i) {
        ctx.update();
        modules.updateAll(ctx);
        ImGui::NewFrame();
        WindowControl::Get().DrawWindows();
        modules.drawOverlays(ctx);
        ctx.drawOverlay();
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
          ctx.drawOverlay();
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

    std::puts("frames: orders expanded (striped rows)");
    {
      // Open the group "Versandbereit" and order 42 like a user click would
      // (same ID stack as OrdersModule::drawWindow / drawOrderEntry).
      ImGuiWindow *window = ImGui::FindWindowByName("Bestellungen");
      SMOKE_CHECK(window != nullptr);
      const float collapsedHeight = window ? window->ContentSize.y : 0.0f;
      if (window) {
        const int state = static_cast<int>(orders::OrderState::Prepared);
        const ImGuiID group = ImHashStr("##state", 0, ImHashData(&state, sizeof(state), window->ID));
        const int order = 42;
        const ImGuiID entry = ImHashStr("ORD-1 -> BMG UK", 0, ImHashData(&order, sizeof(order), group));
        window->StateStorage.SetInt(group, 1);
        window->StateStorage.SetInt(entry, 1);
      }
      SMOKE_CHECK(!ctx.prefs().ordersAsPopup);
      frames(3);
      SMOKE_CHECK(window && window->ContentSize.y > collapsedHeight + 100.0f); // order editor is open
      sweep("orders");
    }

    std::puts("frames: autocomplete input");
    {
      // ui::autocompleteInput: focus the field, suggestion list opens, Enter takes the first match
      app::ui::AutocompleteState state;
      state.refocus = true;
      std::string customer = "bmg u";
      const std::vector<std::string> candidates = {"Rota Lab Turkey", "BMG USA", "BMG UK", "Isogen Netherlands"};
      const auto draw = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(400.0f, 400.0f));
        ImGui::Begin("Smoke Autocomplete");
        app::ui::autocompleteInput("Kunde", customer, candidates, state);
        ImGui::End();
        ImGui::Render();
      };
      for (int i = 0; i < 3; ++i)
        draw();
      bool listShown = false;
      for (ImGuiWindow *window : GImGui->Windows)
        listShown |= window->Active && std::string_view(window->Name).starts_with("##autocomplete");
      SMOKE_CHECK(listShown);
      io.AddKeyEvent(ImGuiKey_Enter, true);
      draw();
      io.AddKeyEvent(ImGuiKey_Enter, false);
      draw();
      SMOKE_CHECK(customer == "BMG USA");
    }

    std::puts("frames: notifications");
    {
      // Start: one order is ready for shipping -> Windows notification + red dot
      SMOKE_CHECK(ctx.alerts.subscribed("orders.prepared"));
      SMOKE_CHECK(notifier.shown.size() == 1);
      if (!notifier.shown.empty()) {
        SMOKE_CHECK(notifier.shown[0].title == "ServiceTracker - Bestellungen");
        SMOKE_CHECK(notifier.shown[0].body == "Es ist 1 Bestellung versandbereit");
      }
      SMOKE_CHECK(notifier.badge);
      SMOKE_CHECK(notifier.lastPersistent); // stays until clicked/closed by default
      SMOKE_CHECK(notifier.tooltip.find("Es ist 1 Bestellung versandbereit") != std::string::npos);

      // Click on the notification focuses the orders window
      notifier.activation = app::DesktopNotifier::Activation{"orders.prepared", "orders.prepared"};
      frames(2);
      SMOKE_CHECK(GImGui->NavWindow && std::string(GImGui->NavWindow->RootWindow->Name).starts_with("Bestellungen"));

      // Another user prepares an order -> new notification with the total count
      util::FixedClock clock(util::makeLocalTime(2026, 2, 1, 9, 0, 0));
      util::Session other("kollege", clock);
      orders::OrderStore store(root / "orders.json");
      SMOKE_CHECK(store.load().has_value());
      orders::Order order;
      order.orderNumber = "ORD-2";
      order.customer = "BMG DE";
      order.entryDate = "2026.01.20";
      order.bookedDate = "2026.01.21";
      order.preparedDate = "2026.01.22";
      SMOKE_CHECK(store.upsert(order, other).has_value());
      std::this_thread::sleep_for(std::chrono::milliseconds(600));
      frames(3);
      SMOKE_CHECK(notifier.shown.size() == 2);
      if (notifier.shown.size() == 2)
        SMOKE_CHECK(notifier.shown[1].body == "Es sind 2 Bestellungen versandbereit");

      // Program in front: the message stays inside the program
      notifier.foreground = true;
      order.id = 0;
      order.orderNumber = "ORD-3";
      SMOKE_CHECK(store.upsert(order, other).has_value());
      std::this_thread::sleep_for(std::chrono::milliseconds(600));
      frames(3);
      SMOKE_CHECK(notifier.shown.size() == 2);
      notifier.foreground = false;

      // Muted channel: no red dot
      ctx.prefs().mutedAlerts = {"orders.prepared"};
      std::this_thread::sleep_for(std::chrono::milliseconds(1100));
      frames(2);
      SMOKE_CHECK(!notifier.badge);
      ctx.prefs().mutedAlerts.clear();
      std::this_thread::sleep_for(std::chrono::milliseconds(1100));
      frames(2);
      SMOKE_CHECK(notifier.badge);

      // Settings menu incl. "Benachrichtigungen" (rendered in a plain window)
      for (int i = 0; i < 3; ++i) {
        ImGui::NewFrame();
        ImGui::Begin("Smoke Einstellungen");
        generalModule.drawMenu(ctx);
        ImGui::End();
        ImGui::Render();
      }
      const auto shownBefore = notifier.shown.size();
      ctx.prefs().persistentNotifications = false;
      ctx.alerts.showTest(ctx, "orders.prepared");
      SMOKE_CHECK(notifier.shown.size() == shownBefore + 1);
      SMOKE_CHECK(!notifier.lastPersistent);
      ctx.prefs().persistentNotifications = true;

      // All orders shipped: notification is withdrawn, red dot disappears
      notifier.withdrawn.clear();
      clock.advance(std::chrono::minutes(5)); // newer change than the module's copy
      SMOKE_CHECK(store.refresh().has_value());
      for (const auto id : store.idsInState(orders::OrderState::Prepared)) {
        orders::Order shipped = *store.find(id);
        shipped.shippingDate = "2026.01.25";
        SMOKE_CHECK(store.upsert(shipped, other).has_value());
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1100));
      frames(3);
      SMOKE_CHECK(std::find(notifier.withdrawn.begin(), notifier.withdrawn.end(), "orders.prepared") != notifier.withdrawn.end());
      SMOKE_CHECK(!notifier.badge);
    }

    std::puts("frames: device selected");
    ctx.selectDevice("0601-1234");
    SMOKE_CHECK(ctx.activeDevice() != nullptr);
    SMOKE_CHECK(devices::hasFlag(ctx.activeDevice()->flags, devices::ConfigFlag::LUM));
    SMOKE_CHECK(ctx.activeDevice()->info.processedWells == 4200);
    frames(5);

    std::puts("frames: service created");
    const auto shownBeforeCreate = notifier.shown.size();
    auto created = ctx.services().createService(*ctx.activeDevice(), ctx.session());
    SMOKE_CHECK(created.has_value());
    if (created)
      ctx.applyRecord(*created);
    SMOKE_CHECK(ctx.checklist().isOpen());
    frames(5);
    SMOKE_CHECK(ctx.statusIndex().statusOf("0601-1234") == "Wareneingang");
    SMOKE_CHECK(notifier.shown.size() == shownBeforeCreate); // own device booked in: no notification

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

    std::puts("frames: record dialog, record and e-mail");
    if (ctx.checklist().isOpen()) {
      ctx.events.publish(app::events::ShowRecordDialog{});
      frames(3);
      SMOKE_CHECK(ImGui::GetTopMostPopupModal() != nullptr);
      sweep("record dialog");
      io.AddKeyEvent(ImGuiKey_Escape, true);
      frames(1);
      io.AddKeyEvent(ImGuiKey_Escape, false);
      frames(2);
      SMOKE_CHECK(ImGui::GetTopMostPopupModal() == nullptr);

      // Record (German): no browser on Linux -> HTML with a warning, opened afterwards
      const auto exportDir = fs::temp_directory_path() / "ServiceTracker";
      recordsModule.createRecord(ctx, {"Wareneingang"}, false, true);
      SMOKE_CHECK(recordsModule.busy());
      frames(2); // progress overlay
      recordsModule.finishPendingJob(ctx);
      frames(2);
      const auto stem = util::pathToUtf8(ctx.checklist().doc->path().stem());
      SMOKE_CHECK(fs::exists(exportDir / (stem + "_ServiceRecord.html")));

      // E-mail: all lines are in the shared translation file (no server in CI)
      {
        struct Prefix final : translate::Backend {
          util::Result<std::vector<std::string>> translate(const std::vector<std::string> &texts) override {
            std::vector<std::string> out;
            for (const auto &t : texts)
              out.push_back("EN " + t);
            return out;
          }
        } backend;
        translate::TranslationCache cache(configs / "translations_de-en.json");
        translate::Translator translator(cache, &backend);
        auto copy = checklist::emailSubset(ctx.checklist().doc->data());
        SMOKE_CHECK(checklist::translateChecklist(copy, translator).has_value());
      }
      const auto serviceFolder = ctx.activeDevice()->path / ctx.selectedService();
      write(serviceFolder / "EV-Plate" / "0601-1234 Reader Evaluation.pdf", "%PDF-1.7");
      SMOKE_CHECK(exporting::findReaderEvaluation(serviceFolder).has_value());
      ctx.events.publish(app::events::CreateServiceMail{});
      frames(2);
      recordsModule.finishPendingJob(ctx);
      frames(2);
      const auto date = service::ServiceManager::serviceDate(ctx.selectedService());
      const auto mailRecord = exportDir / ("0601-1234_Service_Report_" + date + ".html");
      SMOKE_CHECK(fs::exists(mailRecord));
      const std::string html = util::readFile(mailRecord).value_or("");
      SMOKE_CHECK(html.find("EN Wareneingang") != std::string::npos);
      SMOKE_CHECK(html.find("smoketest") == std::string::npos); // no names
      SMOKE_CHECK(html.find("Total Time") == std::string::npos); // no working times

      // E-mail template editor (menu "Configs")
      ctx.events.publish(app::events::ShowMailTemplateEditor{});
      frames(3);
      SMOKE_CHECK(ImGui::FindWindowByName("E-Mail-Vorlage") != nullptr);
      sweep("mail template editor");
    }

    std::puts("frames: table layout saved by table name");
    if (ctx.checklist().isOpen()) {
      const auto &prices = std::get<checklist::TaskTable>(ctx.checklist().doc->data().sections[1].items[0]);
      const ImGuiID saved = ImHashStr("##table", 0, ImHashStr(checklist::tableLayoutKey(prices).c_str()));
      const auto drawTables = [&] {
        app::ChecklistViewState state;
        state.expandAll = true;
        for (int i = 0; i < 3; ++i) {
          ImGui::NewFrame();
          ImGui::Begin("Smoke Tabellen");
          app::ChecklistRenderer renderer(ctx, *ctx.checklist().doc, state, false, [](bool) {});
          renderer.drawSections();
          ImGui::End();
          ImGui::Render();
        }
      };
      ctx.prefs().saveTableLayout = false;
      drawTables();
      SMOKE_CHECK(ImGui::TableFindByID(saved) == nullptr);
      ctx.prefs().saveTableLayout = true;
      drawTables();
      SMOKE_CHECK(ImGui::TableFindByID(saved) != nullptr);
      ctx.prefs().saveTableLayout = false;
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

    std::puts("frames: external service – local checklist mirrored with the device folder");
    {
      SMOKE_CHECK(ctx.checklist().isOpen() && ctx.checklist().mirror.has_value());
      const auto serverChecklist = device / "Service_2026.12.01" / "0601-1234_Checklist.json";
      SMOKE_CHECK(fs::exists(serverChecklist));
      if (ctx.checklist().isOpen() && ctx.checklist().mirror) {
        SMOKE_CHECK(ctx.checklist().doc->path().native().starts_with((root / "local").native()));
        auto &task = std::get<checklist::Task>(ctx.checklist().doc->data().sections[0].items[0]);
        task.value = true;
        checklist::stamp(task, ctx.session());
        ctx.checklist().markDirty();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        frames(3);
        SMOKE_CHECK(ctx.checklist().mirrorState.starts_with("Mit Geräteordner synchronisiert"));
        auto server = checklist::ChecklistDocument::load(serverChecklist);
        SMOKE_CHECK(server && std::get<bool>(std::get<checklist::Task>(server->data().sections[0].items[0]).value));
      }

      std::puts("frames: extern abschließen, service fertigmelden");
      auto back = ctx.services().finishExternService(*ctx.activeDevice());
      SMOKE_CHECK(back.has_value());
      if (back) {
        ctx.applyRecord(*back);
        const std::string service = ctx.selectedService();
        ctx.selectService({});
        ctx.selectService(service);
      }
      frames(5);
      SMOKE_CHECK(ctx.activeRecord().status == "extern abgeschlossen");
      SMOKE_CHECK(!fs::exists(root / "local" / "extern" / "0601-1234"));
      SMOKE_CHECK(ctx.checklist().isOpen() && !ctx.checklist().mirror);
      SMOKE_CHECK(ctx.checklist().isOpen() && ctx.checklist().doc->path() == serverChecklist);
      sweep("extern finished");
      if (ctx.checklist().isOpen()) {
        auto done = ctx.services().finishService(*ctx.activeDevice(), &ctx.checklist().doc->data(), app::ServiceModule::htmlOptions(ctx), ctx.session());
        SMOKE_CHECK(done.has_value());
        if (done)
          ctx.applyRecord(done->record);
      }
      frames(3);
      SMOKE_CHECK(ctx.activeRecord().status == "erledigt extern");
      auto externHistory = service::loadHistoryRows(sync / "History_extern.json");
      SMOKE_CHECK(externHistory && externHistory->size() == 1);
    }

    std::puts("frames: device received by another user");
    {
      service::ServiceRecord arrived;
      arrived.name = "0471-77";
      arrived.status = std::string(service::status::Received);
      std::this_thread::sleep_for(std::chrono::milliseconds(1100)); // new modification time
      SMOKE_CHECK(service::RecordStore(sync / "Sync.json").upsert(arrived).has_value());
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      frames(3);
      SMOKE_CHECK(!notifier.shown.empty() && notifier.shown.back().body == "Neuer Reader im Wareneingang: 0471-77");
      SMOKE_CHECK(!notifier.shown.empty() && notifier.shown.back().key == "devices.received/0471-77");
      // Click selects the device
      notifier.activation = app::DesktopNotifier::Activation{"devices.received", "devices.received/0471-77"};
      frames(2);
      SMOKE_CHECK(ctx.activeDeviceName() == "0471-77");
    }

    std::puts("frames: status notifications");
    {
      // default rules: "versendet" -> TECHSUPPORT, "im Versand" -> SALES (smoke user has both)
      SMOKE_CHECK(ctx.alerts.subscribed("status.versendet"));
      SMOKE_CHECK(ctx.alerts.subscribed("status.im versand"));
      service::ServiceRecord shipped;
      shipped.name = "0471-77";
      shipped.status = "Versendet"; // compared case-insensitively
      std::this_thread::sleep_for(std::chrono::milliseconds(1100));
      SMOKE_CHECK(service::RecordStore(sync / "Sync.json").upsert(shipped).has_value());
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      frames(3);
      SMOKE_CHECK(!notifier.shown.empty() && notifier.shown.back().body == "0471-77: Status „versendet“");
      SMOKE_CHECK(!notifier.shown.empty() && notifier.shown.back().title == "ServiceTracker - versendet");
      ctx.selectDevice("");
      notifier.activation = app::DesktopNotifier::Activation{"status.versendet", "status.versendet/0471-77"};
      frames(2);
      SMOKE_CHECK(ctx.activeDeviceName() == "0471-77");

      // another power user changes the rules: only "Rechnung erstellen" -> SALES
      alerts::StatusRuleStore rules(configs / "statusAlerts.json");
      SMOKE_CHECK(rules.save({{"Rechnung erstellen", {"SALES"}}}).has_value());
      std::this_thread::sleep_for(std::chrono::milliseconds(1100));
      frames(2);
      SMOKE_CHECK(!ctx.alerts.subscribed("status.versendet"));
      SMOKE_CHECK(ctx.alerts.subscribed("status.rechnung erstellen"));

      // editor dialog (menu "Configs")
      ctx.events.publish(app::events::ShowStatusAlertsEditor{});
      frames(3);
      SMOKE_CHECK(ImGui::FindWindowByName("Status-Benachrichtigungen") != nullptr);
      sweep("status alerts editor");
    }

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

  std::puts("offline: external service at the customer (network drive missing)");
  {
    const fs::path offline = root / "offline";
    const fs::path localExtern = offline / "local" / "extern";
    const fs::path copy = localExtern / "0601-1234";
    fs::create_directories(copy / "Service_2026.12.24");
    write(copy / "Service_2026.12.24" / "0601-1234_Checklist.json", checklist::writeJson(templateChecklist()));
    service::ServiceRecord record;
    record.name = "0601-1234";
    record.path = util::pathToUtf8(offline / "Y" / "Geraete" / "0601 FLUOstar" / "0601-1234");
    record.status = "extern";
    record.activeService = "Service_2026.12.24";
    SMOKE_CHECK(service::RecordStore(localExtern / "extern_local.json").upsert(record).has_value());
    // copy without list entry (older version / copied by hand)
    fs::create_directories(localExtern / "0471-77" / "Service_2026.10.01");
    fs::create_directories(offline / "work");
    write(offline / "work" / "users_cache.bin", users::encodeUserTable({users::User{"techniker", {"TECHSUPPORT"}, false}}));

    config::Settings offlineSettings;
    const auto missing = [&](const char *name) { return util::pathToUtf8(offline / "Y" / name); };
    offlineSettings.paths.usersFile = missing("users.bin");
    offlineSettings.paths.requestsDir = missing("REQUESTS");
    offlineSettings.paths.devicesRoot = missing("Geraete");
    offlineSettings.paths.configsDir = missing("configs");
    offlineSettings.paths.syncDir = missing("SYNC");
    offlineSettings.paths.externFile = missing("SYNC/extern.json");
    offlineSettings.paths.ordersFile = missing("SYNC/Bestellungen.json");
    offlineSettings.paths.externLocalDir = util::pathToUtf8(localExtern);
    offlineSettings.bootstrapAdmin = "admin";

    app::AppContext ctx(offlineSettings, "techniker", offline / "work");
    ctx.initialize();
    SMOKE_CHECK(!ctx.syncAvailable());
    SMOKE_CHECK(ctx.hasAccess(users::groups::Techsupport)); // permissions from the local cache
    SMOKE_CHECK(ctx.deviceRegistry.find("0601-1234") != nullptr);
    SMOKE_CHECK(ctx.statusIndex().statusOf("0601-1234") == "extern");
    SMOKE_CHECK(ctx.deviceRegistry.find("0471-77") != nullptr);
    SMOKE_CHECK(ctx.statusIndex().statusOf("0471-77") == "geplant extern");
    {
      service::DeviceFilter all;
      all.showClosed = true; // offline Reader Control shows all local services
      SMOKE_CHECK(service::filterDevices(ctx.deviceRegistry, ctx.statusIndex(), all).size() == 2);
    }
    SMOKE_CHECK(app::ExternModule().available(ctx));
    SMOKE_CHECK(!app::OrdersModule().available(ctx));
    ctx.selectDevice("0601-1234");
    SMOKE_CHECK(ctx.activeRecord().status == "extern");
    SMOKE_CHECK(ctx.selectedService() == "Service_2026.12.24");
    const auto *device = ctx.activeDevice();
    SMOKE_CHECK(device != nullptr);
    if (device) {
      const auto found = ctx.services().findChecklist(*device, ctx.selectedService(), ctx.activeRecord());
      SMOKE_CHECK(found && found->parent_path() == copy / "Service_2026.12.24");
    }
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
