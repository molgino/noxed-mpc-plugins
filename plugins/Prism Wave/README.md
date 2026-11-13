# NOXED Prism Wave 0.1.1

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

## Oberfläche (ab 0.1.1)

Eigener dunkler Skin im Stil moderner Software-Wavetable-Synths: Panels mit Kopfzeile, Bogen-Regler mit Cyan/Violett-Verlauf, Auswahlfelder und Anzeigen. Alle Grafiken sind neu gezeichnet; keine Logos, Grafiken oder Schriften von Xfer, Vital oder anderen Herstellern. Fünf Seiten:

- **OSC:** OSC A und OSC B nebeneinander. Kopfzeile mit Wavetable-Auswahl; darunter eine 3D-Wavetable-Anzeige, die der **Position** folgt (wischen ändert die Position). Regler Position, Octave, Level; darunter Unison, Detune, Sub, Noise (A) bzw. Semi, Unison, Detune, Width (B).
- **FILTER:** Filterkurve, die **Cutoff** und Modus (LP/BP/HP/NOTCH) folgt, Cutoff, Resonance, Env 2 Amt; VOICE mit Key Track, Bend Range, Master; Distortion (Drive), Chorus (Mix), Delay (Mix, Division, Feedback).
- **ENV:** ENV 1 (Amplitude) und ENV 2 (Modulation) mit Kurvenanzeige (folgt dem Sustain-Pegel; die Zeiten stehen an den Reglern) und je vier Reglern A/D/S/R.
- **LFO:** Pro LFO Formanzeige, Shape-Auswahl, Rate (frei) bzw. Division (bei SYNC an) an derselben Stelle und SYNC-Schalter.
- **MATRIX:** Vier Zeilen Quelle → bipolarer Amount-Balken → Ziel.

Der Master-Regler steht zusätzlich oben rechts auf jeder Seite. Auswahlfelder öffnen die Liste per Doppeltipp oder Encoder-Druck; Antippen eines Elements schaltet die passende Q-Link-Reihe ein. Q-Links arbeiten in Reihen zu vier (oberste Hardware-Reihe = obere Reglergruppe). Im Program-Modus liegen A/B Position, Cutoff, Resonance, Env 2 Amt, Drive, Chorus, Delay, A/B Level, Sub, Noise und die Amp-ADSR auf den 16 Q-Links.

Die Anzeigen für User 02..08 zeigen eine neutrale Ebenen-Darstellung, weil importierte Tabellen erst auf der MPC geladen werden. User 01 zeigt die mitgelieferte Demotabelle; wer sie ersetzt, kann mit `tools/make_prismwave_skin.py` die Anzeige neu erzeugen.

Die Anzeige-Grafiken sind Filmstreifen mit 21 Stufen; die Klangposition selbst bleibt stufenlos. Ladezeit und Speicherbedarf des größeren Skins sind noch auf der MPC zu prüfen.

Die Skin-Dateien liegen in `skins/` und werden vom Build unverändert übernommen. Neu erzeugen (Pillow und numpy nötig):

```text
python tools/make_prismwave_skin.py --params "plugins/Prism Wave/build/params.json"
```

`ui.json` bleibt als einfache Rückfallvariante erhalten, wird aber nicht verwendet, solange `skins/` existiert.

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

Version 0.1.1 ändert nur die Oberfläche; Klang, Plugin-ID, Parameterreihenfolge und Presets sind unverändert (Hosttest-Audio bytegleich mit 0.1.0). Nach dem Ersetzen des Plugin-Ordners die MPC neu starten. Der separate Testbericht dokumentiert Computerpruefungen. Der neue Skin ist noch nicht auf echter MPC getestet: Darstellung, Touch/Q-Link-Bedienung, CPU, Ladezeit, Bedienung und Projektwiederherstellung dort pruefen. Fuer viele Instanzen Unison reduzieren. Plugins laufen ohne Crashschutz; Projekte vor dem ersten Test sichern.

## Lizenz

Originaler NOXED-Code und Demo-Wavetable: MIT. Nutzt das MIT-lizenzierte inMusic MPC Plugin Builder SDK; dessen LICENSE liegt im Quellpaket. Serum/Xfer gehoeren ihren jeweiligen Inhabern; keine Verbindung oder Freigabe.
