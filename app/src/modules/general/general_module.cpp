#include "modules/general/general_module.h"

#include "framework/app_context.h"
#include "framework/events.h"
#include "framework/platform.h"
#include "framework/widgets.h"

#include <imgui.h>

#include <algorithm>

namespace st::app {

void GeneralModule::init(AppContext &ctx) {
  ctx.ui.addMenu("Einstellungen", [this, &ctx] { drawMenu(ctx); });
}

bool GeneralModule::drawNotificationSettings(AppContext &ctx) {
  const auto channels = ctx.alerts.center().channels();
  if (channels.empty())
    return false; // the user receives no notifications
  auto &prefs = ctx.prefs();
  bool changed = false;
  ImGui::SeparatorText("Benachrichtigungen");
  changed |= ImGui::Checkbox("Windows-Benachrichtigungen", &prefs.desktopNotifications);
  ui::help("Ist ServiceTracker im Vordergrund, erscheint die Meldung nur im Programm.\n"
           "Ein Klick auf die Benachrichtigung öffnet ServiceTracker.");
  {
    ui::Disabled off(!prefs.desktopNotifications);
    changed |= ImGui::Checkbox("Benachrichtigung bleibt bis zum Schließen", &prefs.persistentNotifications);
    ui::help("Aus: Windows blendet die Benachrichtigung nach einigen Sekunden aus\n"
             "(sie bleibt im Benachrichtigungscenter).");
  }
  changed |= ImGui::Checkbox("Roter Punkt in der Taskleiste", &prefs.taskbarBadge);
  ui::help("Wird nur bei großen Taskleisten-Schaltflächen angezeigt (Windows-Einstellung).");
  for (const auto *channel : channels) {
    ui::ScopedId scope(channel->id.c_str());
    const bool muted = std::find(prefs.mutedAlerts.begin(), prefs.mutedAlerts.end(), channel->id) != prefs.mutedAlerts.end();
    bool enabled = !muted;
    if (ImGui::Checkbox(channel->label.c_str(), &enabled)) {
      if (enabled)
        std::erase(prefs.mutedAlerts, channel->id);
      else
        prefs.mutedAlerts.push_back(channel->id);
      changed = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Testen"))
      ctx.alerts.showTest(ctx, channel->id);
    ImGui::SetItemTooltip("Zeigt die Benachrichtigung mit dem aktuellen Stand an");
  }
  return changed;
}

void GeneralModule::drawMenu(AppContext &ctx) {
  auto &prefs = ctx.prefs();
  bool changed = false;
  ImGui::SeparatorText("Checkliste");
  changed |= ImGui::Checkbox("Nur Arbeitsgänge mit Berechtigungen anzeigen", &prefs.onlyPermittedSections);
  changed |= ImGui::Checkbox("Deutsches Zeitformat", &prefs.germanDates);
  if (ctx.isPoweruser()) {
    changed |= ImGui::Checkbox("Fertiggemeldet ignorieren", &prefs.ignoreFinished);
    ui::help("Fertig gemeldete Arbeitsgänge bleiben bearbeitbar.");
    changed |= ImGui::Checkbox("Berechtigungen ignorieren", &prefs.ignorePermissions);
    ui::help("Alle Arbeitsgänge sind bearbeitbar, unabhängig von der Abteilung.");
  }
  ImGui::SeparatorText("Listen");
  changed |= ImGui::Checkbox("Erwartete und erledigte Geräte anzeigen", &prefs.showClosedDevices);
  ui::help("Reader Control zeigt ohne Filter sonst nur Geräte, die gerade bearbeitet werden.");
  changed |= ImGui::Checkbox("Bestellungen als Popup öffnen", &prefs.ordersAsPopup);
  if (!prefs.recentDevices.empty() && ImGui::Button("Zuletzt verwendete Geräte leeren")) {
    prefs.recentDevices.clear();
    changed = true;
  }
  changed |= drawNotificationSettings(ctx);
  if (changed) {
    prefs.restrictTo(ctx.isPoweruser());
    ctx.savePrefs();
    ctx.events.publish(events::PreferencesChanged{});
  }

  ImGui::SeparatorText("Info");
  ImGui::Text("Benutzer: %s%s", ctx.user().c_str(), ctx.isPoweruser() ? " (Poweruser)" : "");
  ImGui::Text("Version: %s", APP_VERSION);
  if (ImGui::BeginMenu("Pfade")) {
    const auto &p = ctx.paths();
    const auto row = [](const char *label, const std::string &path) {
      ImGui::Text("%s:", label);
      ImGui::SameLine();
      if (ImGui::SmallButton(path.c_str()))
        platform::openPath(util::pathFromUtf8(path));
      ImGui::SetItemTooltip("Öffnen");
    };
    row("Geräte", p.devicesRoot);
    row("Sync", p.syncDir);
    row("Configs", p.configsDir);
    row("Benutzer", p.usersFile);
    row("Bestellungen", p.ordersFile);
    row("Extern (lokal)", p.externLocalDir);
    ImGui::Separator();
    ui::hintText("Pfade werden in ServiceTracker.json neben der .exe konfiguriert (siehe Doku).");
    ImGui::EndMenu();
  }
  if (ImGui::MenuItem("Log-Datei öffnen"))
    platform::openPath(ctx.workingDir() / "run.log");
  if (ImGui::BeginMenu ("Anleitungen")) {
		if (ImGui::MenuItem("Anleitung öffnen"))
			platform::openPath(platform::executableDir() / "Anleitung.pdf");
		if (ImGui::MenuItem("Anleitung-Extern öffnen"))
			platform::openPath(platform::executableDir() / "Anleitung-Extern.pdf");
    ImGui::EndMenu();
  }
}

} // namespace st::app
