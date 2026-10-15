# AGENTS.md – Leitfaden für die Entwicklung (Menschen und KI-Agenten)

ServiceTracker 2: C++23, CMake, Dear ImGui über das Framework
[cpp_app](https://github.com/BigAgg/cpp_app). Fachlogik in `core/` (plattformunabhängig,
getestet), GUI in `app/` (Windows). Einstieg: [README.md](README.md),
[docs/architektur.md](docs/architektur.md).

## Arbeitsweise

- **Selbstständig arbeiten**, nur bei echten Unklarheiten nachfragen.
- **Produktionsqualität**: keine Abkürzungen, keine auskommentierten Altlasten, keine TODO-Hacks.
- **Tests zuerst mitdenken**: jede Logik-Änderung in `core/` braucht Unit-Tests; jede neue/
  geänderte UI braucht einen grünen Smoke-Test. Vor jedem Commit:
  ```sh
  cmake --preset check && cmake --build --preset check && ctest --preset check
  ```
  (Windows: Presets `windows` / `windows-release` / `windows`.)
- **Kompatibilität ist Pflicht**: Dateien von Version 1 müssen lesbar bleiben, geschriebene
  Dateien müssen von Version 1 lesbar sein. Siehe Skill `dateiformat-aendern`.
- **Kleine Komfort-Features** (Tooltips, Kontextmenüs, Tastenkürzel, sinnvolle Vorbelegung)
  sind ausdrücklich erwünscht.

## Sprache und Stil

- Code, Bezeichner, Code-Kommentare: **Englisch**. UI-Texte, Fehlermeldungen, Dokumentation: **Deutsch**.
- Formatierung: `.clang-format` (2 Leerzeichen, geschweifte Klammer in derselben Zeile:
  `void f() {`).
- Vor Klassen und Funktionen in Headern eine kurze `///`-Doku (Zweck, Besonderheiten), im
  Code nur kommentieren, wenn das *Warum* nicht offensichtlich ist.
- Namen: Klassen `PascalCase`, Funktionen/Variablen `camelCase`, Member `mName`,
  Konstanten `kName`, Namespaces `st::<bereich>`.
- Fehlerbehandlung in `core`: `util::Result<T>`/`util::Status` (std::expected), keine
  Exceptions nach außen. In der App: `ctx.notify.check(result, "Kontext")`.

## Struktur und Regeln

| Bereich | Regel |
|---|---|
| `core/include/st/<bereich>/`, `core/src/<bereich>/` | keine UI-/Windows-Abhängigkeiten; Dateizugriffe nur über `util::fs`/`util::json` (atomar, Retry) |
| `app/src/framework/` | featureunabhängige Infrastruktur (Module, UiHost, EventBus, Widgets) |
| `app/src/modules/<name>/` | ein Feature = ein Modul; registriert in `modules/register_modules.cpp` |
| `app/smoke/` | Headless-UI-Test; neue Fenster dort ergänzen |
| `tests/` | doctest, Hilfen in `tests/test_helpers.h`, Testdaten in `tests/data/` |
| `docs/` | deutsch; bei neuen Modulen/Dateien/Einstellungen aktualisieren |

Wichtige Invarianten:
- **Fensternamen nie ändern** (Docking-Layout `imgui.ini` der Benutzer hängt daran).
- **Namen von Expressions und Section Actions nie ändern** (stehen in Checklisten-Dateien).
- **Status-Texte** (`service::status`) und **Flag-Bitwerte** (`devices::ConfigFlag`) nie ändern.
- **Kanal-IDs der Benachrichtigungen** (`alerts::Channel::id`) nie ändern (stehen in `preferences.json`).
- Zeitstempel immer über `util::Session` (testbar), Format `JJJJ-MM-TT hh:mm:ss`.
- Pfade intern UTF-8, Umwandlung nur mit `util::pathFromUtf8`/`util::pathToUtf8`.
- Netzlaufwerk nicht jedes Frame lesen – `PollTimer` in `update()`.

## Skills

Schritt-für-Schritt-Anleitungen für wiederkehrende Aufgaben liegen in `.claude/skills/`:

| Skill | Wann |
|---|---|
| `neues-modul` | neues Feature/Fenster hinzufügen |
| `dateiformat-aendern` | ein gespeichertes Format erweitern oder neu einführen |
| `pruefen` | Build + alle Tests + Smoke-Test vor dem Commit |
| `release` | neue Version und Installer erstellen |

## Git

- Repository: https://github.com/BigAgg/ServiceTracker (Branch `main`).
- Kleine, thematisch saubere Commits; Commit-Messages auf Deutsch, Imperativ oder Ergebnis
  („Bestellungen: Nächster-Schritt-Button“).
- Keine Build-Ordner, `installer/setup.iss` oder lokale Einstellungen committen (`.gitignore`).
