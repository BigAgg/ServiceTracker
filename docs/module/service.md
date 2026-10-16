# service – Service Control

Services eines Geräts anlegen und abschließen. Fenster: **Service Control**.

| Aktion | Voraussetzung | Wirkung |
|---|---|---|
| Service erstellen | `SERVICE_CREATOR`, kein laufender Service | legt `Service_JJJJ.MM.TT` im Geräteordner an (inkl. Ordner aus der Config), Sync-Ordner `SYNC/temp/<Gerät>/<Service>` mit kopierten Vorlagen, Status „Wareneingang“ |
| Erwarten | `SERVICE_CREATOR`, Gerät nicht in Arbeit | Status „erwartet“ mit Zusatzinformationen (Vorlage) |
| Aus Liste nehmen | `SERVICE_CREATOR` | Eintrag aus `Sync.json` entfernen (mit Rückfrage) |
| Service fertigmelden (intern) | `SERVICE_CREATOR`, laufender interner Service | schließt die Checkliste ab, kopiert den Sync-Ordner in den Geräteordner, erstellt den Service Record (HTML, im Service-Ordner und im Geräteordner), Eintrag in `History.json`, Status „erledigt“ |
| Service fertigmelden (extern) | `SERVICE_CREATOR`, Status „extern abgeschlossen“ | schließt die Checkliste ab, Service Record, Eintrag in `History_extern.json`, Status „erledigt extern“, Eintrag aus `extern.json` entfernt – siehe [extern.md](extern.md) |
| Status setzen | Poweruser | Status manuell ändern |
| Zusatzinformationen | `TECHSUPPORT` bearbeiten, alle lesen | |

Während ein externer Service läuft („geplant extern“, „extern“) ist „Service fertigmelden“
gesperrt – zuerst muss unter „Externe Services“ **Extern abschließen** gewählt werden.

Service-Liste: neueste oben, „(aktiv)“ markiert den laufenden Service; Doppelklick öffnet
den Ordner.

## Expressions und Section Action
Stellt bereit: Expressions `@rinfo.readerinfo.*`, `@rinfo.config.<flag>`, `@rinfo.project`,
`@rinfo.serial`, `@rinfo.path`, `@rinfo.activeService` und die Section Action
`update_status_rinfo` (setzt den Status auf die Argumente, nur vorwärts). Der Status eines
externen Service („geplant extern“, „extern“, „extern abgeschlossen“) wird von der Section
Action nie überschrieben.

**Code:** `app/src/modules/service/`, Abläufe `core/include/st/service/service_manager.h`.
