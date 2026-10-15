# Module: Nutzung und neue Module einbinden

Jedes Feature des Programms ist ein **Modul** (`app/src/modules/<name>/`). Ein Modul
registriert seine Fenster, Menüs, Expressions und Ereignis-Handler selbst und hält seinen
UI-Zustand als Member. Module kennen sich nicht gegenseitig – sie teilen sich den
`AppContext` und kommunizieren über Ereignisse.

Eine Übersicht aller vorhandenen Module mit ihren Fenstern und Funktionen steht in
[module-referenz.md](module-referenz.md).

## Lebenszyklus

```
main()
 ├─ AppContext ctx(settings, user, workingDir)
 ├─ ctx.initialize()                     Benutzer, Configs, Geräte, Sync.json laden
 ├─ registerModules(registry)            modules/register_modules.cpp
 ├─ registry.initAll(ctx)                für jedes Modul: available()? → init()
 ├─ Schleife pro Frame
 │    ctx.update()                       Sync.json pollen
 │    registry.updateAll(ctx)            Module: update()      (keine ImGui-Aufrufe!)
 │    app::BeginDrawing()                cpp_app zeichnet alle registrierten Fenster
 │    registry.drawOverlays(ctx)         Module: drawOverlay() (Dialoge aus Menüs)
 │    ctx.notify.draw()                  Toasts + Statuszeile
 │    app::EndDrawing()
 └─ registry.shutdownAll(ctx)            Module: shutdown() in umgekehrter Reihenfolge
```

| Methode | Pflicht | Zweck |
|---|---|---|
| `name()` | ja | technischer Name (Log) |
| `available(ctx)` | nein | Modul für diesen Benutzer/diese Installation aktiv? (Rechte, erreichbare Pfade) |
| `init(ctx)` | ja | Fenster/Menüs registrieren, Expressions/Actions anmelden, Events abonnieren, Daten laden |
| `update(ctx)` | nein | jedes Frame **vor** dem Zeichnen: Dateien pollen, Autosave, Timer |
| `drawOverlay(ctx)` | nein | jedes Frame **im** ImGui-Frame: freie Dialogfenster (z.B. aus einem Menü geöffnet) |
| `shutdown(ctx)` | nein | Zustand sichern |

## Neues Modul in 5 Schritten

Beispiel: ein Fenster „Leihgeräte“, das eine Liste aus einer JSON-Datei anzeigt.

### 1. Fachlogik in `core` (mit Tests)

Alles, was Dateien liest oder schreibt, gehört in `core/` – dort ist es testbar:

```cpp
// core/include/st/loans/loan_store.h
namespace st::loans {

struct Loan {
  std::string device;
  std::string customer;
  std::string until; // "YYYY.MM.DD"
};

/// Leihgeräte-Liste (loans.json) auf dem Sync-Laufwerk.
class LoanStore {
public:
  explicit LoanStore(util::fs::path file) : mFile(std::move(file)) {}
  util::Result<std::vector<Loan>> loadAll() const;
  util::Status upsert(const Loan &loan) const;

private:
  util::fs::path mFile;
};

} // namespace st::loans
```

- Lesen/Schreiben immer über `util::json::load/save` bzw. `util::readFile/writeFileAtomic`
  (atomar, mit Retry).
- Fehler als `util::Result`/`util::Status` zurückgeben, deutsche Meldung.
- Test in `tests/domain/test_loans.cpp` anlegen (siehe [tests.md](tests.md)). Neue Dateien
  in `core/src` und `tests/` werden automatisch gefunden (CMake-Glob).

### 2. Modul-Klasse

```cpp
// app/src/modules/loans/loans_module.h
#pragma once
#include "framework/app_context.h"
#include "framework/module.h"
#include <st/loans/loan_store.h>

namespace st::app {

/// Leihgeräte. Fenster: "Leihgeräte".
class LoansModule final : public Module {
public:
  std::string_view name() const override { return "loans"; }
  bool available(const AppContext &ctx) const override;
  void init(AppContext &ctx) override;
  void update(AppContext &ctx) override;

private:
  void drawWindow(AppContext &ctx);
  void reload(AppContext &ctx);

  loans::LoanStore mStore{{}};
  std::vector<loans::Loan> mLoans;
  std::int64_t mFileTime = -1;
  PollTimer mPoll;
};

} // namespace st::app
```

### 3. Implementierung

```cpp
// app/src/modules/loans/loans_module.cpp
#include "modules/loans/loans_module.h"
#include "framework/events.h"
#include "framework/widgets.h"
#include <st/users/user_store.h>
#include <imgui.h>

namespace st::app {

bool LoansModule::available(const AppContext &ctx) const {
  return ctx.syncAvailable() && ctx.hasAccess(users::groups::Techsupport);
}

void LoansModule::init(AppContext &ctx) {
  mStore = loans::LoanStore(util::pathFromUtf8(ctx.paths().syncDir) / "loans.json");
  // Fenstername = Eintrag im Docking-Layout (imgui.ini). Nie umbenennen!
  ctx.ui.addWindow("Leihgeräte", [this, &ctx] { drawWindow(ctx); });
  // auf andere Module reagieren
  ctx.events.subscribe<events::DeviceSelected>([this, &ctx](const events::DeviceSelected &) { reload(ctx); });
  reload(ctx);
}

void LoansModule::update(AppContext &ctx) {
  // Änderungen anderer Benutzer erkennen – gedrosselt, nicht jedes Frame
  if (mPoll.due() && util::lastWriteTime(util::pathFromUtf8(ctx.paths().syncDir) / "loans.json") != mFileTime)
    reload(ctx);
}

void LoansModule::reload(AppContext &ctx) {
  auto loaded = mStore.loadAll();
  if (ctx.notify.check(loaded, "Leihgeräte laden"))
    mLoans = std::move(*loaded);
}

void LoansModule::drawWindow(AppContext &ctx) {
  for (const auto &loan : mLoans) {
    ui::ScopedId id(loan.device.c_str());
    if (ImGui::Selectable(loan.device.c_str(), loan.device == ctx.activeDeviceName()))
      ctx.selectDevice(loan.device); // Auswahl gilt für alle Module
  }
}

} // namespace st::app
```

### 4. Registrieren

In `app/src/modules/register_modules.cpp` eine Zeile ergänzen. Die Reihenfolge ist die
Initialisierungsreihenfolge – Module, die Expressions/Actions bereitstellen, vor denen, die
sie benutzen:

```cpp
#include "modules/loans/loans_module.h"
...
registry.add<LoansModule>();
```

### 5. Prüfen

- `ctest --preset check` (Linux) bzw. `ctest --preset windows` – Unit-Tests **und** der
  UI-Smoke-Test müssen grün sein.
- Für neue Fenster das Modul im Smoke-Test (`app/smoke/smoke_main.cpp`) ergänzen und den
  Fensternamen in die Layout-Liste aufnehmen, damit der Maus-Sweep es abdeckt.
- Dokumentation: Eintrag in [module-referenz.md](module-referenz.md), neue Dateien in
  [dateiformate.md](dateiformate.md).

## Bausteine im Detail

### Fenster, Menüs, Popups (`ctx.ui`)

```cpp
ctx.ui.addWindow("Name", [this, &ctx] { draw(ctx); }, /*openByDefault*/ true, ImGuiWindowFlags_HorizontalScrollbar);
ctx.ui.addMenu("Menüname", [this] { if (ImGui::MenuItem("Dialog öffnen...")) mOpen = true; });
ctx.ui.setAttention("Name", true);   // Punkt im Fenster-Tab (z.B. "ungespeichert")
```

- Fenster sind andockbar; das Layout speichert cpp_app in `imgui.ini` **unter dem
  Fensternamen**. Bestehende Namen deshalb nie ändern (sonst verlieren Benutzer ihr Layout).
- Menüs schließen sich beim Klick. Dialoge daher nicht im Menü zeichnen, sondern ein Flag
  setzen und das Fenster in `drawOverlay()` mit `ImGui::Begin(..., &mOpen, ImGuiWindowFlags_NoDocking)` zeichnen
  (Beispiel: `modules/users`).
- Popups, die aus einem Fenster geöffnet werden, im selben Fenster (gleiche ID-Ebene) mit
  `ImGui::BeginPopupModal` zeichnen.
- Es stehen 128 Callback-Plätze zur Verfügung (`UiHost::freeSlots()`).

### Gemeinsamer Zustand (`AppContext`)

| Zugriff | Bedeutung |
|---|---|
| `ctx.settings()`, `ctx.paths()` | Konfiguration ([konfiguration.md](konfiguration.md)) |
| `ctx.user()`, `ctx.session()` | Benutzer, Zeitstempel (`session().timestamp()`, `.date()`) |
| `ctx.isPoweruser()`, `ctx.hasAccess(group)`, `ctx.hasAnyAccess(groups)` | Rechte |
| `ctx.prefs()`, `ctx.savePrefs()` | persönliche Einstellungen |
| `ctx.deviceRegistry`, `ctx.configStore`, `ctx.userStore` | Daten |
| `ctx.services()` | `ServiceManager` (alle Service-Workflows) |
| `ctx.activeDevice()`, `ctx.activeRecord()`, `ctx.selectedService()` | aktuelle Auswahl |
| `ctx.selectDevice(name)`, `ctx.selectService(name)` | Auswahl ändern (sendet Ereignisse) |
| `ctx.applyRecord(record)` | Ergebnis einer Service-Aktion übernehmen |
| `ctx.checklist()` | geöffnete Checkliste (`doc`, `dirty`, `markDirty()`) |
| `ctx.statusIndex()` | Status aller Geräte aus `Sync.json` |
| `ctx.notify` | Rückmeldungen: `info/success/warning/error`, `check(result, "Kontext")` |
| `ctx.alerts` | Windows-Benachrichtigungen + roter Punkt: `addChannel`, `report`, `markSeen` ([benachrichtigungen.md](benachrichtigungen.md)) |
| `ctx.displayTime(ts)` | Zeitstempel im gewählten Format |

### Ereignisse (`ctx.events`)

Definiert in `app/src/framework/events.h`:

| Ereignis | Wann |
|---|---|
| `DeviceSelected` | aktives Gerät gewechselt |
| `ServiceSelected` | Service-Ordner gewählt (auch nach Gerätewechsel) |
| `RecordsChanged` | `Sync.json` geändert (eigene Aktion oder anderer Benutzer) |
| `DevicesRescanned` | Geräteordner neu eingelesen |
| `ChecklistOpened` / `ChecklistSaved` / `ChecklistClosed` | Checklisten-Sitzung |
| `SaveChecklistRequest` | Checkliste sofort speichern lassen |
| `PreferencesChanged` | persönliche Einstellungen geändert |

Neues Ereignis = neues `struct` in `events.h`. Handler laufen synchron in `publish()`.

### Checklisten erweitern

**Expressions** füllen Aufgaben beim Fertigmelden automatisch aus. Im Checklisten-Editor
erscheinen alle registrierten Namen mit Beschreibung:

```cpp
ctx.expressions.add("@loan.customer", [&ctx](checklist::Task &task) {
  task.value = checklist::Text{findCustomer(ctx.activeDeviceName())};
}, "Kunde des Leihgeräts");
```

**Section Actions** laufen nach „Arbeitsgang fertigmelden“ (Feld *Post-Action* im Editor,
Argumente in *postActionArgs*):

```cpp
ctx.sectionActions.add("notify_sales", [&ctx](checklist::SectionActionContext &a) {
  ctx.notify.info("Vertrieb informieren: " + a.section.postActionArgs);
}, "Erinnert an die Information des Vertriebs");
```

Vorhandene Namen niemals umbenennen – sie stehen in bestehenden Checklisten-Dateien.

### UI-Bausteine (`framework/widgets.h`)

`ui::Disabled`, `ui::ScopedId`, `ui::searchField` (Strg+F, Esc leert), `ui::confirmButton`,
`ui::statusBadge`/`statusColor`, `ui::inputMultiline` (wächst mit), `ui::todayButton`,
`ui::help`, `ui::errorText`, `ui::hintText`, `ui::copyContextMenu`, `ui::listBox`.
Neue, mehrfach genutzte Bausteine dort ergänzen statt in Modulen zu kopieren.

### Regeln für Module

1. Keine Dateizugriffe direkt im Modul – immer über Core-Klassen.
2. Fehler nie verschlucken: `ctx.notify.check(result, "Kontext")`.
3. Netzlaufwerk nicht jedes Frame lesen: `PollTimer` verwenden.
4. Texte im UI deutsch, Code/Bezeichner englisch.
5. Eingabefelder brauchen einen Puffer, der über Frames lebt (Member oder Daten des
   Dokuments) – nie eine temporäre Kopie an `ImGui::InputText` übergeben.
6. Bei Listen IDs mit `ui::ScopedId` eindeutig machen (der Smoke-Test meldet Konflikte).
