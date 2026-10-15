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

- Externer service wird angelegt
    - Kopiert den Geräte-Ordner in ein lokales verzeichnes in welchem dann beim kunden gearbeitet wird
    - Gearbeitet wird im lokalen verzeichnis
    - wenn der service techniker wieder im haus ist, schlißt er den service ab und der angelegte service ordner wird in den originalen geräte ordner kopiert. die lokalen dateien werden gelöscht
- Alle externen services sollen ebenfalls in einer unabhängigen liste gepflegt werden

## Workflow Bestellungen:

- Bestellung wird angelegt
- Gerichtet und datum eingetragen
- In den versand gestellt und datum eingetragen
- Versendet und Rechnungsnummer sowie versanddatum werden eingetragen
- Bestellung wird abgeschlossen

## Neuimplementierungen:

### Benachrichtigungssystem:

- Personen aus "SALES" Abteilung sollen eine windows benachrichtung bekommen
    - Benachrichtigung wie eine Chat benachritigung bei Teams, klein und informativ
    - Beim Klicken der Benachritigung wird der Servicetracker geöffnet
    - Die Benachrichtigung soll neu auftauchen sobald eine neue Bestellung versandbereit ist oder beim starten des programmes wenn die anzahl > 0
    - Die Benachritigung soll anzeigen:
    "ServiceTracker - Bestellungen
    Es sind [n] Bestellungen
    versandbereit"
    - In der Taskbar soll an dem Icon ein Roter Punkt angezeigt werden
    - Implementiere das Benachrichtungssystem so ein, dass es auch für andere bereiche leicht implementierbar wäre und auch weitere nachrichten in zukunft erstellt werden können wie z.B. "Neuer Reader im Wareneingang [Seriennummer]"
