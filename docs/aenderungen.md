# Änderungsprotokoll

Alle Änderungen am Programm, neueste zuerst. Jede Änderung wird hier mit einem kurzen Satz
für Anwender und einem Verweis auf die ausführliche Doku eingetragen. Der Vergleich mit
Version 1 steht in [migration.md](migration.md).

## 2026-10-22 (Version 2.0.2)

- **Offline beim Kunden** ([module/extern.md](module/extern.md#offline-beim-kunden)):
  Berechtigungen werden lokal zwischengespeichert, Reader Control zeigt offline die Geräte
  mit lokaler Kopie, das Fenster „Externe Services“ und die Checkliste der lokalen Kopie
  sind verfügbar. Hinweis, sobald das Netzlaufwerk wieder erreichbar ist.
- **Status-Benachrichtigungen** ([module/statusalerts.md](module/statusalerts.md)):
  Windows-Benachrichtigung, sobald ein Gerät mit einem bestimmten Status erscheint –
  mitgeliefert: „versendet“, „extern abgeschlossen“, „Rechnung versendet“ → `TECHSUPPORT`,
  „im Versand“, „Rechnung erstellen“ → `SALES`. Poweruser pflegen die Regeln unter
  Configs → Status-Benachrichtigungen (gemeinsame Datei `configs/statusAlerts.json`).
- **Bestellungen**: Kunde mit Autovervollständigung (inkl. Kunden bisheriger Bestellungen),
  erweiterte Liste bekannter Kunden.
- **Checkliste**: Arbeitsgänge sind beim Öffnen zugeklappt.

## 2026-10-16

- **Externe Services – neuer Ablauf** ([module/extern.md](module/extern.md)):
  - Im Haus wird die Checkliste der lokalen Kopie automatisch in beide Richtungen mit der
    Checkliste im Geräteordner abgeglichen (inkl. Anhängen); Kollegen können mitarbeiten.
  - „Service abschließen“ heißt jetzt **„Extern abschließen“**: letzter Abgleich, der lokale
    Geräteordner wird über den Original-Geräteordner kopiert, lokale Kopie gelöscht, neuer
    Status **„extern abgeschlossen“**.
  - Der Service wird danach in „Service Control“ mit **„Service fertigmelden“** abgeschlossen:
    Service Record und Eintrag in der neuen Liste **`History_extern.json`** (nicht mehr in
    `History.json`). Die Historie kann `History_extern.json` als Quelle anzeigen.
  - Die Section Action `update_status_rinfo` ändert den Status externer Services nicht mehr.
- **Configs**: eigene **Vorlagen für externe Services** (z. B. andere Checkliste),
  Schlüssel `filesExtern` in `configs.json` ([module/configs.md](module/configs.md)).
- **Bestellungen**: `SALES` darf Bestellungen anlegen ([module/orders.md](module/orders.md)).
- **Checkliste**: fertiggemeldete Arbeitsgänge werden nicht mehr grün eingefärbt.
- **Konfiguration**: `deploy/ServiceTracker.json` wird ausgeliefert; Standardwerte jetzt
  `Bestellungen.json` und Update-Quelle `BigAgg/ServiceTracker`.
- **Doku**: jedes Modul in einer eigenen Datei unter [module/](module/), dieses
  Änderungsprotokoll.

## 2026-10-15

- **Gleichzeitiges Arbeiten**: jede Speicherung bekommt eine neuere Änderungszeit als die
  ersetzte Datei – Änderungen kurz nacheinander gehen nicht mehr unbemerkt verloren
  (betraf Checklisten, Sync.json, Bestellungen und alle anderen Listen).
- **Benachrichtigungen** ([benachrichtigungen.md](benachrichtigungen.md)):
  - `SALES`: Windows-Benachrichtigung bei versandbereiten Bestellungen, roter Punkt in der
    Taskleiste.
  - `TECHSUPPORT`: „Neuer Reader im Wareneingang: *Seriennummer*“, Klick wählt das Gerät aus.
  - Benachrichtigungen bleiben stehen, bis sie angeklickt oder geschlossen werden
    (abschaltbar); ein neuer Stand ersetzt die alte Meldung.
  - Einstellungen → Benachrichtigungen: an/aus, roter Punkt, je Art einzeln + „Testen“.
- **Bestellungen**: abwechselnd hinterlegte Einträge mit Trennlinien (ohne Popup-Modus).
- **Checkliste**: Datumsfeld zeigt den Aufgabennamen ohne ID-Suffix.
