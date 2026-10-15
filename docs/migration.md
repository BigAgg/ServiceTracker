# Änderungen gegenüber Version 1

Version 2 liest und schreibt alle Dateien von Version 1 (siehe [dateiformate.md](dateiformate.md)).
Beide Versionen können während der Umstellung parallel benutzt werden.

## Neu

- **Automatisches Speichern** der Checkliste (Standard 2 s nach der letzten Eingabe), Strg+S,
  Anzeige „Gespeichert/Ungespeichert“.
- **Live-Abgleich** mit Änderungen anderer Benutzer (pro Aufgabe zusammengeführt).
- **Rückmeldungen** als Meldungen unten rechts statt stiller Fehler oder Abstürze; Statuszeile
  mit Benutzer und letzter Synchronisation.
- Checkliste: Fortschritt je Arbeitsgang und gesamt, „Alle auf/zu“, „Nur offene Aufgaben“,
  relevante Arbeitsgänge beim Öffnen automatisch aufgeklappt, Enter in letzter Tabellenzeile
  fügt eine Zeile an.
- Checklisten-Editor: Vorlagen separat laden/neu anlegen, ohne die aktive Checkliste zu ersetzen;
  Spaltenänderungen behalten vorhandene Werte; Beschreibungen der Expressions/Actions.
- Reader Control: Status farbig in der Liste, Anzahl je Status, „Zuletzt verwendet“,
  Kontextmenü (Ordner öffnen, Name/Seriennummer kopieren), Doppelklick öffnet den Ordner.
- Filewalker: Kontextmenü, „Zum Gerät“, Drag & Drop in den aktuellen Ordner.
- Externe Services vollständig: Abschluss kopiert die Arbeit zurück, erstellt Record und Historie.
- Historie: Jahresfilter, Export der gefilterten Einträge, `extern.json` als Quelle.
- Bestellungen: „Nächster Schritt“, „Heute“-Buttons, Status farbig mit Anzahl, Bearbeitung
  mehrerer Benutzer wird zusammengeführt.
- Benutzer- und Config-Verwaltung als eigene Dialoge mit Suche.
- **Benachrichtigungen** (neu): `SALES` erhält eine Windows-Benachrichtigung und einen roten
  Punkt in der Taskleiste, sobald Bestellungen versandbereit sind ([benachrichtigungen.md](benachrichtigungen.md)).
- Alle Pfade über `ServiceTracker.json` konfigurierbar (Testumgebungen ohne `Y:`).

## Behobene Fehler aus Version 1

| Bereich | Fehler v1 | Jetzt |
|---|---|---|
| Alle Dateien | Schreiben nicht atomar – andere lasen halbe Dateien → Absturz/Datenverlust | Temp-Datei + Umbenennen, Retry |
| Alle Dateien | Endlosschleife, wenn eine Datei fehlte/gesperrt war (`while (!in) open`) | Retry mit Timeout, Meldung |
| Checkliste (txt) | Datumswerte und Checklisten-Version gingen verloren | werden gespeichert (v1-kompatibel) |
| Checkliste | neue Tabellenzeilen anderer Benutzer kamen leer an | Merge übernimmt Zeilen + Zellen |
| Checkliste | Zeilenumbrüche in Namen zerstörten die txt-Datei | werden ersetzt |
| Checkliste | Tabellen-Spalte ändern löschte alle Werte der Zeile | Werte bleiben (Zuordnung per Name) |
| Service Record | Werte wurden ungeprüft ins HTML geschrieben | HTML-Escaping |
| Externer Service | „Service abschließen“ ohne Funktion; Vorlagen ohne Umbenennung kopiert | implementiert |
| Bestellungen | „Letzte Änderung“ wurde bei jedem Laden überschrieben → falscher Abgleich | nur bei Änderungen |
| Bestellungen | Excel: „Abgeschlossen“ zeigte die offenen Bestellungen | korrigiert |
| Historie | Statistik „fehlende Durchlaufzeit“ invertiert, Durchschnitt mit Nullwerten | korrigiert |
| Alte Liste konvertieren | Beträge „1.234,50 €“ wurden zu 123450 | korrekt 1234,50 |
| Pfade | Umlaute in Pfaden teilweise falsch (ANSI/UTF-8 gemischt) | durchgehend UTF-8/Unicode |
| JSON | ungültiges UTF-8 führte zum Absturz beim Speichern | wird ersetzt |
| Start | fehlende `users.bin`/`Sync.json` → Absturz oder Hänger | Start mit Meldung, eingeschränkte Rechte |
| Excel-Export | globaler Zustand im Writer | sauber gekapselt |

## Geändertes Verhalten

- **Arbeitsgänge ohne Berechtigungen** sind für alle Benutzer bearbeitbar (v1: für niemanden).
  Vorlagen, in denen ein Arbeitsgang bewusst gesperrt sein soll, brauchen eine Berechtigung.
- **configs.json**: Configs ohne Adressregeln werden geladen (v1 hat sie ignoriert).
- **Record erstellen** schreibt in den Temp-Ordner statt in den Programmordner.
- **Benutzernamen** werden ohne Groß-/Kleinschreibung verglichen.
- **Installer** überschreibt das Fensterlayout bei Updates nicht mehr.
- **Testdatenbank aufsetzen** (Bestellungen, Passwort) wurde entfernt – die Funktion konnte
  echte Bestelldaten mit Testdaten füllen.
- Der Status kann von Powerusern manuell gesetzt werden.
