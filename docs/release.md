# Release & Installer

## Ablauf

```bat
python deploy\draft_new_release.py
```

Das Skript
1. fragt die neue Version (`X.Y.Z`) ab und setzt sie in `CMakeLists.txt` (`project(... VERSION ...)`, Backup `.bak`),
2. baut mit `cmake --preset windows` / `--build --preset windows-release`,
3. führt alle Tests aus (`ctest --preset windows`) – bei Fehlern wird abgebrochen,
4. schreibt `output/VERSION` und eine `output/CHANGES`-Vorlage,
5. erstellt mit Inno Setup den Installer `output/install_ServiceTracker.exe`.

Danach `CHANGES` ausfüllen, Änderungen committen und ein GitHub-Release im Update-Repository
(`updateUrl`, Standard `BigAgg/ServiceTracker`) mit dem Installer anlegen. Die
installierten Programme finden das Update über den cpp_app-Updater.

## Installer (`installer/setup_template.iss`)

CMake erzeugt daraus beim Konfigurieren `installer/setup.iss` (Name/Version eingesetzt).

Inhalt:
- `ServiceTracker.exe` und `updater.exe` aus `build/windows/app/Release`
- `res/*` (Schrift, Icons, Anleitung)
- `deploy/imgui.ini` und `deploy/windowcontrol.ini` als Standard-Layout – **nur wenn noch nicht
  vorhanden**, damit eigene Layouts bei Updates erhalten bleiben
- `deploy/ServiceTracker.json`, falls vorhanden (nie überschrieben)

Ein installiertes „ServiceCreator“ (Vorgänger) wird beim Setup deinstalliert.

## Version im Programm

`APP_VERSION`/`ST_VERSION` werden aus `project(VERSION)` erzeugt. `ServiceTracker.exe --version`
gibt die Version in der aufrufenden Konsole aus.
