---
name: pruefen
description: ServiceTracker vollständig bauen und alle Tests ausführen (Unit-Tests, App-Compile-Check mit Warnungen als Fehler, Headless-UI-Smoke-Test) – vor jedem Commit.
---

# Prüfen vor dem Commit

## Linux / Agent-Umgebung

```sh
cmake --preset check
cmake --build --preset check
ctest --preset check
```

- `check` baut `st_core`, alle Unit-Tests, kompiliert jede App-Quelle gegen die echten
  ImGui-/cpp_app-Header (Warnungen = Fehler) und führt `st_app_smoke` aus.
- Fehlschlag im Smoke-Test: Ausgabe von `build/check/app/st_app_smoke` lesen
  (ImGui-Assertion, `ID conflict`, `SMOKE CHECK FAILED`, `[ERROR]`).
  `SMOKE_VERBOSE=1` listet die erreichten Fenster.
- Einzelne Unit-Tests: `build/check/tests/st_tests -tc="*name*"`, Logs mit `ST_TEST_LOG=1`.

## Windows

```bat
cmake --preset windows
cmake --build --preset windows-release
ctest --preset windows
```

## Danach

- `git status` prüfen: keine Build-Artefakte, kein `installer/setup.iss`.
- Doku aktualisiert? (`docs/module-referenz.md`, `docs/dateiformate.md`, `docs/konfiguration.md`)
