# history – Service Historie

Abgeschlossene Services auswerten. Fenster: **Service Historie** (`TECHSUPPORT` oder `SALES`).

- Quelle wählbar: `History.json`, `History_extern.json` (externe Services), `Sync.json`,
  `extern.json`, `SYNC/oldhistory/*.json`.
- Filter nach Jahr und Suchtext, sortierbare/ausblendbare Spalten.
- Klick auf Gerät/Service wählt es aus, Klick auf Info kopiert, Klick auf Pfad öffnet.
- **Jahresexport**: Excel mit Kennzahlen (Services, Upgrades, Reparaturen, Garantie/Kulanz,
  Durchlaufzeit, Umsatz) gesamt und je Projekt – für die gefilterten Einträge.
- Poweruser: **Alte Liste konvertieren** (CSV der alten Excel-Liste → History-Format).

**Code:** `app/src/modules/history/`, Logik `core/include/st/service/history.h`,
Export `core/include/st/exporting/`.
