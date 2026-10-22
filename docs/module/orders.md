# orders – Bestellungen

Ersatzteilbestellungen (POs). Fenster: **Bestellungen** (`TECHSUPPORT` oder `SALES`).

- **Bestellung anlegen** (`TECHSUPPORT` und `SALES`): Kunde mit **Autovervollständigung**
  (Vorschläge aus `knownCustomers` + Kunden bisheriger Bestellungen; Pfeiltasten wählen,
  Enter/Tab übernimmt, Esc schließt die Liste) oder Auswahl „Bekannte Kunden...“,
  Bestellnummer, Eingangsdatum (vorbelegt mit heute), Express.
- Status-Gruppen farbig mit Anzahl; Fenster-Tab markiert, solange Bestellungen
  „Versandbereit“ sind.
- Ohne „Als Popup öffnen“: Bestellungen abwechselnd farbig hinterlegt und durch Linien
  getrennt; eine aufgeklappte Bestellung behält ihre Farbe über den ganzen Bereich.
- Bearbeiten: Datumsfelder mit „Heute“, **Nächster Schritt** trägt das heutige Datum beim
  nächsten offenen Schritt ein, Abschließen (benötigt Versanddatum), Stornieren mit Grund.
- Änderungen werden beim Verlassen eines Feldes gespeichert und mit anderen Benutzern
  zusammengeführt (neueste Änderung gewinnt pro Bestellung).
- Excel Export: Kennzahlen + Tabelle gesamt und je Kunde.

## Benachrichtigung
`SALES`: „Es sind *n* Bestellungen versandbereit“, sobald eine Bestellung versandbereit wird
und beim Start; roter Punkt in der Taskleiste, solange Bestellungen versandbereit sind.
Kanal `orders.prepared`, siehe [benachrichtigungen.md](../benachrichtigungen.md).

**Dateien:** `paths.ordersFile` (Standard `SYNC/Bestellungen.json`).

**Code:** `app/src/modules/orders/`, Logik `core/include/st/orders/order_store.h`.
