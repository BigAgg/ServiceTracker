---
name: neues-modul
description: Neues Feature/Fenster als Modul in ServiceTracker hinzufügen (Core-Logik mit Tests, Modul-Klasse, Registrierung, Smoke-Test, Doku).
---

# Neues Modul hinzufügen

Ausführliche Erklärung mit Beispielcode: `docs/module.md`. Kurzfassung:

1. **Fachlogik in `core`**
   - Header `core/include/st/<bereich>/<datei>.h`, Quelle `core/src/<bereich>/<datei>.cpp`
     (werden per Glob gefunden, kein CMake-Eintrag nötig).
   - Dateien nur über `util::json::load/save`, `util::readFile`, `util::writeFileAtomic`.
   - Fehler als `util::Result`/`util::Status` mit deutscher Meldung.
   - Gemeinsam genutzte Listen: vor dem Schreiben neu lesen, nur eigenen Eintrag ändern,
     unbekannte JSON-Felder erhalten (Vorbild `service/records.cpp`).
2. **Tests** in `tests/domain/test_<name>.cpp` mit `test::TempDir` und `test::TestSession`:
   Round-Trip, Fehlerfälle, zwei Instanzen auf derselben Datei.
3. **Modul** `app/src/modules/<name>/<name>_module.{h,cpp}`:
   - von `Module` ableiten, `name()`, `available()` (Rechte/Pfade), `init()`.
   - Fenster mit `ctx.ui.addWindow("Deutscher Name", ...)` – Name ist dauerhaft (Layout!).
   - Polling in `update()` mit `PollTimer`, keine ImGui-Aufrufe dort.
   - Dialoge aus Menüs in `drawOverlay()`.
   - UI-Bausteine aus `framework/widgets.h` verwenden, IDs mit `ui::ScopedId`.
   - Eingabefelder brauchen über Frames lebende Puffer (Member), nie temporäre Kopien.
   - Soll eine Abteilung benachrichtigt werden: Kanal mit `ctx.alerts.addChannel` in `init()`,
     offene Einträge mit `ctx.alerts.report` melden (`docs/benachrichtigungen.md`).
   - Auswahl über `ctx.selectDevice/selectService`, Ergebnisse von Service-Aktionen über
     `ctx.applyRecord`, Fehler über `ctx.notify.check`.
4. **Registrieren** in `app/src/modules/register_modules.cpp` (Reihenfolge = Init-Reihenfolge).
5. **Smoke-Test** `app/smoke/smoke_main.cpp`: Modul hinzufügen (`modules.add<...>()`),
   Fensternamen in die Layout-Liste aufnehmen, relevanten Workflow durchspielen.
6. **Prüfen** mit Skill `pruefen`.
7. **Doku**: `docs/module/<name>.md` (Nutzung, Dateien, Code) + Zeile in
   `docs/module-referenz.md`, Eintrag in `docs/aenderungen.md`, ggf. `docs/dateiformate.md`,
   `docs/konfiguration.md`.
