# Dateiformate & Kompatibilität

Grundsatz: **Alle Dateien von Version 1 werden unverändert gelesen und so geschrieben, dass
Version 1 sie weiterhin öffnen kann.** Neue Informationen werden nur als zusätzliche Felder
bzw. Zeilen ergänzt, die Version 1 ignoriert. Unbekannte Felder (z.B. von einer neueren
Version) bleiben beim Speichern erhalten.

Alle Dateien sind UTF-8. Geschrieben wird immer atomar (Temp-Datei + Umbenennen).

## Übersicht

| Datei | Ort (Standard) | Inhalt | Code |
|---|---|---|---|
| `*Checklist*.json` / `.txt` | Sync-Ordner bzw. Service-Ordner | Checkliste | `checklist/formats.h` |
| `Sync.json` | `SYNC/` | Geräte in Arbeit | `service/records.h` |
| `extern.json` | `SYNC/` | externe Services bis zum Fertigmelden | `service/records.h` |
| `History.json` | `SYNC/` | abgeschlossene Services | `service/history.h` |
| `History_extern.json` | `SYNC/` | abgeschlossene externe Services (neu, Format wie `History.json`) | `service/history.h` |
| `<Gerät>_info.json` | Service-Ordner | Datensatz beim Abschluss | `service/records.h` |
| `<Gerät>_SR_<Datum>.html` | Service- und Geräteordner | Service Record | `checklist/export.h` |
| `style.txt` | `SYNC/` | optionaler HTML-Kopf für Records | `checklist/export.h` |
| `configs.json` | `configs/` | Gerätekonfigurationen | `devices/device_config.h` |
| `statusAlerts.json` | `configs/` | Status-Benachrichtigungen (neu): `[{"status", "groups"}]` | `alerts/status_rules.h` |
| `extern_local.json` | `extern/` (lokal) | externe Services mit lokaler Kopie (neu, Format wie `Sync.json`) | `service/records.h` |
| `users_cache.bin` | Programmordner | eigener Eintrag aus `users.bin` für den Offline-Modus (neu, gleiches Format) | `users/user_store.h` |
| `users.bin` | `ServiceCreatorApp/` | Benutzer + Rechte | `users/user_store.h` |
| `Bestellungen*.json` | `SYNC/` | Bestellungen | `orders/order_store.h` |
| `REQUESTS/<id>/info.txt` | `ServiceCreatorApp/` | Änderungswünsche | `changes/change_requests.h` |
| `ServiceTracker.json` | neben der .exe | Konfiguration (neu, optional) | `config/settings.h` |
| `preferences.json` | Programmordner | persönliche Einstellungen (neu) | `config/preferences.h` |
| `imgui.ini`, `windowcontrol.ini` | Programmordner | Fensterlayout (cpp_app) | – |

## Checkliste – Struktur

```
Checklist  version, user, timestamp, finished, permissions
└─ Section ("Arbeitsgang")  name, time (h.mm), user, timestamp, finished, permissions,
   │                        postAction, postActionArgs, postPopup{title, header, text}
   ├─ Task      ("info")  Einzelaufgabe
   ├─ TaskGroup ("line")  Aufgaben in einer Zeile: name, singleTick, items[]
   └─ TaskTable ("table") name, timestamp, templateLine (Spalten), rows[]
Task: name, type, value, comment, attachments[], user, timestamp, expression, tooltip, fmt
```

Typen: `bool`, `int`, `double`, `string`, `textbox`, `combo` (`{selected, options[]}`),
`datepicker` (Text `TT.MM.JJJJ`). Eine Aufgabe gilt als erledigt, sobald `timestamp` gesetzt ist.
Zeitstempel haben das Format `JJJJ-MM-TT hh:mm:ss` und sind als Text sortierbar – darauf beruht
der Merge.

### JSON (`*.json`)

```json
{
  "version": "1.0", "user": "", "timestamp": "", "finished": false,
  "attachmentsDir": "Y:/…/attachments/", "lastedit": 0, "permissions": [],
  "sections": [{
    "name": "Wareneingang", "time": 0.3, "user": "", "timestamp": "", "finished": false,
    "permissions": ["LAGER"], "postAction": "update_status_rinfo", "postActionArgs": "Technik",
    "postPopup": {"title": "", "header": "", "text": ""},
    "items": [
      {"kind": "info",  "data": {"name": "Verpackung ok", "type": "bool", "value": false, "comment": "",
                                  "attachments": [], "user": "", "timestamp": "", "expression": "",
                                  "tooltip": "", "fmt": ""}},
      {"kind": "line",  "data": {"name": "Zubehör", "singleTick": false, "items": [ … Tasks … ]}},
      {"kind": "table", "data": {"name": "Teile", "timestamp": "", "templateLine": { … line … },
                                  "rows": [ … lines … ], "enteradd": false, "cfocus": -1, "rfocus": -1}}
    ]
  }]
}
```

- `attachmentsDir` und `lastedit` sind informativ (werden nicht gelesen).
- `enteradd`, `cfocus`, `rfocus` waren UI-Zustand in v1 und werden nur noch zur
  Kompatibilität geschrieben.
- Gelesen werden auch v1-Varianten: fehlende `items`/`rows`, `"multiline": true` bei
  `string` (→ Textbox), Post-Action `resolve_rinfo` (→ `auto_finish`).

### Text (`*.txt`, Format von Version 1)

Zeilenbasiert `SCHLÜSSEL=Wert` mit Blöcken `*_START` … `*_END`:

```
VERSION=1
FINISHED=0
SECTIONS_COUNT=1
PERMISSIONS_START
PERMISSIONS_END
SECTION_START
NAME=Wareneingang
TIME=1.3
FINISHED=0
ITEMS_COUNT=1
POST_ACTION=update_status_rinfo
CHECKINFO_START
VERSION=1
VARIABLE_BOOL=1
NAME=Verpackung ok
COMMENT_START
mehrzeiliger Kommentar
COMMENT_END
USER=lager
TIMESTAMP=2026-02-01 08:55:00
CHECKINFO_END
SECTION_END
```

Weitere Blöcke: `CHECKLINE_START/END` (Gruppe, `SINGLETICK=`, `ITEMS_COUNT=`),
`CHECKTABLE_START/END` mit `TEMPLATE_START/END` und `ROWS_START/END`, `TEXTBOX_START/END`,
`COMBO_START/END` mit `SELECTED=` und `OPTIONS_START/END`, `ATTACHMENTS_START/END`,
`TOOLTIP_START/END`, `POSTPOPUP_TEXT_START/END`, `PERMISSIONS_START/END`.

Unterschiede beim Schreiben gegenüber v1 (alle von v1 lesbar):
- `VERSION=1` bleibt die Formatversion. Die Checklisten-Version (Editor) steht zusätzlich in
  `CHECKLIST_VERSION=` – v1 hat sie im Text-Format verloren.
- Datumswerte stehen in `VARIABLE_DATE=` – v1 hat sie nicht gespeichert.
- Zahlen werden verlustfrei geschrieben (v1: 6 signifikante Stellen).
- Zeilenumbrüche in einzeiligen Feldern werden durch Leerzeichen ersetzt (v1 hat dadurch
  defekte Dateien erzeugt).

Der Reader akzeptiert CRLF und ignoriert unbekannte Zeilen.

## Sync.json / extern.json

Array von Datensätzen, ein Eintrag pro Gerät:

```json
[{
  "name": "0601-1234", "path": "Y:/Gerätenachweise/0601 …/0601-1234",
  "status": "Wareneingang", "activeservice": "Service_2026.03.15",
  "syncfolder": "Y:/…/SYNC/temp/0601-1234/Service_2026.03.15",
  "additionalinfo": "Kunde: …", "services": ["Service_2026.03.15"], "duration": 0
}]
```

Status von Programm: `erwartet`, `Wareneingang`, `erledigt`, `geplant extern`, `extern`,
`extern abgeschlossen` (neu: zurückkopiert, wartet auf „Service fertigmelden“),
`erledigt extern`; Section Actions können beliebige weitere setzen (außer bei externen
Services, siehe [module/extern.md](module/extern.md)). Leerer Status bzw. kein
Eintrag = Gerät nicht in der Liste.

## History.json

```json
[{"name": "0601-1234", "services": [ { …Datensatz wie oben…, "total_price": 150.5, "end_date": "2026.03.18" } ]}]
```

`total_price` = Summe der Spalte *Preis* der Tabelle *Preisliste (Rabatt bereits
berechnet)* im Arbeitsgang *Preisliste*. `duration` = Tage zwischen Service-Datum und Abschluss.

## configs.json

```json
[{
  "project": "0601",
  "folders": ["Bilder"],
  "files": [{"path": "Y:/…/Checklist.json", "rename": "{reader}_Checklist_{service}.json"}],
  "filesExtern": [{"path": "Y:/…/Checklist_extern.json", "rename": "{reader}_Checklist_{service}.json"}],
  "addresses": [{"address": 100, "flagToSet": 4, "validationvalue": 1, "foldersToAdd": ["LUM"]}],
  "baseFlags": ["abs"]
}]
```

`filesExtern` ist neu und optional: Vorlagen für externe Services. Fehlt der Schlüssel oder ist
die Liste leer, werden die `files` verwendet; er wird nur geschrieben, wenn er belegt ist.

`flagToSet` ist der Bitwert der Hardware-Option (FI=1, ABS=2, LUM=4, TRF=8, ALPHA=16,
EXT_INC=32, FP=64, TRF_LASER=128, LTS=256). `baseFlags` ist neu und optional – fehlt es,
gelten die Standardwerte von v1 (0601 → ABS, 0471/0472 → FP). Configs ohne Adressen werden
jetzt ebenfalls geladen (v1 hat sie ignoriert).

## users.bin

Binär: wiederholt `uint32 Länge` (little endian) + Datensatz, jedes Byte XOR `0x5A`.
Datensatz: `name[poweruser]=GRUPPE1,GRUPPE2`, z.B. `mmueller[0]=TECHSUPPORT,LAGER`.
Benutzernamen werden ohne Beachtung der Groß-/Kleinschreibung verglichen.

## Bestellungen

```json
{"Bestellungen": [{
  "id": 1234567890123, "Bestellnummer": "", "Rechnungsnummer": "", "Kunde": "",
  "Eingangsdatum": "2026.01.01", "Sivas erfasst Datum": "", "Gerichtet Datum": "",
  "Versanddatum": "", "Abgeschlossen Datum": "", "Storniert Datum": "", "Storniert Notiz": "",
  "Preis": 0.0, "Expressversand": false, "Dateien": [], "Letzte Änderung": "2026-01-01 10:00:00"
}]}
```

Status ergibt sich aus den gefüllten Datumsfeldern (Storniert > Erledigt > Versendet >
Versandbereit > Sivas erfasst > Eingegangen). `Letzte Änderung` wird nur noch bei echten
Änderungen gesetzt (v1 hat sie bei jedem Laden überschrieben).

## Änderungswünsche (`REQUESTS/<id>/info.txt`)

```
ID=123
STATUS=OPEN            (oder CLOSED)
SUBJECT=Titel
AUTHOR=mmueller        (neu)
CREATED=2026-03-15 10:00:00   (neu)
MESSAGE_START
Text
MESSAGE_END
ATTACHMENTS_START
Y:/…/REQUESTS/123/attachments/bild.png
ATTACHMENTS_END
```
