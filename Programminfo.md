# Generelle Information zur Funktionsweise des Programmes

Das Programm wird in einer Firma verwendet um Services von Geräten zu verwalten.
Die Daten über Vorhandene Geräten werden vom lokalen Server (Y-Laufwerk) gelesen.
Über die Ordnerstruktur werden Gerätetypen und deren einzelnen Geräte-Seriennummern gemanaged.

Zudem werden Ersatzteilversendungen darüber gesteuert.

## Workflow service im haus:

- Ein Gerät kommt herein
- Ein service wird angelegt
    - Dabei wird ein neuer Ordner erstellt
    - Eine Checkliste aus einem Template kopiert und zum bearbeiten bereitgestellt
- Alle mitarbeiter müssen gleichzeitig an der Checkliste des Gerätes arbeiten können
- Die Dateien werden auf einem Sync ordner auf welchem jeder zugriff hat bearbeitet und erst nach fertigstellung des services in den geräteordner kopiert.
- Service wird bearbeitet und danach abgeschlossen
- Beim Abschließen wird das Gerät in einer Liste gepflegt um daten auszuwerten und diese in excel zu exportieren

## Workflow service extern:

- Externer Service wird angelegt
    - Kopiert den Geräte-Ordner in ein lokales Verzeichnis, in welchem dann beim Kunden gearbeitet wird
    - Für externe Services können in den Configs eigene Vorlagen (z. B. eine andere Checkliste) hinterlegt werden
- Gearbeitet wird im lokalen Verzeichnis
    - Wenn man intern arbeitet und Zugriff auf den Geräte-Ordner hat, wird die Checkliste mit dem Geräteordner synchronisiert
- Wenn der Servicetechniker wieder im Haus ist, schließt er den Service extern ab
    - Die Checkliste wird ein letztes Mal synchronisiert, der lokale Geräte-Ordner wird über den originalen kopiert, die lokalen Dateien werden gelöscht
    - Status "extern abgeschlossen"
- Anschließend wird intern "Service fertigmelden" gewählt, um den Service endgültig abzuschließen
- Externe Services werden in einer eigenen Liste gepflegt (extern.json) und nicht in History, sondern in History extern (History_extern.json) getrackt

## Workflow Bestellungen:

- Bestellung wird angelegt
- Gerichtet und datum eingetragen
- In den versand gestellt und datum eingetragen
- Versendet und Rechnungsnummer sowie versanddatum werden eingetragen
- Bestellung wird abgeschlossen

## Benutzungsänderungen:

- Für den externen service ist es aktuell nicht möglich offline zu arbeiten. die readerliste wird nicht geladen der lokal angelegten services.
- fenster wie "Externe Services" werden nicht erstellt und fehlen somit.
- Möglicherweise kopiere den aktuellen stand meiner berechtigungen lokal und greife im offline fall darauf zurück?
- erstelle eine windows benachrichtigung, wenn ein reader mit dem status "versendet" auftaucht für alle mitarbeiter im TECHSUPPORT
- erstelle eine windows benachrichtigung, wenn ein reader mit dem status "Extern abschließen" auftaucht für alle mitarbeiter im TECHSUPPORT
- erstelle eine windows benachrichtigung, wenn ein reader mit dem status "im Versand" auftaucht für alle mitarbeiter in SALES
- erstelle eine windows benachrichtigung, wenn ein reader mit dem status "Rechnung erstellen"
- erstelle eine windows benachrichtigung, wenn ein reader mit dem status "Rechnung versendet" auftaucht für alle mitarbeiter in TECHSUPPORT