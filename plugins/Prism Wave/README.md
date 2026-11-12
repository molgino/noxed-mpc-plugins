# NOXED Prism Wave 0.1.0

Eigenstaendiger Wavetable-Synth fuer Akai MPC Standalone, inspiriert vom Workflow moderner Wavetable-Synthesizer. Keine vollstaendige Serum-2-Nachbildung. Kein Xfer-Code, keine mitgelieferten Serum-Wavetables, -Samples oder -Presets.

## Klangerzeugung

- Acht polyphone Stimmen, je zwei Wavetable-Oszillatoren mit jeweils 1..3 Unison-Stimmen, Detune, Oktave, Pegel und Frame-Morphing.
- Vier eigene Grundbaenke: Basic Morph, Spectral FM, Formants, Digital; acht WAV-Importslots.
- Frame- und Phaseninterpolation; zehn FFT-gefilterte Mip-Stufen begrenzen hohe Harmonische. Keine Garantie voellig aliasfreier Ausgabe bei allen Modulationen.
- Sinus-Sub, weisses Rauschen, Stereo-SVF mit LP/BP/HP/Notch und Keytracking.
- Amp-ADSR und Env2, zwei globale LFOs mit Tempo-Sync, vier Modulationsrouten.
- Modulationsquellen: LFO1, LFO2, Env2, Velocity, Modwheel, Channel Aftertouch. Ziele: A/B Position, Cutoff, Pitch, Pan, Level. Amount ist bipolar. Env2 moduliert zusaetzlich den Filter ueber Env>Filter.
- Drive, Stereo-Chorus und temposynchrones Ping-Pong-Delay. Kein Reverb, kein Sample-/Granular-Player, kein grafischer Wavetable-Editor, kein MPE und kein Serum-Presetimport.
- Sustain-Pedal und Pitchbend. Acht eingebaute Programme einschliesslich **User 01 Scan**.

## Bedienseiten

**OSC A / OSC B:** Table, Position, Level, Octave, Unison, Detune; B Semi, Width, Master, Sub Level und Noise Level. **TONE FX:** Cutoff, Resonance, Env>Filter, Key Track, Filter Mode, Drive, Chorus Mix, Delay Mix, Delay Div, Feedback und Bend Range. **ENVS:** beide ADSRs. **LFOS:** Shape, Rate, Sync und Division fuer beide LFOs. **MODS:** vier Source/Target/Amount-Gruppen. Sechs Seiten insgesamt.

## Eigene Wavetables importieren

Der Import wird **am Computer vorbereitet**; auf der MPC gibt es keinen Dateiauswahldialog. Das Plugin liest Slots beim Erzeugen einer Instanz ein. Bestehende Instanzen nach Dateiaenderungen entfernen und neu hinzufuegen, bei Bedarf MPC neu starten.

1. Eine WAV-Wavetable bereitstellen, deren hintereinander angeordnete Frames jeweils einen Wellenzyklus enthalten. Typischerweise 2048 Samples je Frame. Normale Songs oder One-Shot-Samples werden nicht automatisch in Zyklen zerlegt.
2. Unter **Windows 64 Bit** die beiliegende **import_wavetable.exe** doppelklicken. Das Werkzeug fragt nach WAV-Pfad, Slot, Frame-Laenge und dem entpackten Plugin-Ordner auf dem Datentraeger. Dafuer ist keine Python-Installation noetig. Alternativ im Terminal (auch skriptbar):

```text
import_wavetable.exe "MeineTabelle.wav" --slot 2 --frame-size 2048 --plugin-folder "E:/VST2 Plugins/NOXED - VST - Prism Wave"
```

Auf macOS/Linux oder mit Python 3.9+ das plattformuebergreifende Werkzeug verwenden:

```text
python import_wavetable.py "MeineTabelle.wav" --slot 2 --frame-size 2048 --plugin-folder "E:/VST2 Plugins/NOXED - VST - Prism Wave"
```

Der Laufwerksbuchstabe ist ein Beispiel. Der Zielpfad muss auf den entpackten Plugin-Ordner zeigen, nicht auf VST/ oder Content/. Bei bereits belegten Slots ist zum Ersetzen zusaetzlich **--replace** erforderlich.

3. Den Plugin-Ordner auf der MPC laden bzw. neu laden. In **A Table** oder **B Table** **User 02** auswaehlen und **Position** bewegen.

Akzeptierte Eingaben: Mono/Stereo PCM-WAV (8/16/24/32 Bit) oder Float32-WAV. Stereo wird mono gemischt. Die Frame-Laenge ist mit --frame-size anzugeben (32..16384); keine automatische Erkennung. Das Werkzeug entfernt DC, normalisiert pro Frame und resampelt auf 2048 Samples. Bis 64 Frames bleiben erhalten; bei 65..1024 Eingabeframes werden 64 gleichmaessig verteilte Frames ausgewaehlt. Einzelzyklus-WAVs funktionieren mit ihrer entsprechenden Frame-Laenge. WAV-Wavetables aus anderen Synthesizern funktionieren nur, wenn sie dieses zyklische Format verwenden und du sie verwenden darfst. Keine .SerumPreset/.fxp/.vstpreset-Dateien.

Intern entstehen **Content/Wavetables/user01.wav** bis **user08.wav**. User 01 enthaelt eine originale NOXED-Demotabelle; User 02..08 sind anfangs leer. Fehlende oder ungueltige Slots spielen Basic Morph; die Parameteranzeige meldet **Basic fallback**. Importierte Dateien werden beim Laden in bandbegrenzte Tabellen umgerechnet. Das kostet Ladezeit und bei acht voll belegten Slots ungefaehr 40 MB Tabellen-RAM je Instanz, zuzüglich Arbeitsdaten.

**Projekte speichern die Slot-Nummer und Position, nicht die WAV-Datei.** Slotdateien zusammen mit dem Projekt sichern. Spaeteres Ersetzen eines Slots aendert den Klang bestehender Projekte. Eigene Content-Dateien bei Updates erhalten. Ohne Computerwerkzeug duerfen nur fertig vorbereitete Mono-WAVs mit 2048 Samples pro Frame und hoechstens 64 Frames direkt in die Slots kopiert werden.

## Installation und Teststand

MPC OS **3.10.1 oder neuer** und unter Preferences > Enhancements (Beta) sowohl **Enable Enhancements** als auch **External Plugin Support** einschalten. **NOXED - VST - Prism Wave** unveraendert nach **VST2 Plugins** im Stammverzeichnis des Datentraegers kopieren. Unter External Plugins als Instrument laden. ARM32/ARM64 sind enthalten; die MPC Live III verwendet ARM64.

Erste Version 0.1.0. Der separate Testbericht dokumentiert Computerpruefungen. Noch kein Test auf echter MPC: CPU, Ladezeit, Bedienung und Projektwiederherstellung dort pruefen. Fuer viele Instanzen Unison reduzieren. Plugins laufen ohne Crashschutz; Projekte vor dem ersten Test sichern.

## Lizenz

Originaler NOXED-Code und Demo-Wavetable: MIT. Nutzt das MIT-lizenzierte inMusic MPC Plugin Builder SDK; dessen LICENSE liegt im Quellpaket. Serum/Xfer gehoeren ihren jeweiligen Inhabern; keine Verbindung oder Freigabe.
