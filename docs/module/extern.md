# extern – Externe Services

Services, die beim Kunden durchgeführt werden. Fenster: **Externe Services**
(`TECHSUPPORT`).

## Ablauf

```
 Externen Service erstellen ──► beim Kunden lokal arbeiten ──► Extern abschließen ──► Service fertigmelden
   „geplant extern“               („extern“, optional)          „extern abgeschlossen“    „erledigt extern“
   Server + lokale Kopie          im Haus: Checkliste wird      lokale Kopie → Server,    (Service Control)
                                  mit dem Server abgeglichen    lokale Kopie gelöscht     Record + History_extern.json
```

1. **Externen Service erstellen** – Gerät wählen, „Service planen“ aufklappen, Datum
   (+ „Heute“) und Infos eingeben:
   - `Service_JJJJ.MM.TT` im Geräteordner mit den **Extern-Vorlagen** der Config
     (ohne eigene Extern-Vorlagen: die normalen Vorlagen, siehe [configs.md](configs.md)),
   - Kopie des gesamten Geräteordners nach `extern/<Gerät>` (lokal, `paths.externLocalDir`),
   - Status „geplant extern“, Eintrag in `Sync.json` **und** `extern.json`.
2. **Arbeiten** – die Checkliste der lokalen Kopie wird automatisch geöffnet. Optional
   „Als "extern" markieren“ (Techniker ist beim Kunden).
   - Beim Kunden (kein Zugriff auf den Geräteordner): es wird nur lokal gespeichert.
   - Im Haus (Geräteordner erreichbar): die lokale Checkliste wird laufend **in beide
     Richtungen** mit der Checkliste im Geräteordner abgeglichen – neuester Eintrag gewinnt
     pro Aufgabe. Kollegen sehen den Stand und können im Geräteordner mitarbeiten.
     Anhänge werden mitkopiert, ihre Pfade zeigen jeweils auf die eigene Kopie.
   - Die Section Action `update_status_rinfo` ändert den Status eines externen Service nicht.
3. **Extern abschließen** (wieder im Haus, Geräteordner muss erreichbar sein):
   - letzter Abgleich der Checkliste mit dem Geräteordner (Änderungen der Kollegen bleiben
     erhalten),
   - der lokale Geräteordner wird über den Original-Geräteordner kopiert,
   - Anhangpfade der kopierten Checklisten zeigen wieder auf den Server,
   - die lokale Kopie wird gelöscht, Status „extern abgeschlossen“ (Sync.json + extern.json).
4. **Service fertigmelden** in [Service Control](service.md): schließt die Checkliste ab,
   erstellt den Service Record, Eintrag in `History_extern.json` (nicht in `History.json`),
   Status „erledigt extern“, Eintrag aus `extern.json` entfernt.

„Aus Liste entfernen“ bricht einen externen Service ab (die lokale Kopie bleibt erhalten).

## Offline beim Kunden

Ist das Netzlaufwerk beim Programmstart nicht erreichbar, startet ServiceTracker im
**Offline-Modus** (Statuszeile „Offline“):

- **Berechtigungen** kommen aus dem zuletzt gespeicherten Stand (`users_cache.bin` im
  Programmordner, wird bei jedem Start im Haus aktualisiert, enthält nur den eigenen Eintrag).
- **Reader Control** zeigt die Geräte mit lokaler Kopie auf diesem PC (aus
  `extern/extern_local.json`), mit Status und laufendem Service.
- **Externe Services**, **Checkliste**, **Service Control** (Service-Liste aus der lokalen
  Kopie) und der Filewalker der lokalen Kopie funktionieren; gespeichert wird lokal.
- Nicht offline möglich: „Extern abschließen“, „Als "extern" markieren“, „Aus Liste
  entfernen“, Bestellungen, Historie und alles andere, was das Netzlaufwerk braucht.
- Wird das Netzlaufwerk wieder erreichbar, erscheint ein Hinweis; nach einem Neustart sind
  alle Funktionen wieder da, die Checkliste wird dann automatisch abgeglichen.

## Dateien

| Datei | Inhalt |
|---|---|
| `SYNC/extern.json` | laufende externe Services bis zum Fertigmelden |
| `SYNC/History_extern.json` | abgeschlossene externe Services (Format wie `History.json`) |
| `extern/<Gerät>/` (lokal) | Arbeitskopie des Geräteordners |
| `extern/extern_local.json` (lokal) | externe Services mit lokaler Kopie auf diesem PC (für den Offline-Modus) |
| `users_cache.bin` (Programmordner) | eigene Berechtigungen für den Offline-Modus |

## Status

| Status | Bedeutung |
|---|---|
| `geplant extern` | angelegt, lokale Kopie vorhanden |
| `extern` | Techniker ist beim Kunden (manuell gesetzt) |
| `extern abgeschlossen` | Arbeit zurückkopiert, wartet auf „Service fertigmelden“ (neu) |
| `erledigt extern` | fertiggemeldet |

**Code:** `app/src/modules/extern/`, Abläufe `ServiceManager::createExternService`,
`finishExternService`, `finishService` in `core/include/st/service/service_manager.h`.
