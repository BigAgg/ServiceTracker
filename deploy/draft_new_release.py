#!/usr/bin/env python3
"""
Neues Release erstellen (Windows).

Ablauf:
  1. fragt nach der neuen Version (z.B. 2.1.0)
  2. setzt die VERSION im project(...)-Block der CMakeLists.txt
  3. konfiguriert und baut mit dem CMake-Preset "windows" (Release)
  4. führt alle Tests aus – schlägt ein Test fehl, wird abgebrochen
  5. schreibt VERSION und eine CHANGES-Vorlage in den Output-Ordner
  6. erstellt den Installer mit Inno Setup (ISCC.exe), falls vorhanden

Aufruf (im Projektordner):  python deploy\\draft_new_release.py
Siehe docs/release.md.
"""
from __future__ import annotations

import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CMAKE_FILE = ROOT / "CMakeLists.txt"
ISS_FILE = ROOT / "installer" / "setup.iss"
OUTPUT_DIR = ROOT / "output"


def run(cmd: list[str]) -> None:
    print("$", " ".join(cmd))
    subprocess.run(cmd, check=True, cwd=ROOT)


def set_version(version: str) -> bool:
    text = CMAKE_FILE.read_text(encoding="utf-8")
    pattern = re.compile(r"(project\([^)]*?\bVERSION\s+)([^\s\)]+)", re.IGNORECASE | re.DOTALL)
    new_text, count = pattern.subn(lambda m: m.group(1) + version, text)
    if count == 0:
        return False
    CMAKE_FILE.with_suffix(".txt.bak").write_text(text, encoding="utf-8")
    CMAKE_FILE.write_text(new_text, encoding="utf-8")
    return True


def find_iscc() -> Path | None:
    for base in (Path("C:/Program Files (x86)"), Path("C:/Program Files")):
        candidate = base / "Inno Setup 6" / "ISCC.exe"
        if candidate.exists():
            return candidate
    exe = shutil.which("ISCC.exe") or shutil.which("iscc")
    return Path(exe) if exe else None


def main() -> int:
    version = input("Neue Version (z.B. 2.1.0): ").strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        print("Abbruch: Version muss das Format X.Y.Z haben.")
        return 1

    if not set_version(version):
        print("Fehler: VERSION im project()-Block nicht gefunden.", file=sys.stderr)
        return 1
    print(f"Version {version} gesetzt (Backup: CMakeLists.txt.bak)")

    try:
        run(["cmake", "--preset", "windows"])
        run(["cmake", "--build", "--preset", "windows-release"])
        run(["ctest", "--preset", "windows"])
    except subprocess.CalledProcessError as e:
        print(f"Build oder Tests fehlgeschlagen: {e}", file=sys.stderr)
        return 1

    OUTPUT_DIR.mkdir(exist_ok=True)
    (OUTPUT_DIR / "VERSION").write_text(version + "\n", encoding="utf-8")
    (OUTPUT_DIR / "CHANGES").write_text(
        f"# v{version}\n\n## Features\n\n### Added\n\n### Removed\n\n## Changes\n\n## Bugs\n", encoding="utf-8")
    print(f"VERSION und CHANGES geschrieben: {OUTPUT_DIR}")

    iscc = find_iscc()
    if not iscc:
        print("Inno Setup (ISCC.exe) nicht gefunden – Installer wird nicht erstellt.")
        return 0
    try:
        run([str(iscc), str(ISS_FILE)])
    except subprocess.CalledProcessError as e:
        print(f"Installer konnte nicht erstellt werden: {e}", file=sys.stderr)
        return 1
    print("Fertig. Installer liegt in", OUTPUT_DIR)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
