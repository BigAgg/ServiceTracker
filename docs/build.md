# Build & Entwicklungsumgebung

## Voraussetzungen

| Werkzeug | Version | Wofür |
|---|---|---|
| CMake | ≥ 3.25 (CMake 4 wird unterstützt) | Build-System, Presets |
| Visual Studio | 2022 oder 2026 mit „Desktopentwicklung mit C++“ | Compiler (MSVC), C++23 |
| Git | aktuell | CMake lädt Abhängigkeiten per Git |
| Python 3 | optional | Release-Skript `deploy/draft_new_release.py` |
| Inno Setup 6 | optional | Installer bauen |

Alle Bibliotheken werden beim ersten Konfigurieren automatisch geladen (FetchContent):
cpp_app (mit raylib, Dear ImGui Docking, rlImGui, NFD, libcurl), nlohmann/json, doctest,
zlib und libxlsxwriter. Es ist **keine** manuelle Installation und kein vcpkg nötig.
Der erste Build dauert deshalb einige Minuten.

## Windows: App + Tests

In einer *Developer PowerShell* oder normalen Konsole im Projektordner:

```bat
cmake --preset windows
cmake --build --preset windows-release      :: oder windows-debug
ctest --preset windows
```

Ergebnis: `build\windows\app\Release\ServiceTracker.exe` – alle Ressourcen (Schrift, Icons,
Anleitung) werden beim Build daneben kopiert, das Programm ist direkt startbar.

In Visual Studio: *Datei → Öffnen → Ordner* auf das Projekt; die Presets erscheinen in der
Konfigurationsauswahl. Alternativ `build\windows\ServiceTracker.sln` öffnen (Startprojekt ist
`ServiceTracker`, Arbeitsverzeichnis ist der Ordner der .exe).

## Linux / ohne GUI: Core + Tests

Die Fachlogik (`core/`) und alle Unit-Tests laufen auf jeder Plattform:

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
```

Das Preset `check` baut zusätzlich alle App-Quellen gegen die echten ImGui-/cpp_app-Header
und führt den Headless-UI-Smoke-Test aus (so läuft auch die Linux-CI):

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check
```

## CMake-Optionen

| Option | Standard | Bedeutung |
|---|---|---|
| `ST_BUILD_APP` | ON unter Windows | GUI bauen (benötigt cpp_app, nur Windows) |
| `ST_BUILD_TESTS` | ON | Unit-Tests bauen |
| `ST_APP_COMPILE_CHECK` | OFF | App-Quellen ohne Linken kompilieren + UI-Smoke-Test (Linux) |
| `ST_WITH_XLSX` | ON | Excel-Export (libxlsxwriter) |
| `ST_WARNINGS_AS_ERRORS` | OFF | Warnungen als Fehler (CI-Preset `check`: ON) |
| `ST_CPP_APP_TAG` | `main` | Git-Tag/Branch von cpp_app |

## Presets

| Configure-Preset | Inhalt |
|---|---|
| `windows` | App + Tests, Visual Studio Generator (x64) |
| `core` | Core + Tests, Ninja, Debug |
| `check` | wie `core` + App-Compile-Check + Smoke-Test, Warnungen = Fehler |

## Häufige Probleme

- **„Compatibility with CMake < 3.5 has been removed“** – tritt mit CMake 4 bei alten
  Abhängigkeiten auf. Die Presets setzen `CMAKE_POLICY_VERSION_MINIMUM=3.5`; bei Builds ohne
  Preset `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` angeben.
- **Download-Fehler beim Konfigurieren** – Proxy/Internet prüfen, dann `build/` löschen und
  neu konfigurieren.
- **Linker-Fehler zur C-Runtime** – alle Ziele verwenden die statische Runtime (`/MT`).
  Eigene Bibliotheken ebenfalls mit `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` bauen.
- **Umlaute falsch** – Quelltexte sind UTF-8, MSVC bekommt `/utf-8`. Dateien immer als UTF-8
  (ohne BOM) speichern.
