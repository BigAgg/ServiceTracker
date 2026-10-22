# Modul-Referenz

Übersicht aller Module. Jedes Modul ist in einer eigenen Datei unter [module/](module/)
beschrieben (Fenster, Funktionen, Berechtigungen, Dateien, Code). Fensternamen sind
identisch zu Version 1, gespeicherte Docking-Layouts (`imgui.ini`) bleiben gültig.

Berechtigungsgruppen (Benutzerverwaltung): `SERVICE_CREATOR`, `SERVICE_CREATOR_EXTERN`,
`TECHSUPPORT`, `SALES`, beliebige weitere Abteilungen für Checklisten-Arbeitsgänge.
**Poweruser** dürfen zusätzlich Vorlagen, Configs und Benutzer bearbeiten.

| Modul | Fenster / Menü | Wer | Beschreibung |
|---|---|---|---|
| [general](module/general.md) | Menü „Einstellungen“ | alle | persönliche Einstellungen, Pfade, Diagnose |
| [devices](module/devices.md) | Reader Control, Reader Filewalker, ReaderInfo, Konfiguration | alle | Geräte suchen und auswählen, Dateien, Zähler, Hardware |
| [service](module/service.md) | Service Control | alle, Aktionen `SERVICE_CREATOR` | Services anlegen, erwarten, fertigmelden |
| [checklist](module/checklist.md) | Checkliste, Checklisten Editor | alle / Poweruser | Checkliste bearbeiten, Vorlagen pflegen |
| [extern](module/extern.md) | Externe Services | `TECHSUPPORT` | Services beim Kunden |
| [history](module/history.md) | Service Historie | `TECHSUPPORT`, `SALES` | Auswertung, Jahresexport |
| [orders](module/orders.md) | Bestellungen | `TECHSUPPORT`, `SALES` | Ersatzteilbestellungen |
| [configs](module/configs.md) | Menü „Configs“ | Poweruser | Ordner, Vorlagen, EEPROM-Regeln je Projekt |
| [users](module/users.md) | Menü „Benutzer“ | Poweruser | Benutzer und Abteilungen |
| [changes](module/changes.md) | Menü „Änderungen“ | alle | Änderungswünsche |
| [statusalerts](module/statusalerts.md) | Configs → Status-Benachrichtigungen | alle / Poweruser | Windows-Benachrichtigung bei bestimmten Status |

Querschnittsthemen: [Benachrichtigungen](benachrichtigungen.md),
[Dateiformate](dateiformate.md), [Konfiguration](konfiguration.md).
