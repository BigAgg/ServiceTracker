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
    - Ohne Verbindung zum Netzlaufwerk (beim Kunden) startet das Programm offline: eigene Berechtigungen aus dem lokalen Stand, Geräte mit lokaler Kopie, Arbeit nur lokal
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

## Neuimplementierungen:

- Sectionen / arbeitsgänge in checklisten sollen die option bekommen, ob sie auf einem e-mail export auftauchen dürfen oder nicht.
    - E-Mail export siehe unteren Punkt "Ein neuer Button "E-Mail erstellen""
- Bei "Record erstellen" soll es eine option geben "In englisch übersetzen"
    - Das Übersetzen soll mit einer lokalen instanz von libretranslate funktionieren
    - falls möglich, soll der record direkt als pdf erstellt werden mit kopf und fußzeile mit seitenzahlen
- Ein neuer Button "E-Mail erstellen"
    - Der Button erstellt einen von deutsch auf englisch übersetzten record ohne die bearbeitungszeiten und ohne die namen der bearbeiter als pdf.
    - Der Button öffnet per mailto: eine vordefinierte e-mail
        - als anhang wird der zuvor erstellte record gesetzt und aus dem aktuellen Service odner (Service_yyyy.mm.dd) die PDF datei im Unterordner "EV-Plate". Dabei ist wichtig, dass im namen der Datei "Reader Evaluation" enthalten ist.
        - Ist die Datei nicht vorhanden, wird sie einfach nicht mit angehangen.
- Übersetzer vorgaben:
    - Libretranslate lokal laufend
    - Alle übersetzten sätze werden in einer cache datei für alle zugänglich gespeichert.
        - Wenn der satz dort schon übersetzt wurde, nimm diese übersetzung. somit kann ich händisch änderungen vornehmen falls notwendig.
