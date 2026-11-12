# NOXED Acidline 0.1.0

Monophoner Acid-Synth fuer Akai MPC Standalone. Eigenstaendige TB-303-inspirierte Klangerzeugung; keine originalgetreue Schaltungsemulation, keine Roland-Software oder Original-Samples.

## Spielen

- Die Noten kommen vom MPC-Sequencer, Pads oder Keyboard. Kein eingebauter 303-Step-Sequencer.
- Velocity ab **Accent Vel** (Standard 100) aktiviert Accent auf einer neu angeschlagenen Note. Accent hebt VCA und Filterhuellkurve an.
- Fuer **Slide** die MIDI-Noten ueberlappen lassen und **Legato Slide** einschalten. Die Huellkurven werden dabei nicht erneut gestartet; Accent bleibt vom vorherigen Anschlag erhalten.
- Nach Loslassen der neuesten Note kehrt das Instrument zur zuletzt noch gehaltenen Note zurueck. Pitchbend ist einstellbar. Kein Sustain-Pedal.

## Bedienung

**ACID:** Cutoff, Resonance, Env Amount, Decay, Accent, Slide Time, Drive, Tune, Master, Waveform (Saw/Square), Legato Slide.

**SETUP:** Pulse Width, Accent Vel, Release, Key Track, Bend Range.

Signalweg: PolyBLEP-Oszillator -> nichtlinearer resonanter Dreipol-Tiefpass (2x Abtastrate) -> VCA/Accent -> Drive/DC-Filter -> Master. Sechs eingebaute Presets: NOXED Acid, Rubber Square, Warehouse, Clean Sub, Sharp Screamer, Liquid.

## Installation

MPC OS **3.10.1 oder neuer**. Ordner **NOXED - VST - Acidline** unveraendert nach **VST2 Plugins** im Stammverzeichnis des Datentraegers kopieren. Unter Preferences > Enhancements (Beta) **Enable Enhancements** und **External Plugin Support** einschalten. Instrument unter External Plugins laden. Enthalten sind ARM32 und ARM64; MPC Live III verwendet ARM64.

## Stand und Grenzen

Version 0.1.0 ist eine erste Implementierung. Host-Simulator- und Funktionstests werden im mitgelieferten Testbericht ausgewiesen. Klangtreue zur TB-303, Hardware-CPU und Funktion auf der echten MPC sind noch nicht verifiziert. Hohe Resonanz ist absichtlich markant. Erst mit niedrigem Pegel auf der MPC testen; ein Pluginfehler kann die MPC-Anwendung beenden. Projekt speichern und neu laden, bevor du das Instrument produktiv nutzt.

## Quellcode und Lizenz

Originaler NOXED-Code, MIT. Nutzt das MIT-lizenzierte inMusic MPC Plugin Builder SDK (separate LICENSE im Quellpaket). Roland und TB-303 sind Marken ihrer jeweiligen Inhaber; keine Verbindung oder Freigabe.
