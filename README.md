# ServiceTracker 2

ServiceTracker verwaltet Services von Geräten (Readern) abteilungsübergreifend:
Wareneingang, Checklisten, die mehrere Mitarbeiter gleichzeitig bearbeiten, Service-Records,
externe Services beim Kunden, Ersatzteil-Bestellungen und Auswertungen nach Excel.

Version 2 ist eine komplette Neuentwicklung mit Fokus auf **Bedienung, Stabilität und
Erweiterbarkeit durch Module**. Alle bestehenden Dateien (Checklisten `.txt`/`.json`,
`Sync.json`, `History.json`, `users.bin`, `configs.json`, Bestellungen, Änderungswünsche)
werden unverändert weiterverwendet – alte und neue Version können parallel laufen.

## Schnellstart (Windows)

```bat
cmake --preset windows
cmake --build --preset windows-release
ctest --preset windows
build\windows\app\Release\ServiceTracker.exe
```

Voraussetzungen und Details: [docs/build.md](docs/build.md).

## Projektstruktur

| Ordner | Inhalt |
|---|---|
| `core/` | Bibliothek `st_core`: komplette Fachlogik ohne UI (Dateiformate, Workflows, Merge). Plattformunabhängig, vollständig getestet. |
| `app/` | GUI auf Basis von [cpp_app](https://github.com/BigAgg/cpp_app) (raylib + Dear ImGui Docking). Modul-System in `app/src/framework`, Module in `app/src/modules`. |
| `app/smoke/` | Headless-UI-Test: rendert alle Fenster ohne Grafikkarte und spielt die Workflows durch. |
| `tests/` | Unit-Tests (doctest) für `st_core`, Testdaten in `tests/data`. |
| `docs/` | Dokumentation (deutsch). |
| `deploy/`, `installer/` | Release-Skript, Standard-Layout, Inno-Setup-Vorlage. |
| `.claude/skills/` | Arbeitsanleitungen für KI-Agenten (siehe [AGENTS.md](AGENTS.md)). |

## Dokumentation

- [Build & Entwicklungsumgebung](docs/build.md)
- [Architektur](docs/architektur.md)
- [Module: Nutzung & neue Module einbinden](docs/module.md)
- [Modul-Referenz (alle Fenster und Funktionen)](docs/module-referenz.md), je Modul: [docs/module/](docs/module/)
- [Dateiformate & Kompatibilität](docs/dateiformate.md)
- [Konfiguration (ServiceTracker.json)](docs/konfiguration.md)
- [Benachrichtigungen](docs/benachrichtigungen.md)
- [Tests](docs/tests.md)
- [Release & Installer](docs/release.md)
- [Änderungsprotokoll](docs/aenderungen.md)
- [Änderungen gegenüber Version 1](docs/migration.md)

## Lizenz

MIT, siehe [LICENSE](LICENSE).
