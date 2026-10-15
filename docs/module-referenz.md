# Modul-Referenz

Alle Module mit Fenstern, Funktionen, Berechtigungen und Dateien. Fensternamen sind
identisch zu Version 1, gespeicherte Docking-Layouts (`imgui.ini`) bleiben gültig.

Berechtigungsgruppen (Benutzerverwaltung): `SERVICE_CREATOR`, `SERVICE_CREATOR_EXTERN`,
`TECHSUPPORT`, `SALES`, beliebige weitere Abteilungen für Checklisten-Arbeitsgänge.
**Poweruser** dürfen zusätzlich Vorlagen, Configs und Benutzer bearbeiten.

---

## general – Menü „Einstellungen“

| Funktion | Beschreibung |
|---|---|
| Nur Arbeitsgänge mit Berechtigungen anzeigen | blendet fremde Arbeitsgänge in der Checkliste aus |
| Deutsches Zeitformat | Zeitstempel als `TT.MM.JJJJ hh:mm:ss` |
| Fertiggemeldet / Berechtigungen ignorieren | nur Poweruser: alles bearbeitbar |
| Erwartete und erledigte Geräte anzeigen | Reader Control ohne Filter zeigt sonst nur Geräte in Arbeit |
| Bestellungen als Popup öffnen | Darstellung im Fenster „Bestellungen“ |
| Benachrichtigungen | Windows-Benachrichtigungen, „bleibt bis zum Schließen“, roter Punkt, je Art an/aus + „Testen“ (nur für Empfänger, siehe [benachrichtigungen.md](benachrichtigungen.md)) |
| Pfade | zeigt alle konfigurierten Pfade, Klick öffnet den Ordner |
| Log-Datei / Anleitung öffnen | `run.log`, `Anleitung.pdf` |

Datei: `preferences.json` im Programmordner (übernimmt beim ersten Start `checklist.bin` von v1).

## devices – Geräte

**Reader Control**
- Suche (Strg+F, Esc leert), Filter nach Projekt und Status (mit Anzahl), „Zuletzt verwendet“.
- Liste mit farbigem Status; Klick wählt das Gerät für alle Fenster, Doppelklick öffnet den
  Ordner, Rechtsklick: Ordner öffnen / Name kopieren / Seriennummer kopieren.
- Ohne Filter: nur Geräte, die gerade bearbeitet werden (Einstellung „Erwartete und
  erledigte Geräte anzeigen“ zeigt alle aus der Liste). Mit Suchtext/Projekt: alle Geräte.
- „Neu laden“ liest Geräteordner und `Sync.json` neu.

**Reader Filewalker** – Dateien des Geräts durchsuchen. Klick auf Ordner öffnet ihn,
Doppelklick auf Datei öffnet sie, Rechtsklick: Öffnen / Im Explorer zeigen / Pfad kopieren.
Dateien per Drag & Drop in das Fenster ziehen = in den aktuellen Ordner kopieren.

**ReaderInfo** – Zähler aus der neuesten `*info*.txt` (Rechtsklick kopiert Werte).
**Konfiguration** – erkannte Hardware-Optionen (aus EEPROM + Basis-Konfiguration).

Nur mit Zugriff auf den Geräteordner: ReaderInfo, Konfiguration. Ohne Zugriff zeigt Reader
Control die Geräte aus `Sync.json`.

## service – Service Control

| Aktion | Voraussetzung | Wirkung |
|---|---|---|
| Service erstellen | `SERVICE_CREATOR`, kein laufender Service | legt `Service_JJJJ.MM.TT` im Geräteordner an (inkl. Ordner aus der Config), Sync-Ordner `SYNC/temp/<Gerät>/<Service>` mit kopierten Vorlagen, Status „Wareneingang“ |
| Erwarten | `SERVICE_CREATOR`, Gerät nicht in Arbeit | Status „erwartet“ mit Zusatzinformationen (Vorlage) |
| Aus Liste nehmen | `SERVICE_CREATOR` | Eintrag aus `Sync.json` entfernen (mit Rückfrage) |
| Service fertigmelden | `SERVICE_CREATOR`, laufender interner Service | schließt die Checkliste ab, kopiert den Sync-Ordner in den Geräteordner, erstellt den Service Record (HTML, im Service-Ordner und im Geräteordner), Eintrag in `History.json`, Status „erledigt“ |
| Status setzen | Poweruser | Status manuell ändern |
| Zusatzinformationen | `TECHSUPPORT` bearbeiten, alle lesen | |

Service-Liste: neueste oben, „(aktiv)“ markiert den laufenden Service; Doppelklick öffnet
den Ordner.

Stellt bereit: Expressions `@rinfo.readerinfo.*`, `@rinfo.config.<flag>`, `@rinfo.project`,
`@rinfo.serial`, `@rinfo.path`, `@rinfo.activeService` und die Section Action
`update_status_rinfo` (setzt den Status auf die Argumente, nur vorwärts).

## checklist – Checkliste / Checklisten Editor

**Checkliste** (alle)
- Zeigt die Checkliste des gewählten Service. Arbeitsgänge, die noch Arbeit brauchen und
  für die der Benutzer berechtigt ist, sind beim Öffnen aufgeklappt.
- Kopfzeile je Arbeitsgang: `[erledigt/gesamt]`, bei Abschluss Benutzer und Zeit.
- Fortschrittsbalken, „Alle auf/zu“, Filter „Nur offene Aufgaben“.
- **Speichern automatisch** nach `autosaveDelayMs` (Standard 2 s) und beim Gerätewechsel;
  Strg+S speichert sofort. Status „Gespeichert/Ungespeichert“ in der Werkzeugleiste.
- Änderungen anderer Benutzer werden laufend übernommen (Merge pro Aufgabe).
- (?) neben einer Aufgabe: Kommentar und Anhänge (Drag & Drop, Klick öffnet,
  Rechtsklick entfernt). Rot = Kommentar/Anhang vorhanden.
- Combo-/Datumsfelder: Rechtsklick leert die Auswahl.
- Tabellen: „+ Zeile“, Enter in der letzten Zeile hängt eine Zeile an, „x“ löscht eine Zeile.
- Arbeitsgang fertigmelden: füllt Expressions, listet offene Aufgaben zur Bestätigung,
  führt die Post-Action aus und zeigt ggf. den Hinweis-Dialog.
- Record erstellen: wählbare, sortierbare Auswahl fertiger Arbeitsgänge als HTML.

**Checklisten Editor** (Poweruser)
- Bearbeitet die aktive Checkliste oder eine separat geladene **Vorlage**
  („Vorlage laden…“, „Neue Vorlage…“, „Aktive Checkliste bearbeiten“).
- Arbeitsgänge: anlegen, verschieben, entfernen, bearbeiten (Name, Post-Action + Argumente,
  Berechtigungen, Hinweis-Dialog).
- Elemente: Einzelaufgabe, Aufgabengruppe, Tabelle; verschieben/entfernen.
- Aufgaben-Editor: Name, Tooltip, Typ (BOOL, INT, DOUBLE, STRING, TEXTBOX, COMBO,
  DATEPICKER), Expression, Standardwert, Format für Zahlen, Combo-Optionen.
- Tabellen-Spalten ändern: bestehende Zeilen behalten ihre Werte (Zuordnung über den
  Spaltennamen), bei Typwechsel wird die Zelle zurückgesetzt.
- Speichern als JSON, TXT (Format v1), CSV oder HTML.
- „Checkliste fertig“ schließt die Checkliste ab und erzeugt CSV + HTML daneben.

## extern – Externe Services (`TECHSUPPORT`)

1. Gerät wählen, „Service planen“ aufklappen, Datum (+ „Heute“) und Infos eingeben,
   **Externen Service erstellen**: Service-Ordner + Vorlagen im Geräteordner, Kopie des
   gesamten Geräteordners nach `extern/<Gerät>` (lokal), Status „geplant extern“, Eintrag in
   `extern.json`.
2. Beim Kunden lokal arbeiten (die Checkliste der lokalen Kopie wird automatisch geöffnet).
   Optional „Als extern markieren“.
3. **Service abschließen**: lokale Service-Daten in den Geräteordner kopieren, Record +
   Historie, lokale Kopie löschen, Status „erledigt extern“.
- „Aus Liste entfernen“ bricht ab (lokale Kopie bleibt erhalten).

## history – Service Historie (`TECHSUPPORT` oder `SALES`)

- Quelle wählbar: `History.json`, `Sync.json`, `extern.json`, `SYNC/oldhistory/*.json`.
- Filter nach Jahr und Suchtext, sortierbare/ausblendbare Spalten.
- Klick auf Gerät/Service wählt es aus, Klick auf Info kopiert, Klick auf Pfad öffnet.
- **Jahresexport**: Excel mit Kennzahlen (Services, Upgrades, Reparaturen, Garantie/Kulanz,
  Durchlaufzeit, Umsatz) gesamt und je Projekt – für die gefilterten Einträge.
- Poweruser: **Alte Liste konvertieren** (CSV der alten Excel-Liste → History-Format).

## orders – Bestellungen (`TECHSUPPORT` oder `SALES`)

- Status-Gruppen farbig mit Anzahl; Fenster-Tab markiert, solange Bestellungen
  „Versandbereit“ sind.
- `SALES`: Windows-Benachrichtigung „Es sind *n* Bestellungen versandbereit“, sobald eine
  Bestellung versandbereit wird und beim Start; roter Punkt in der Taskleiste, solange
  Bestellungen versandbereit sind ([benachrichtigungen.md](benachrichtigungen.md)).
- Bestellung anlegen (`TECHSUPPORT`): Kunde (bekannte Kunden + bisherige), Bestellnummer,
  Eingangsdatum (vorbelegt mit heute), Express.
- Bearbeiten: Datumsfelder mit „Heute“, **Nächster Schritt** trägt das heutige Datum beim
  nächsten offenen Schritt ein, Abschließen (benötigt Versanddatum), Stornieren mit Grund.
- Änderungen werden beim Verlassen eines Feldes gespeichert und mit anderen Benutzern
  zusammengeführt (neueste Änderung gewinnt pro Bestellung).
- Excel Export: Kennzahlen + Tabelle gesamt und je Kunde.

## configs – Menü „Configs“ (Poweruser)

Dialog „Reader Configs“: Projekte links, rechts Serviceordner, Vorlagen (Quelle +
Zielname mit `{reader}`/`{service}`), Basis-Konfiguration und EEPROM-Adressregeln
(Adresse, Wert, Konfiguration, zusätzliche Ordner). „Speichern“ schreibt `configs.json`.

## users – Menü „Benutzer“ (Poweruser)

Dialog „Benutzerverwaltung“: Benutzer suchen/anlegen, Poweruser-Flag, Abteilungen
hinzufügen/entfernen, Benutzer entfernen (nicht sich selbst). Der in `bootstrapAdmin`
konfigurierte Benutzer ist immer Poweruser.

## changes – Menü „Änderungen“

- **Änderung anstoßen**: Titel, Beschreibung, Dateien per Drag & Drop → neuer Ordner in
  `REQUESTS/<id>/`.
- **Änderungen verfolgen**: offene/bearbeitete Wünsche, Details, Dateien öffnen. Poweruser
  können Wünsche als bearbeitet markieren oder wieder öffnen.
