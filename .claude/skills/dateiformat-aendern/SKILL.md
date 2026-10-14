---
name: dateiformat-aendern
description: Ein gespeichertes Dateiformat (Checkliste, Sync.json, History.json, configs.json, users.bin, Bestellungen, ...) ändern oder ein neues einführen, ohne die Kompatibilität mit Version 1 und anderen Installationen zu brechen.
---

# Dateiformat ändern

Mehrere Programmversionen arbeiten gleichzeitig auf denselben Dateien. Deshalb:

## Regeln

1. **Nur ergänzen, nie umbenennen/entfernen.** Neue JSON-Felder bzw. neue `KEY=`-Zeilen.
   Version 1 ignoriert unbekannte Felder/Zeilen – das ist die Grundlage der Kompatibilität.
2. **Unbekannte Felder erhalten.** Beim Speichern vorhandene Objekte als Basis nehmen
   (`extra`-Member wie in `ServiceRecord`/`Order`, oder `merge_patch`).
3. **Tolerant lesen.** `util::json::str/number/boolean/strings/array` verwenden – fehlende
   Felder und falsche Typen liefern Standardwerte statt Exceptions.
4. **Atomar schreiben** (`util::json::save`, `util::writeFileAtomic`).
5. **Read-Modify-Write** bei gemeinsam genutzten Listen: unmittelbar vor dem Schreiben neu
   laden, nur den eigenen Eintrag ändern.
6. Text-Format der Checkliste: Einzeilige Werte mit `util::singleLine`, neue Blöcke als
   `NAME_START`/`NAME_END`, Reihenfolge der bestehenden Zeilen beibehalten.

## Tests (Pflicht)

- Datei im **alten** Format lesen (Fixture in `tests/data/` oder Raw-String im Test).
- Round-Trip: `parse(write(x)) == x`.
- Für das Text-Format: `writeLegacyText(parseLegacyText(fixture)) == fixture` bleibt grün.
- Unbekannte Felder bleiben nach dem Speichern erhalten.

## Doku

`docs/dateiformate.md` aktualisieren (Beispiel + Hinweis, was Version 1 damit macht).
