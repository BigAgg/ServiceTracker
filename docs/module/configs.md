# configs – Menü „Configs“

Projekt-Konfigurationen bearbeiten (`configs.json`). Dialog **Reader Configs** (Poweruser).

- Projekte links, rechts:
  - **Serviceordner** – werden bei jedem neuen Service angelegt.
  - **Vorlagen (Dateien kopieren)** – Quelle + Zielname mit `{reader}`/`{service}`; werden
    bei internen Services kopiert.
  - **Vorlagen für externe Services** – eigene Vorlagen (z. B. eine andere Checkliste) für
    externe Services. Leer = die normalen Vorlagen werden verwendet. „Normale Vorlagen
    übernehmen“ kopiert die Liste als Ausgangspunkt.
  - **Basis-Konfiguration** und **EEPROM-Adressregeln** (Adresse, Wert, Konfiguration,
    zusätzliche Ordner).
- „Speichern“ schreibt `configs.json`; unbekannte Schlüssel bleiben erhalten.

**Dateien:** `configs.json` (neuer optionaler Schlüssel `filesExtern`, siehe
[dateiformate.md](../dateiformate.md)).

**Code:** `app/src/modules/configs/`, Logik `core/include/st/devices/device_config.h`.
