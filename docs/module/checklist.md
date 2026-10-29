# checklist – Checkliste / Checklisten Editor

Fenster: **Checkliste** (alle), **Checklisten Editor** (Poweruser).

## Checkliste
- Zeigt die Checkliste des gewählten Service. Beim Öffnen sind alle Arbeitsgänge
  zugeklappt; „Alle auf/zu“ öffnet bzw. schließt alle.
- Kopfzeile je Arbeitsgang: `[erledigt/gesamt]`, bei Abschluss Benutzer und Zeit.
- Fortschrittsbalken, „Alle auf/zu“, Filter „Nur offene Aufgaben“.
- **Speichern automatisch** nach `autosaveDelayMs` (Standard 2 s) und beim Gerätewechsel;
  Strg+S speichert sofort. Status „Gespeichert/Ungespeichert“ in der Werkzeugleiste.
- Änderungen anderer Benutzer werden laufend übernommen (Merge pro Aufgabe).
- **Externer Service**: geöffnet wird die Checkliste der lokalen Kopie. Ist der
  Geräteordner erreichbar, wird sie automatisch in beide Richtungen mit der Checkliste im
  Geräteordner abgeglichen (Hinweiszeile mit Uhrzeit des letzten Abgleichs, Tooltip zeigt
  beide Pfade). Ohne Verbindung wird nur lokal gespeichert. Siehe [extern.md](extern.md).
- (?) neben einer Aufgabe: Kommentar und Anhänge (Drag & Drop, Klick öffnet,
  Rechtsklick entfernt). Rot = Kommentar/Anhang vorhanden.
- Combo-/Datumsfelder: Rechtsklick leert die Auswahl.
- Tabellen: „+ Zeile“, Enter in der letzten Zeile hängt eine Zeile an, „x“ löscht eine Zeile.
- Arbeitsgang fertigmelden: füllt Expressions, listet offene Aufgaben zur Bestätigung,
  führt die Post-Action aus und zeigt ggf. den Hinweis-Dialog.
- **Record erstellen**: wählbare, sortierbare Auswahl fertiger Arbeitsgänge, wahlweise
  englisch übersetzt, als PDF (Edge) oder HTML. **E-Mail erstellen**: englischer Record
  ohne Zeiten/Namen + Reader Evaluation als Outlook-Entwurf. Beides siehe
  [records.md](records.md).
- Tabellen: Spaltenbreiten/-reihenfolge werden pro Tabellenname gemerkt, wenn
  *Einstellungen → Tabellenformatierung speichern* an ist.

## Checklisten Editor
- Bearbeitet die aktive Checkliste oder eine separat geladene **Vorlage**
  („Vorlage laden…“, „Neue Vorlage…“, „Aktive Checkliste bearbeiten“).
- Arbeitsgänge: anlegen, verschieben, entfernen, bearbeiten (Name, Post-Action + Argumente,
  Berechtigungen, **Im E-Mail-Export anzeigen**, Hinweis-Dialog). Arbeitsgänge ohne
  E-Mail-Export tragen im Editor den Zusatz „[nicht im E-Mail-Export]“.
- Elemente: Einzelaufgabe, Aufgabengruppe, Tabelle; verschieben/entfernen.
- Aufgaben-Editor: Name, Tooltip, Typ (BOOL, INT, DOUBLE, STRING, TEXTBOX, COMBO,
  DATEPICKER), Expression, Standardwert, Format für Zahlen, Combo-Optionen.
- Tabellen-Spalten ändern: bestehende Zeilen behalten ihre Werte (Zuordnung über den
  Spaltennamen), bei Typwechsel wird die Zelle zurückgesetzt.
- Speichern als JSON, TXT (Format v1), CSV oder HTML.
- „Checkliste fertig“ schließt die Checkliste ab und erzeugt CSV + HTML daneben.

**Dateien:** Checkliste im Sync-Ordner, im Service-Ordner oder in der lokalen Kopie
(externer Service), Format siehe [dateiformate.md](../dateiformate.md).

**Code:** `app/src/modules/checklist/`, Logik `core/include/st/checklist/` (Abgleich:
`merge.h`, `ChecklistDocument::syncWithMirror` in `document.h`).
