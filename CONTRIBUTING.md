# Contributing to the NOXED MPC Plugins

Thanks for wanting to help! English or German – both are welcome.

## Reporting bugs
[Open an issue](https://github.com/molgino/noxend-mpc-plugins/issues/new/choose) and include: MPC model, MPC OS version, plugin and version, connected device (for Overwitch/USB Bridge), what you expected and what happened. For Overwitch and USB Bridge a photo of the status rows (STATUS, DEVICE, STREAM …) or the report file from the plugin folder (`MPC-Overwitch-Report.txt`, `MPC-USB-Bridge-Report.txt`) helps a lot.

## Contributing code
1. Fork the repository and create a branch.
2. Build and test:
   ```sh
   python tools/build.py "plugins/<Name>"           # native + ARM32 + ARM64, including the validator
   .sdk-tools/host_sim "plugins/<Name>/build/native/<slug>.so" test
   (cd "plugins/MPC Overwitch" && sh tests/run_tests.sh)   # if Overwitch code is affected
   (cd "plugins/MPC USB Bridge" && sh tests/run_tests.sh)  # if USB Bridge code is affected (Linux, libasound)
   ```
3. Open a pull request against `main`. GitHub Actions builds all plugins and runs the tests. Say in the PR which MPC (if any) you tested on.

The project is vibecoded (built with an AI coding assistant and tested on hardware), so careful reviews of existing code are just as welcome as new features.

## Rules that keep the plugins safe on the MPC
- **Real time:** no memory allocation, no locks, no logging, no file or USB access in `process()`.
- **Identity is permanent:** never change `uid`, parameter IDs or parameter order; only add new parameters at the end. Otherwise users' projects break.
- **MPC Overwitch and Send share one engine** (`plugins/MPC Overwitch/src/ob_shared.h`): if you change the shared layout, increase `kAbi`.
- **Skins:** `python tools/validate.py "plugins/<Name>"` must run without errors. On the MPC, buttons placed directly in a tab do not respond, and fixed-text labels placed directly in a tab are not drawn; the validator warns about both.
- **No third-party files:** no code from the Steinberg VST SDK, nothing from MPC firmware, no manufacturer graphics. No changes to the MPC system.
- **Licence:** contributions to `plugins/MPC Overwitch`, `plugins/MPC Overwitch Send` and `plugins/MPC USB Bridge` are GPL-3.0-or-later; all other NOXED parts are MIT, and contributions to `framework/`, `tools/`, `templates/`, `docs/` are MIT (see below). By opening a pull request you agree to this.

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
