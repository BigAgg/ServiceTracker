# Konfiguration

## ServiceTracker.json (Installation)

Optional, liegt **neben `ServiceTracker.exe`**. Ohne Datei verhält sich das Programm wie
Version 1 (Pfade auf `Y:`). Es müssen nur die Werte angegeben werden, die abweichen:

```json
{
  "paths": {
    "syncDir": "D:/Test/SYNC",
    "devicesRoot": "D:/Test/Gerätenachweise"
  },
  "autosaveDelayMs": 3000
}
```

Die ausgelieferte Datei `deploy/ServiceTracker.json` enthält alle Standardwerte (ein Test
stellt sicher, dass sie mit den Standardwerten im Code übereinstimmt). Der Installer legt sie
nur an, wenn noch keine vorhanden ist.
Ist die Datei fehlerhaft, startet das Programm mit Standardwerten und zeigt eine Meldung.

| Schlüssel | Standard | Bedeutung |
|---|---|---|
| `paths.usersFile` | `Y:/CrossDept/Automation/ServiceCreatorApp/users.bin` | Benutzer und Rechte |
| `paths.requestsDir` | `…/ServiceCreatorApp/REQUESTS` | Änderungswünsche |
| `paths.devicesRoot` | `Y:/Gerätenachweise` | Geräteordner (Projektordner `0xxx …`) |
| `paths.configsDir` | `…/ServiceCreatorApp/configs` | enthält `configs.json` |
| `paths.syncDir` | `…/ServiceCreatorApp/SYNC` | `Sync.json`, `History.json`, `History_extern.json`, `style.txt`, `temp/`, `oldhistory/` |
| `paths.externLocalDir` | `extern` | lokale Kopien externer Services (relativ zum Programmordner) |
| `paths.externFile` | `…/SYNC/extern.json` | externe Services bis zum Fertigmelden |
| `paths.ordersFile` | `…/SYNC/Bestellungen.json` | Bestellungen |
| `paths.crashDir` | `…/ServiceCreatorApp/crashes` | Kopie des Logs nach einem Absturz |
| `paths.translationsFile` | `…/ServiceCreatorApp/configs/translations_de-en.json` | gemeinsame Übersetzungen (von Hand korrigierbar), siehe [module/records.md](module/records.md#übersetzung) |
| `updateUrl` | GitHub-Release-API `BigAgg/ServiceTracker` | automatische Updates (cpp_app) |
| `updateInstaller` | `install_ServiceTracker.exe` | Name des Installers im Release |
| `bootstrapAdmin` | `ajahraus` | immer Poweruser (Schutz vor Aussperren) |
| `refreshIntervalMs` | `1000` | wie oft Änderungen anderer Benutzer geprüft werden |
| `autosaveDelayMs` | `2000` | Checkliste wird so lange nach der letzten Eingabe gespeichert (`0` = aus) |
| `knownCustomers` | BMG-Niederlassungen | Vorschläge beim Anlegen von Bestellungen |
| `recordTitle`, `recordFooter` | BMG Labtech | Titel/Fußzeile des Service Records |
| `translator.url` | `http://localhost:5000` | LibreTranslate-Server (lokal oder im Netz) |
| `translator.apiKey` | leer | nur nötig, wenn der Server mit API-Keys läuft |
| `translator.source`, `translator.target` | `de`, `en` | Sprachen der Übersetzung |
| `translatorTimeoutMs` | `30000` | Zeitlimit je Übersetzungsanfrage |
| `pdfBrowser` | leer | Pfad zu `msedge.exe`/`chrome.exe` für den PDF-Druck; leer = Edge wird gesucht |

Für Tests mit Kopien der Daten genügt es, `paths` auf einen lokalen Ordner zu setzen.

## preferences.json (pro Benutzer/Rechner)

Wird vom Programm im Programmordner geschrieben (Menü *Einstellungen*): Anzeigeoptionen der
Checkliste, Liste der zuletzt verwendeten Geräte, Darstellung der Bestellungen,
Benachrichtigungen (`desktopNotifications`, `persistentNotifications`, `taskbarBadge`, `mutedAlerts` = abgeschaltete
Kanal-IDs, siehe [benachrichtigungen.md](benachrichtigungen.md)), `saveTableLayout`
(Tabellenformatierung speichern, Standard `false`). Beim ersten
Start werden die Einstellungen aus `checklist.bin` (Version 1) übernommen.

## Fensterlayout

`imgui.ini` und `windowcontrol.ini` (cpp_app) speichern Docking-Layout, geöffnete Fenster und
Theme. Der Installer legt ein Standard-Layout nur an, wenn noch keines existiert. Layouts aus
Version 1 funktionieren weiter, da alle Fensternamen gleich geblieben sind.

Mit *Einstellungen → Tabellenformatierung speichern* kommen Spaltenbreiten und -reihenfolge
der Checklisten-Tabellen dazu: ein Eintrag `[Table]` je Tabellenname (und Spaltenzahl),
unabhängig von Checkliste und Arbeitsgang – die Datei wächst also nicht mit jeder Checkliste.
Ändert sich die Spaltenzahl einer Tabelle, beginnt sie mit Standardbreiten.

## style.txt (Service Record)

Liegt `SYNC/style.txt` vor, ersetzt ihr Inhalt den kompletten HTML-Kopf des Service Records
(alles bis einschließlich `<body>` und Kopfzeile) – wie in Version 1.
