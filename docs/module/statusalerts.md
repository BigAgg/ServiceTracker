# statusalerts – Status-Benachrichtigungen

Windows-Benachrichtigung für ausgewählte Abteilungen, sobald ein Gerät mit einem bestimmten
Status in der Service-Liste erscheint (z. B. „versendet“ → `TECHSUPPORT`). Kein eigenes
Fenster; Editor-Dialog **Status-Benachrichtigungen** über das Menü „Configs“ (Poweruser).

## Verhalten
- Je Regel ein Benachrichtigungskanal `status.<status>`; empfangen nur Benutzer der gewählten
  Abteilungen. Ein- und ausschaltbar unter Einstellungen → Benachrichtigungen.
- Gemeldet wird jedes Gerät, das neu mit dem Status auftaucht (Vergleich ohne Groß-/
  Kleinschreibung), eine Meldung pro Gerät: „0601-1234: Status „versendet““. Mehr als
  3 auf einmal werden zusammengefasst. Beim Programmstart werden vorhandene Geräte gemeldet.
- Eigene Statusänderungen (z. B. Arbeitsgang fertigmelden) melden nichts.
- Klick auf die Meldung öffnet „Reader Control“ und wählt das Gerät aus.
- Änderungen an den Regeln durch andere Poweruser werden automatisch übernommen
  (Prüfung alle `5 × refreshIntervalMs`, mindestens 1 s).

## Editor
- Tabelle Status (mit Vorschlägen aus den aktuell verwendeten Status) und Abteilungen
  (Mehrfachauswahl aus allen bekannten Abteilungen), „x“ entfernt eine Regel.
- „Regel hinzufügen“, „Standard wiederherstellen“ (mitgelieferte Regeln), „Speichern“,
  „Verwerfen“. Beim Speichern werden Regeln ohne Status/Abteilung entfernt und gleiche
  Status zusammengefasst.

## Dateien
`configs/statusAlerts.json` (neu): `[{"status": "versendet", "groups": ["TECHSUPPORT"]}]`.
Fehlt die Datei, gelten die mitgelieferten Regeln (siehe
[benachrichtigungen.md](../benachrichtigungen.md)); eine leere Liste schaltet alle
Status-Benachrichtigungen ab.

**Code:** `app/src/modules/statusalerts/`, Logik `core/include/st/alerts/status_rules.h`.
