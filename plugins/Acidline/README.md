# NOXED Acidline 0.1.1

Monophoner Acid-Synth fuer Akai MPC Standalone. Eigenstaendige TB-303-inspirierte Klangerzeugung; keine originalgetreue Schaltungsemulation, keine Roland-Software oder Original-Samples.

## Spielen

- Die Noten kommen vom MPC-Sequencer, Pads oder Keyboard. Kein eingebauter 303-Step-Sequencer.
- Velocity ab **Accent Vel** (Standard 100) aktiviert Accent auf einer neu angeschlagenen Note. Accent hebt VCA und Filterhuellkurve an.
- Fuer **Slide** die MIDI-Noten ueberlappen lassen und **Legato Slide** einschalten. Die Huellkurven werden dabei nicht erneut gestartet; Accent bleibt vom vorherigen Anschlag erhalten.
- Nach Loslassen der neuesten Note kehrt das Instrument zur zuletzt noch gehaltenen Note zurueck. Pitchbend ist einstellbar. Kein Sustain-Pedal.

## Bedienung

**BASSLINE:** Silbernes Hardware-Panel mit sechs klassischen Tone-Reglern: Tuning, Cut Off Freq, Resonance, Env Mod, Decay, Accent. Darunter Volume, Drive, Slide Time, Release sowie Waveform-Tasten (Saw/Square) und ein Legato-Schalter mit Status-LED. Regler und Wertfelder reagieren auf Touch und Q-Links; Doppeltippen oeffnet den grossen Werteditor.

**SETUP:** Pulse Width, Accent Velocity, Key Track, Bend Range. Release ist jetzt direkt auf der Hauptseite erreichbar.

Seit 0.1.1: eigene silberne Panelgrafik, geriffelte schwarze Filmstrip-Knoepfe, elfenbeinfarbene Taster und orange Akzente. Alle Grafiken sind neu gezeichnet; keine Herstellerbilder. Auf der Hauptseite steuert die erste Q-Link-Gruppe Tuning/Cutoff/Resonance/Env Mod, die zweite Decay/Accent/Waveform/Legato und die dritte Volume/Drive/Slide/Release. Klang, Presets, Parameterreihenfolge und Plugin-ID sind gegenueber 0.1.0 unveraendert, bestehende Projekte bleiben zugeordnet.

Die normalen Builds kopieren **skins/**; ui.json bleibt nur als einfacher Alternativentwurf erhalten. Zur Grafikregeneration: `python tools/make_acidline_skin.py --params plugins/Acidline/build/params.json` (Pillow erforderlich). Die Vorschau wird aus dem erzeugten TUI-Komponentenbaum und denselben PNGs gerendert; sie ist kein Screenshot vom Geraet.

Signalweg: PolyBLEP-Oszillator -> nichtlinearer resonanter Dreipol-Tiefpass (2x Abtastrate) -> VCA/Accent -> Drive/DC-Filter -> Master. Sechs eingebaute Presets: NOXED Acid, Rubber Square, Warehouse, Clean Sub, Sharp Screamer, Liquid.

## Installation

MPC OS **3.10.1 oder neuer**. Ordner **NOXED - VST - Acidline** unveraendert nach **VST2 Plugins** im Stammverzeichnis des Datentraegers kopieren. Unter Preferences > Enhancements (Beta) **Enable Enhancements** und **External Plugin Support** einschalten. Instrument unter External Plugins laden. Enthalten sind ARM32 und ARM64; MPC Live III verwendet ARM64.

## Stand und Grenzen

Die urspruengliche Version 0.1.0 wurde vom Auftraggeber auf seiner MPC ausprobiert. Der neue Skin 0.1.1 ist am Computer validiert; die Touch-/Q-Link-Bedienung muss noch auf der MPC geprueft werden. Host-Simulator- und Funktionstests werden im Testbericht ausgewiesen. Klangtreue zur TB-303 und Hardware-CPU sind nicht vermessen. Hohe Resonanz ist absichtlich markant. Erst mit niedrigem Pegel auf der MPC testen; ein Pluginfehler kann die MPC-Anwendung beenden. Projekt speichern und neu laden, bevor du das Instrument produktiv nutzt.

## Update von 0.1.0

Den Ordner **NOXED - VST - Acidline** durch die neue Version ersetzen. Danach die MPC neu starten, damit die geaenderten Skins neu geladen werden. Kein neuer Plugin-Name und keine neue ID; alte Projekte verwenden weiterhin Acidline. MPC OS 3.10.1+ und Enable Enhancements / External Plugin Support muessen aktiviert bleiben.

## Quellcode und Lizenz

Originaler NOXED-Code, MIT. Nutzt das MIT-lizenzierte inMusic MPC Plugin Builder SDK (separate LICENSE im Quellpaket). Roland und TB-303 sind Marken ihrer jeweiligen Inhaber; keine Verbindung oder Freigabe.
