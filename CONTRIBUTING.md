# Mitmachen bei den NOXED MPC Plugins

Danke, dass du helfen willst! Deutsch oder Englisch – beides ist willkommen.

## Fehler melden
[Issue anlegen](https://github.com/molgino/noxed-mpc-plugins/issues/new/choose) und angeben: MPC-Modell, MPC-OS-Version, Plugin und Version, angeschlossenes Gerät (bei Overwitch/USB Bridge), was du erwartet hast und was passiert ist. Bei Overwitch und USB Bridge hilft ein Foto der Statuszeilen (STATUS, DEVICE, STREAM …) oder die Berichtsdatei aus dem Plugin-Ordner (`MPC-Overwitch-Report.txt`, `MPC-USB-Bridge-Report.txt`).

## Code beitragen
1. Fork anlegen, einen Branch erstellen.
2. Bauen und testen:
   ```sh
   python tools/build.py "plugins/<Name>"           # native + ARM32 + ARM64, inkl. Validator
   .sdk-tools/host_sim "plugins/<Name>/build/native/<slug>.so" test
   (cd "plugins/MPC Overwitch" && sh tests/run_tests.sh)   # falls Overwitch-Code betroffen
   (cd "plugins/MPC USB Bridge" && sh tests/run_tests.sh)  # falls USB-Bridge-Code betroffen (Linux, libasound)
   ```
3. Pull Request gegen `main`. GitHub Actions baut alle Plugins und führt die Tests aus. Schreib in den PR, auf welcher MPC (falls überhaupt) du getestet hast.

## Regeln, die die Plugins auf der MPC heil halten
- **Echtzeit:** in `process()` keine Speicheranforderung, keine Sperren, keine Ausgaben, keine Datei- oder USB-Zugriffe.
- **Identität ist dauerhaft:** `uid`, Parameter-IDs und Parameter-Reihenfolge nie ändern; neue Parameter nur hinten anfügen. Sonst gehen Projekte der Nutzer kaputt.
- **MPC Overwitch und Send teilen sich eine Engine** (`plugins/MPC Overwitch/src/ob_shared.h`): wer das gemeinsame Layout ändert, erhöht `kAbi`.
- **Skins:** `python tools/validate.py "plugins/<Name>"` muss ohne Fehler laufen. Auf der MPC reagieren Knöpfe direkt in einem Tab nicht, und feste Textbeschriftungen direkt in einem Tab werden nicht gezeichnet; der Validator warnt davor.
- **Keine fremden Dateien:** kein Code aus dem Steinberg-VST-SDK, nichts aus MPC-Firmware, keine Herstellergrafiken. Keine Änderungen am MPC-System.
- **Lizenz:** Beiträge zu `plugins/MPC Overwitch`, `plugins/MPC Overwitch Send` und `plugins/MPC USB Bridge` stehen unter GPL-3.0-or-later, alle anderen NOXED-Teile unter MIT, Beiträge zu `framework/`, `tools/`, `templates/`, `docs/` unter MIT (siehe unten). Mit dem Pull Request stimmst du dem zu.

---

# SDK contribution rules (inMusic MPC Plugin Builder SDK)

# Contributing

Thanks for helping. A few ground rules keep the SDK safe to redistribute:

- **No Steinberg VST SDK code**, ever, in any form. The ABI header is a clean-room description; keep it that way.
- **No files extracted from MPC firmware** (skins, images, fonts, binaries, settings files). Describe formats; do not copy artefacts.
- **No MPC system modifications.** The SDK targets the official plugin folder and preference; pull requests that depend on root access, patched binaries or custom firmware will be declined.
- **MIT only** for anything added to `framework/`, `tools/`, `templates/` and `docs/`. Examples may use other OSI licences if clearly marked, but prefer MIT so people can reuse them freely.
- **Keep it simple for an AI agent.** Docs are read by coding agents; be exact about names, versions and paths. Tools are stdlib Python with friendly error messages.

Workflow: fork, branch, run `python tools/build.py examples/SimpleSynth` (all three examples should build and validate), open a pull request describing what changed and on which MPC you tested.

Reporting problems: open an issue with your MPC model, firmware version, the output of `python tools/validate.py` and the host simulator report.
