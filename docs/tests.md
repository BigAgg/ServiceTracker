# Tests

## Ausführen

```sh
ctest --preset core      # Core-Unit-Tests (jede Plattform)
ctest --preset check     # + Headless-UI-Smoke-Test (Linux)
ctest --preset windows   # Windows (nach dem Build)
```

Einzelne Tests direkt: `build/core/tests/st_tests -tc="*merge*"` (doctest-Filter),
Log-Ausgaben sichtbar machen mit Umgebungsvariable `ST_TEST_LOG=1`.

## Aufbau

| Ebene | Ort | Was wird geprüft |
|---|---|---|
| Unit-Tests | `tests/util`, `tests/checklist`, `tests/domain` | gesamte Fachlogik in `st_core`: Formate, Merge, Workflows, Stores, Exporte |
| Testdaten | `tests/data` | z.B. `legacy_checklist.txt` exakt im Format von Version 1 |
| App-Compile-Check | Preset `check` | alle App-Quellen gegen die echten ImGui-/cpp_app-Header, Warnungen = Fehler |
| UI-Smoke-Test | `app/smoke` | alle Module mit echten Daten initialisieren, Workflows durchspielen, jedes Fenster rendern, Maus über den ganzen Bildschirm bewegen; ImGui-Assertions, ID-Konflikte und jede Fehlermeldung des Programms lassen den Test scheitern |
| CI | `.github/workflows/ci.yml` | Linux (`check`) und Windows (App + Tests) bei jedem Push |

## Neue Tests schreiben

```cpp
#include <doctest/doctest.h>
#include "st/loans/loan_store.h"
#include "test_helpers.h"

using namespace st;

TEST_CASE("loans: round trip") {
  test::TempDir dir;                       // wird automatisch gelöscht
  test::TestSession ts;                    // Benutzer "tester", feste Uhr (15.03.2026 10:00)
  loans::LoanStore store(dir / "loans.json");
  REQUIRE(store.upsert({"0601-1", "Uni", "2026.04.01"}));
  auto all = store.loadAll();
  REQUIRE(all.has_value());
  CHECK(all->size() == 1);
}
```

Hilfen in `tests/test_helpers.h`: `TempDir`, `TestSession` (mit `advance(sekunden)`),
`readText`, `writeText`, `dataFile(name)`.

Richtlinien:
- Jede Änderung an einem Dateiformat braucht einen Test mit einer Datei im **alten** Format
  (Kompatibilität) und einen Round-Trip-Test.
- Gleichzeitiges Arbeiten mit zwei Store-/Dokument-Instanzen auf derselben Datei testen.
- Zeit nie über die Systemuhr, sondern über `TestSession`.
- Neue Fenster in `app/smoke/smoke_main.cpp` aufnehmen.
