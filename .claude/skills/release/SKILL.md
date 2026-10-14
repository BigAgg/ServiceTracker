---
name: release
description: Neue ServiceTracker-Version erstellen – Version setzen, bauen, testen, Installer erzeugen, Release vorbereiten.
---

# Release erstellen

Details: `docs/release.md`.

1. Sauberer Stand auf `main`, CI grün.
2. Unter Windows im Projektordner: `python deploy\draft_new_release.py`, Version `X.Y.Z` eingeben.
   Das Skript setzt die Version, baut (`windows-release`), testet, schreibt `output/VERSION` und
   `output/CHANGES` und erstellt mit Inno Setup `output/install_ServiceTracker.exe`.
3. `output/CHANGES` ausfüllen (Features/Changes/Bugs, deutsch).
4. `CMakeLists.txt` committen: „Version X.Y.Z“, Tag `vX.Y.Z` setzen, pushen.
5. GitHub-Release im Update-Repository (`updateUrl` in `ServiceTracker.json`, Standard
   `BigAgg/ServiceCreator_Releases`) mit dem Installer anlegen – installierte Programme
   aktualisieren sich darüber.

Nicht vergessen: Dateiformat-Änderungen in `docs/dateiformate.md` und Verhaltensänderungen in
`docs/migration.md` dokumentieren.
