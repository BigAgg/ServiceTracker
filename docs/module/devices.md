# devices – Geräte

Gerätesuche und Geräteinformationen. Fenster: **Reader Control**, **Reader Filewalker**,
**ReaderInfo**, **Konfiguration**. Für alle Benutzer; ReaderInfo und Konfiguration nur mit
Zugriff auf den Geräteordner.

## Reader Control
- Suche (Strg+F, Esc leert), Filter nach Projekt und Status (mit Anzahl), „Zuletzt verwendet“.
- Liste mit farbigem Status; Klick wählt das Gerät für alle Fenster, Doppelklick öffnet den
  Ordner, Rechtsklick: Ordner öffnen / Name kopieren / Seriennummer kopieren.
- Ohne Filter: nur Geräte, die gerade bearbeitet werden (Einstellung „Erwartete und
  erledigte Geräte anzeigen“ zeigt alle aus der Liste). Mit Suchtext/Projekt: alle Geräte.
- „Neu laden“ liest Geräteordner und `Sync.json` neu.
- Ohne Zugriff auf den Geräteordner zeigt Reader Control die Geräte aus `Sync.json`.

## Reader Filewalker
Dateien des Geräts durchsuchen. Klick auf Ordner öffnet ihn, Doppelklick auf Datei öffnet
sie, Rechtsklick: Öffnen / Im Explorer zeigen / Pfad kopieren. Dateien per Drag & Drop in
das Fenster ziehen = in den aktuellen Ordner kopieren.

## ReaderInfo / Konfiguration
- **ReaderInfo**: Zähler aus der neuesten `*info*.txt` (Rechtsklick kopiert Werte).
- **Konfiguration**: erkannte Hardware-Optionen (aus EEPROM + Basis-Konfiguration).

## Benachrichtigung
`TECHSUPPORT`: „Neuer Reader im Wareneingang: *Seriennummer*“, sobald ein Gerät mit Status
„Wareneingang“ in der Liste erscheint; Klick wählt das Gerät aus. Kanal `devices.received`,
siehe [benachrichtigungen.md](../benachrichtigungen.md).

**Code:** `app/src/modules/devices/`, Logik `core/include/st/devices/`, Filter
`core/include/st/service/device_filter.h`.
