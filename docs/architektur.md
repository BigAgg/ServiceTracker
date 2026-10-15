# Architektur

## Überblick

```
┌──────────────────────────── app (ServiceTracker.exe) ─────────────────────────────┐
│  main.cpp ── AppContext ── ModuleRegistry ── Module (je Feature)                  │
│                 │  UiHost (Fenster/Menüs) · EventBus · Notifications · Preferences│
│                 │  Alerts (Windows-Benachrichtigungen, roter Punkt Taskleiste)    │
│                 │  aktive Auswahl (Gerät, Service) · geöffnete Checkliste         │
├─────────────────┴──────────────────── st_core ────────────────────────────────────┤
│ util      Strings, Zeit, Dateisystem (atomar + Retry), JSON, Logging, Session     │
│ checklist Modell, TXT/JSON-Format, Merge, Dokument, Expressions/Actions, Export   │
│ devices   Gerät, Registry (Ordner-Scan), configs.json, EEPROM/Info-Parser         │
│ service   Sync.json/extern.json, History, ServiceManager (Workflows), Filter      │
│ orders    Bestellungen                users  users.bin + Berechtigungen           │
│ changes   Änderungswünsche            config ServiceTracker.json, Preferences     │
│ alerts    Benachrichtigungen: Kanäle, Neu-Erkennung, Stummschalten                │
│ exporting Excel (libxlsxwriter), Berichte                                         │
└───────────────────────────────────────────────────────────────────────────────────┘
```

**Regel:** Alles, was Dateien liest/schreibt oder Entscheidungen trifft, liegt in `core`
und ist unit-getestet. Die App zeichnet nur und ruft Core-Funktionen auf. Dadurch ist die
Logik ohne GUI testbar und Fehler in Dateiformaten fallen in der CI auf.

## Schichten

### core (`st_core`)

- Keine Abhängigkeit zu ImGui, raylib oder Windows-APIs.
- Fehler werden als `std::expected` zurückgegeben (`util::Result<T>`, `util::Status`),
  nicht geworfen. Meldungen sind deutsch, weil sie direkt im UI erscheinen.
- Zeit und Benutzer kommen über `util::Session` (Benutzername + `Clock`). Tests verwenden
  eine `FixedClock` und sind dadurch deterministisch.
- Pfade sind intern UTF-8-Strings; Umwandlung nur über `util::pathFromUtf8` /
  `util::pathToUtf8` (korrekte Umlaute unter Windows).

### app

- `framework/` – unabhängig von konkreten Features:
  - `AppContext`: Einstellungen, Session, Stores, Registries, aktuelle Auswahl,
    geöffnete Checkliste, `UiHost`, `EventBus`, `Notifications`.
  - `Module` / `ModuleRegistry`: Lebenszyklus `available → init → update/drawOverlay → shutdown`.
  - `UiHost`: registriert Fenster/Menüs/Popups bei cpp_app. Da cpp_app nur
    Funktionszeiger akzeptiert, werden Lambdas über einen Pool generierter Trampoline
    gebunden.
  - `EventBus`: typisierte Ereignisse (`framework/events.h`) zwischen Modulen.
  - `widgets`: wiederverwendbare UI-Bausteine (Suchfeld, Bestätigungs-Button, Status-Farben…).
  - `platform`: Explorer/Datei öffnen (Unicode), Dateidialoge.
  - `Alerts` / `DesktopNotifier`: Benachrichtigungen der Module an Windows weitergeben
    (siehe [benachrichtigungen.md](benachrichtigungen.md)).
- `modules/` – je Feature ein Ordner (siehe [module.md](module.md)).

## Datenfluss beim Arbeiten

1. *Reader Control* → `ctx.selectDevice(name)`: liest Flags/Zähler des Geräts, lädt den
   Datensatz aus `Sync.json`, wählt den laufenden Service und sendet `DeviceSelected` +
   `ServiceSelected`.
2. Das Checklisten-Modul reagiert auf `ServiceSelected`, speichert ggf. die alte Checkliste
   und öffnet die neue (`ServiceManager::findChecklist`).
3. Änderungen in der Checkliste stempeln die Aufgabe (Benutzer + Zeit) und markieren die
   Sitzung als ungespeichert. Nach `autosaveDelayMs` wird automatisch gespeichert.
4. Service-Aktionen (`ServiceManager`) liefern den neuen Datensatz zurück;
   `ctx.applyRecord()` aktualisiert Auswahl und Listen und sendet `RecordsChanged`.

## Gleichzeitiges Arbeiten mehrerer Benutzer

Alle gemeinsam genutzten Dateien liegen auf dem Netzlaufwerk. Dafür gilt:

- **Atomares Schreiben:** jede Datei wird als Temp-Datei geschrieben und per Rename ersetzt.
  Andere Leser sehen nie eine halb geschriebene Datei (Hauptursache für Abstürze in v1).
- **Retry:** gesperrte Dateien werden bis zu ~3 s erneut versucht.
- **Read-Modify-Write:** Listen (`Sync.json`, `extern.json`, `History.json`,
  Bestellungen) werden unmittelbar vor dem Schreiben neu gelesen; nur der eigene Eintrag
  wird geändert. Unbekannte Felder bleiben erhalten.
- **Checklisten-Merge** (`checklist/merge.h`): vor jedem Speichern und regelmäßig
  (`refreshIntervalMs`) wird die Datei von der Platte gelesen und elementweise
  zusammengeführt – „neuester Zeitstempel gewinnt“ pro Aufgabe, pro Arbeitsgang und für die
  Zeilenstruktur von Tabellen. So gehen gleichzeitige Eingaben verschiedener Mitarbeiter
  nicht verloren.
- **Polling statt Dauerlesen:** Änderungen anderer werden über die Änderungszeit der Datei
  erkannt (gedrosselt), nicht jedes Frame.

## Erweiterungspunkte

| Was | Wo | Beispiel |
|---|---|---|
| Neues Fenster/Feature | eigenes Modul | `modules/orders` |
| Automatisch ausgefüllte Checklisten-Werte | `ctx.expressions.add(...)` | `@rinfo.serial` |
| Aktion nach „Arbeitsgang fertigmelden“ | `ctx.sectionActions.add(...)` | `update_status_rinfo` |
| Neue Hardware-Option | `devices::kFlags` | `LTS` |
| Neuer Status | `service::status` | `geplant extern` |
| Kommunikation zwischen Modulen | `framework/events.h` | `RecordsChanged` |
| Windows-Benachrichtigung | `ctx.alerts.addChannel(...)` + `report(...)` | `orders.prepared` |
