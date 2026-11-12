# NOXED MPC Plugins

Zwei eigenständige Instrumente für Akai MPC Standalone, gebaut mit dem
[inMusic MPC Plugin Builder SDK](https://github.com/inmusic-software/mpc-plugin-builder-sdk).

**Version 0.1.0: am Computer gebaut und getestet, noch nicht auf echter MPC-Hardware geprüft.**

## Instrumente

| Instrument | Funktionen |
| --- | --- |
| **Acidline** | Monophoner, TB-303-inspirierter Acid-Synth mit Saw/Square, nichtlinearem resonantem Filter, Velocity-Accent, Legato-Slide, Drive und sechs Presets. |
| **Prism Wave** | Eigenständiger, Serum-inspirierter Wavetable-Synth mit acht Stimmen, zwei Oszillatoren, je bis zu drei Unison-Stimmen, Sub/Noise, Filter, zwei ADSRs, zwei LFOs, vier Modulationsrouten, Chorus, Delay und acht Presets. |

Keine originalgetreue TB-303-Schaltungsemulation und keine vollständige Serum-2-Nachbildung.
Es sind keine proprietären Roland-/Xfer-Codebestandteile, Samples oder Presets enthalten.
Prism Wave hat keinen Serum-Presetimport, Sample-/Granular-Modus, MPE oder grafischen Wavetable-Editor.

## Download und Installation

Die fertigen Pakete und Hörbeispiele stehen unter **[Releases](../../releases)**.
Das kombinierte Paket heißt `NOXED-MPC-Plugins-0.1.0.zip`.

1. ZIP am Computer entpacken.
2. Den enthaltenen Ordner `VST2 Plugins` ins Stammverzeichnis eines MPC-Datenträgers kopieren.
   Bei einem bestehenden Ordner nur die beiden NOXED-Unterordner ergänzen; deren Namen unverändert lassen.
3. **MPC OS 3.10.1 oder neuer** verwenden und unter Preferences → Enhancements (Beta)
   **Enable Enhancements** und **External Plugin Support** einschalten.
4. Bei Bedarf MPC neu starten und das Instrument unter **External Plugins** auswählen.

ARM32- und ARM64-Binaries liegen bei; die MPC Live III verwendet ARM64.
Die Release-Binaries benötigen glibc ≥ 2.38; laut SDK verwendet MPC OS 3 glibc 2.39.

## Wavetable-Import

Prism Wave bietet acht User-Slots für WAV-Wavetables. User 01 enthält eine originale
NOXED-Demotabelle. Auf Windows 64 Bit kann das beiliegende `import_wavetable.exe`
ohne Python gestartet werden; für andere Systeme liegt `import_wavetable.py` bei.

Der Import wird **am Computer vorbereitet**, nicht über einen Dateidialog auf der MPC.
Er unterstützt zyklische Mono-/Stereo-PCM- oder Float32-WAVs mit bekannter Frame-Länge.
Das Werkzeug erzeugt 2048 Samples pro Frame und höchstens 64 Frames. Bei größeren
Tabellen bis 1024 Frames werden 64 gleichmäßig verteilte Frames ausgewählt.
Nach dem Kopieren alle Plugin-Instanzen neu laden.

**Projekte speichern Slotnummern, keine WAV-Audiodaten.** Die Content-Dateien mitsichern;
das Ersetzen eines Slots verändert den Klang bestehender Projekte.

Details: [Prism-Wave-Anleitung](plugins/Prism%20Wave/README.md) ·
[Acidline-Anleitung](plugins/Acidline/README.md) · [Installation](docs/NOXED-Anleitung.txt).

## Teststand

- Je Instrument 19 SDK-Hostprüfungen: keine Warnungen oder Fehler.
- 33 zusätzliche Audio-/Funktionstests, einschließlich aller Presets, Accent/Slide,
  Wavetable-Import und -Morphing, Modulationsmatrix, Sustain und Unison-Stresstest.
- Python-Importer: vier Tests; Windows-Importer: Import, Überschreibschutz,
  explizites Ersetzen und Ablehnung nicht endlicher Audiodaten geprüft.
- ARM32/ARM64-Paketvalidator: keine Fehler; je zwei Kompatibilitätshinweise zu glibc ≥ 2.38.

[Testzusammenfassung](tests/reports/summary.json) und vollständige Logs liegen bei.
Hardware-CPU, Ladezeit, Touch/Q-Link-Bedienung und Projektwiederherstellung müssen auf
der MPC geprüft werden. Plugins laufen ohne Crashschutz in der MPC-Anwendung.

## Aus dem Quellcode bauen

Python 3.9+; der SDK-Builder lädt den Zig-Crosscompiler beim ersten Build herunter:

```sh
python tools/build.py plugins/Acidline
python tools/build.py "plugins/Prism Wave"
```

Anschließend mit dem SDK-Hostsimulator testen. Siehe [Build-Hinweise](NOXED-BUILD.md),
[SDK-Dokumentation](SDK-README.md) und [Testanleitung](docs/install-and-test.md).
Die Windows-Release-Builds wurden alternativ mit Arm GNU 15.2 und LLVM-MinGW erstellt;
`tools/build_noxed_windows.py` unterstützt diese vorkompilierten Toolchains.

## Lizenz und Herkunft

Originaler NOXED-Code und Demo-Wavetable: [MIT](NOXED-LICENSE.txt).
Das mitgelieferte SDK basiert auf inMusic-Revision `febd2ca` vom 7. November 2026,
ebenfalls [MIT](LICENSE). Runtime-Lizenzhinweise sind den Release-Paketen beigefügt.
Roland/TB-303 und Serum/Xfer sind Marken ihrer jeweiligen Inhaber. Keine Verbindung
zu oder Freigabe durch diese Hersteller; Drittanbieter-Plugins sind von inMusic nicht unterstützt.
