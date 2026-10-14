#!/usr/bin/env python3
"""
Draft new release - Windows / cross-platform Python version

Verhalten:
- Fragt nach einer neuen Version
- Sucht rekursiv nach der nächstgelegenen CMakeLists.txt (ausgehend vom Skriptordner)
- Ersetzt die VERSION innerhalb des project(...) Blocks
- Führt CMake Configure + Build (Release) aus
- Versucht, setup.iss mit Inno Setup (ISCC.exe) zu kompilieren

Aufruf: python deploy\draft_new_release.py
"""
from __future__ import annotations
import re
import sys
import subprocess
from pathlib import Path
import shutil


def find_cmake_upwards(start: Path) -> Path | None:
		p = start.resolve()
		for parent in [p] + list(p.parents):
				candidate = parent / "CMakeLists.txt"
				if candidate.exists():
						return candidate
		return None


def replace_version_in_cmake(path: Path, new_version: str) -> bool:
		text = path.read_text(encoding="utf-8")
		pattern = re.compile(r"(project\([^)]*?\bVERSION\s+)([^\s\)]+)", re.IGNORECASE | re.DOTALL)
		new_text, count = pattern.subn(lambda m: m.group(1) + new_version, text)
		if count == 0:
				return False
		# Backup
		backup = path.with_suffix(path.suffix + ".bak")
		path.write_text(new_text, encoding="utf-8")
		backup.write_text(text, encoding="utf-8")
		return True


def run(cmd: list[str], **kwargs) -> None:
		print("$", " ".join(cmd))
		subprocess.run(cmd, check=True, **kwargs)


def find_iscc() -> Path | None:
		# check common install locations
		pf = Path("C:/Program Files")
		pfx86 = Path("C:/Program Files (x86)")
		candidates = [
				pfx86 / "Inno Setup 6" / "ISCC.exe",
				pf / "Inno Setup 6" / "ISCC.exe",
		]
		for c in candidates:
				if c.exists():
						return c
		# fallback to PATH
		exe = shutil.which("ISCC.exe") or shutil.which("iscc")
		if exe:
				return Path(exe)
		return None


def main() -> int:
		start = Path(__file__).parent
		cmake_file = find_cmake_upwards(start)
		if cmake_file is None:
				print("Fehler: CMakeLists.txt nicht gefunden (ausgehend vom Skriptordner).", file=sys.stderr)
				return 1

		new_ver = input("Neue Version (z.B. 1.5.0): ").strip()
		if not new_ver:
				print("Abbruch: keine Version angegeben.")
				return 1

		print(f"Setze Version {new_ver} in '{cmake_file}'")
		ok = replace_version_in_cmake(cmake_file, new_ver)
		if not ok:
				print("Warnung: Projekt-VERSION konnte nicht gefunden/ersetzt werden.", file=sys.stderr)
		else:
				print(f"Backup: {cmake_file}.bak erstellt")

		src_dir = cmake_file.parent
		build_dir = src_dir / "build"
		build_dir.mkdir(exist_ok=True)

		try:
				print("Starte CMake-Konfiguration...")
				run(["cmake", "-S", str(src_dir), "-B", str(build_dir), "-DCMAKE_BUILD_TYPE=Release"])
				print("Baue Projekt (Release)...")
				run(["cmake", "--build", str(build_dir), "--config", "Release"])
		except subprocess.CalledProcessError as e:
				print(f"Build fehlgeschlagen: {e}", file=sys.stderr)
				return e.returncode if isinstance(e.returncode, int) else 1

		# Bestimme den Inno-Setup Output-Ordner (OutputDir in setup.iss / setup_template.iss)
		def parse_output_dir(iss_path: Path) -> Path | None:
				try:
						txt = iss_path.read_text(encoding="utf-8")
				except Exception:
						return None
				m = re.search(r"^\s*OutputDir\s*=\s*(.+)$", txt, flags=re.IGNORECASE | re.MULTILINE)
				if not m:
						return None
				val = m.group(1).strip().strip('"')
				# Resolve relative to the iss file location
				p = Path(val)
				if not p.is_absolute():
						return (iss_path.parent / p).resolve()
				return p

		iss = src_dir / "setup.iss"
		installer_output_dir: Path | None = None
		if iss.exists():
				installer_output_dir = parse_output_dir(iss)
		if installer_output_dir is None:
				template = src_dir / "setup_template.iss"
				if template.exists():
						installer_output_dir = parse_output_dir(template)
		if installer_output_dir is None:
				# Fallback wie in Vorlage: "./output"
				installer_output_dir = (src_dir / "output").resolve()

		# Stelle sicher, dass der Output-Ordner existiert
		try:
				installer_output_dir.mkdir(parents=True, exist_ok=True)
		except Exception as e:
				print(f"Warnung: Konnte Output-Ordner nicht erstellen: {e}", file=sys.stderr)

		version_file = installer_output_dir / "VERSION"
		changes_file = installer_output_dir / "CHANGES"

		try:
				version_file.write_text(new_ver + "\n", encoding="utf-8")
				print(f"Geschriebene VERSION: {version_file}")
				changes_content = f"# v{new_ver}\n\n## Features\n\n### Added\n\n### Removed\n\n## Changes\n\n## Bugs\n"
				changes_file.write_text(changes_content, encoding="utf-8")
				print(f"Geschriebene CHANGES: {changes_file}")
		except Exception as e:
				print(f"Fehler beim Schreiben von VERSION/CHANGES: {e}", file=sys.stderr)

		iss = src_dir / "setup.iss"
		if iss.exists():
				iscc = find_iscc()
				if iscc:
						try:
								print(f"Gefundener Inno Setup Compiler: {iscc}")
								run([str(iscc), str(iss)])
						except subprocess.CalledProcessError as e:
								print(f"Inno Setup Kompilierung fehlgeschlagen: {e}", file=sys.stderr)
								return e.returncode if isinstance(e.returncode, int) else 1
				else:
						print("Inno Setup Compiler (ISCC.exe) nicht gefunden. Bitte installieren oder PATH anpassen.")
		else:
				print(f"setup.iss nicht gefunden in {src_dir}. Skipping Inno Setup.")

		print("Fertig.")
		return 0


if __name__ == "__main__":
		raise SystemExit(main())
