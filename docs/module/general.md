# general – Menü „Einstellungen“

Persönliche Einstellungen des Benutzers, Pfade und Diagnose. Für alle Benutzer.

| Funktion | Beschreibung |
|---|---|
| Nur Arbeitsgänge mit Berechtigungen anzeigen | blendet fremde Arbeitsgänge in der Checkliste aus |
| Deutsches Zeitformat | Zeitstempel als `TT.MM.JJJJ hh:mm:ss` |
| Tabellenformatierung speichern | Spaltenbreiten/-reihenfolge der Checklisten-Tabellen pro Tabellenname merken (Standard aus) |
| Fertiggemeldet / Berechtigungen ignorieren | nur Poweruser: alles bearbeitbar |
| Erwartete und erledigte Geräte anzeigen | Reader Control ohne Filter zeigt sonst nur Geräte in Arbeit |
| Bestellungen als Popup öffnen | Darstellung im Fenster „Bestellungen“ |
| Benachrichtigungen | Windows-Benachrichtigungen, „bleibt bis zum Schließen“, roter Punkt, je Art an/aus + „Testen“ (nur für Empfänger, siehe [benachrichtigungen.md](../benachrichtigungen.md)) |
| Pfade | zeigt alle konfigurierten Pfade, Klick öffnet den Ordner |
| Log-Datei / Anleitung öffnen | `run.log`, `Anleitung.pdf` |

**Dateien:** `preferences.json` im Programmordner (übernimmt beim ersten Start `checklist.bin`
von v1), siehe [konfiguration.md](../konfiguration.md).

**Code:** `app/src/modules/general/`, Einstellungen `core/include/st/config/preferences.h`.
