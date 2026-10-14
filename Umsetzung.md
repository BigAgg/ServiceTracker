# ServiceTracker

Wir wollen ein gut entwickeltes, gut durchdachtes system verwenden. Hauptfokus liegt in der benutzung und stabilität sowie erweiterbarkeit von neuen modulen.

Dafür soll das bestehende Programm neu geschrieben werden.
## Dev Workflow

* Ask questions only when absolutely necessary - work autonomously
* Testing is of the utmost importance - use lots of unit tests (any framework, simple is good), run automated testing
* Do things properly, this is production-grade and not a hack project
* Erstelle AGENTS.md und Relevante Skills um die Entwicklung weiter zu unterstützen, und bleibe bei gewissen guidelines und entwicklung
* Im aktuell bestehenden Programm wird eine von mir geschriebene framework verwendet: https://github.com/BigAgg/cpp_app
* Das Programm soll optisch auch flexibel sein wie das alte mit den docking fenstern
* Es ist wichtig, dass alte Dateistrukturen weiterhin verwendbar sind 
* Wording im Code auf englisch aber dokumentationen auf deutsch

## Struktur

C++ für Frontend und Backend.
cmake als Build-System.
Postgres als Datenbank.
Die Projekte sind in Frontend und Backend unterteilt. Beide haben ihr eigenes Repo auf welchem commits stattfinden.

Lizenzmodell, 50€ im monat als abomodell monatlich kündbar

## Features

* Artikel-Stammdaten

  * Es sollen auch baugruppen angelegt werden können, sodass z.B. für die Montage einer Steckdose direkt 0,4h und die teile für die Steckdose hinterlegt werden können
  * dateien sollen abgelegt werden können wie z.b. datenblätter, bilder etc.
* Nutzerverwaltung

  * Nur lesen
  * Admin
  * Einstellen auf welche Abschnitte ein Nutzer zugriff hat
  * Neue Nutzer anlegen
  * Passwörter zurücksetzen
  * Passwörter ändern
* Kundenstammdaten
* Bestellwesen
* Lieferantenstammdaten
* Angebote/Aufträge/Rechnungen
* PI soll vom frontend eigenständig im Netzwerk gesucht werden
* Automatischer Updater welcher später über einen Server läuft

Lege besonders viel wert auf dokumentation, sodass das aufsetzen und die buildprozesse ordentlich dokumentiert sind.
Codeblöcke nur dokumentieren, wenn nötig.
Frage nur nach, wenn etwas wirklich unklar ist.
Das projekt soll falls möglich auf windows compilen können und falls nicht dann auf einem pi compiled.



