# records – Record erstellen, E-Mail erstellen

Kein eigenes Fenster. Bedienung über die Werkzeugleiste der **Checkliste** („Record
erstellen“, „E-Mail erstellen“), Dialog **E-Mail-Vorlage** über das Menü „Configs“
(Poweruser). Übersetzen, PDF-Druck und Outlook laufen im Hintergrund – das Programm bleibt
bedienbar, oben in der Mitte zeigt ein Hinweis den Fortschritt. Währenddessen sind beide
Buttons gesperrt.

## Record erstellen
- Auswahl der fertig gemeldeten Arbeitsgänge (sortierbar, „Alle“, „Keine“,
  „Wie E-Mail-Export“ = nur Arbeitsgänge mit der Option *Im E-Mail-Export anzeigen*).
- **In englisch übersetzen**: alle Texte des Records werden übersetzt (siehe
  [Übersetzung](#übersetzung)). Die Datei heißt dann `…_ServiceRecord_EN`.
- **Als PDF erstellen** (Standard): Druck über Microsoft Edge im Hintergrund, A4 mit
  Kopfzeile (Dateiname, Titel) und Fußzeile (`recordFooter`, „Page x of y“). Ohne Edge
  (bzw. ohne Haken) entsteht wie bisher eine HTML-Datei, mit Hinweis.
- Der Record wird im Temp-Ordner (`%TEMP%/ServiceTracker`) erstellt und geöffnet. Ist der
  vorherige Record noch in einem PDF-Viewer offen, heißt der neue `… (2)`.
- Ungespeicherte Änderungen der Checkliste werden vorher gespeichert.

## E-Mail erstellen
1. Record aller fertig gemeldeten Arbeitsgänge, die *Im E-Mail-Export anzeigen* dürfen
   (Einstellung je Arbeitsgang im Checklisten Editor, Standard: ja), in Reihenfolge der
   Checkliste – **ins Englische übersetzt, ohne Bearbeitungszeiten und ohne Namen** (nur das
   Datum je Arbeitsgang), als PDF: `<Gerät>_Service_Report_<Datum>.pdf`.
2. Anhang „Reader Evaluation“: die neueste PDF-Datei im Unterordner `EV-Plate` des aktuellen
   Service-Ordners, deren Name „Reader Evaluation“ enthält (Groß-/Kleinschreibung egal).
   Gesucht wird zuerst im Ordner der geöffneten Checkliste (Sync-Ordner bzw. lokale Kopie),
   dann im Service-Ordner im Geräteordner. Fehlt die Datei, wird sie einfach nicht angehängt.
3. **Outlook** öffnet einen neuen Entwurf mit Empfängern, Betreff, Text und beiden Anhängen.
   Der Text steht über der Standard-Signatur. Gesendet wird erst durch den Benutzer.
4. Ohne Outlook (klassisch) – z. B. mit dem neuen Outlook, das keine Automatisierung
   unterstützt – öffnet sich die E-Mail über `mailto:` ohne Anhänge, und der Record wird im
   Explorer markiert (Anhänge per Drag & Drop einfügen).

Der Button ist gesperrt, solange kein freigegebener Arbeitsgang fertig gemeldet ist.

## E-Mail-Vorlage (Poweruser)
Configs → E-Mail-Vorlage: An (mehrere mit `;`), CC, Betreff, Text. Platzhalter (Klick fügt
ein): `{reader}` Gerät, `{project}` Projektnummer, `{service}` Service-Ordner, `{date}`
Datum des Service, `{user}` angemeldeter Benutzer. „Standard wiederherstellen“ setzt Betreff
und Text zurück. Änderungen anderer Poweruser werden automatisch übernommen.

## Übersetzung
- Über einen lokal laufenden **LibreTranslate**-Server (`translator.url` in
  `ServiceTracker.json`, Standard `http://localhost:5000`), Deutsch → Englisch.
- Übersetzt werden Namen von Arbeitsgängen, Aufgaben, Gruppen, Tabellen und Spalten, Werte
  von Text-/Textbox-Feldern und die Auswahl von Combo-Feldern – Zeile für Zeile. Zahlen,
  Datumswerte, Häkchen, Namen und Zeitstempel bleiben unverändert.
- **Übersetzungsdatei** (`paths.translationsFile`, Standard
  `configs/translations_de-en.json`, für alle gemeinsam): jede übersetzte Zeile wird dort
  gespeichert. Steht eine Zeile schon in der Datei, wird diese Übersetzung genommen – auch
  ohne Server. **Korrekturen von Hand** in dieser Datei werden also bei der nächsten
  Übersetzung verwendet und nie überschrieben. Öffnen über Configs → Übersetzungsdatei öffnen.
- Ist der Server nicht erreichbar und fehlen Zeilen in der Datei, wird kein Record erstellt
  (Meldung mit Grund) – ein halb übersetzter Record wäre unbrauchbar.

## Dateien
- `configs/mailTemplate.json` (neu): `{"to": "", "cc": "", "subject": "…", "body": "…"}`;
  fehlt sie, gilt die mitgelieferte Vorlage. Unbekannte Felder bleiben beim Speichern erhalten.
- `configs/translations_de-en.json` (neu): `{"deutsche Zeile": "englische Zeile", …}`.

## Code
- Ablauf ohne UI: `core/include/st/exporting/record_export.h` (`createRecord`, Edge-Aufruf),
  `exporting/mail.h` (Vorlage, Platzhalter, EV-Plate), `translate/translator.h` (Cache,
  Zeilenlogik), `translate/libretranslate.h` (Protokoll), `checklist/translation.h`,
  `checklist/export.h` (`emailSubset`, `HtmlOptions::hideWorkInfo/pageHeaderFooter`).
- Windows-Anbindung: `app/src/framework/http.h` (WinHTTP), `framework/outlook.h` (COM),
  `framework/platform.h` (`findPdfBrowser`, `printHtmlToPdf`, `runProcess`),
  `framework/background_job.h`.
- Modul: `app/src/modules/records/`.
