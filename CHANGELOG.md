# Änderungen

## Release 0.2.2 – UI cleanup and Acidline preview (16 Nov 2026)

- Acidline 0.2.0 preview: switchable 16-step sequencer synchronized to MPC transport, with per-step pitch, rest, accent and slide, pattern length and gate. Four silver-panel sequencer banks; parameter state is saved. Original engine remains the default for project compatibility.
- Optional Open303 sound model (MIT, Robin Schmidt), with 303 waveforms, dedicated TB filter and measured envelope mapping. Circuit-informed DSP; component-accurate TB-303 equivalence has NOT been established. Hardware sound and CPU checks remain pending.
- Acidline offline tests cover block-size invariance, stop/disable, rests, pattern state and zero allocations in the audio callback; address/undefined-behavior sanitizers pass (leak detection disabled under the sandbox).
- Overwitch 0.5.1, Send 0.5.1, Prism Wave 0.1.3 and USB Bridge 0.2.2: UI changes below; native/ARM builds and offline regressions checked, MPC touch/list confirmation pending.
- Catalog packages now cover Overwitch, Overwitch Send and Prism Wave. Acidline is published in this repository as a preview, not registered in the catalog.

## Overwitch 0.5.1 / Send 0.5.1 / Prism Wave 0.1.3 / USB Bridge 0.2.2

- All five plugin skins: transparent overlay wrappers as specified by the SDK, so the host paints value lists without an opaque panel covering them. Hardware confirmation of the reported list issue remains pending.
- Overwitch: neutral dark panel with red accents; group tracks in two rows of eight, effects/input underneath. Hide report, stream statistics, system and dropouts from the receive screen; retain parameter IDs for saved projects.
- Overwitch Send: matching visual theme. Audio routing and shared-engine ABI are unchanged.
- Native and ARM builds, validators and offline USB/MIDI/audio regressions checked; these UI changes still need MPC touch testing.
- Acidline sequencer and 303 sound-model work is a separate local test build and is excluded from this release.

## Release 0.2.1 – Open MPC catalog packages (16 Nov 2026)

- MPC Overwitch 0.5.0 is also published in the format of the [Open MPC plugin catalog](https://open-mpc.github.io): `noxed-mpc-overwitch-0.5.0-mpc-armv7.zip` (Gen1), `-mpc-aarch64.zip` (Gen2, e.g. MPC Live III) and `-mpc-sdk.zip` (one folder for MPC OS 3.10.1+). Built with Open MPC's `sdk_release.py` and checked with `catalog_check.py` in CI.
- No plugin changes; all versions as in 0.2.0.

## Release 0.2.0 – Open Source (15.11.2026)

- Alle fünf Plugins in einem Release: Acidline 0.1.1, Prism Wave 0.1.2, MPC Overwitch 0.5.0, MPC Overwitch Send 0.4.0, MPC USB Bridge 0.2.1.
- Lizenzen klar aufgeteilt: NOXED-Code MIT, die drei Brücken-Plugins GPL-3.0-or-later (eigene LICENSE-Datei im Plugin-Ordner), SDK MIT.
- Mitmach-Anleitung, Vorlagen für Fehlerberichte und Pull Requests, Spenden-Knopf (Ko-fi).
- Releases entstehen jetzt automatisch aus einem Versions-Tag (je Plugin ein ZIP plus Gesamtpaket).

## MPC Overwitch 0.5.0 / MPC USB Bridge 0.2.1 – MIDI zum Gerät (15.11.2026)

- Neuer Parameter MIDI Ch (Off / 1–16, angehängt): Noten, CCs, Pitch Bend, Aftertouch und Programmwechsel der MPC-Spur gehen an den USB-MIDI-Port des Geräts, umgestellt auf den gewählten Kanal (`ob_midi.h`: direkter ALSA-MIDI-Port, sonst ALSA-Sequencer).
- Gehaltene Noten werden bei Kanalwechsel, Transport-Stopp und Entfernen losgelassen. MIDI-Status in der Zeile DEVICE.
- Standard: Overwitch Off, USB Bridge Kanal 1. Gemeinsames Engine-Layout unverändert (Send 0.4.0 passt).
- Desktop-Test für die MIDI-Weiterleitung in CI.

## MPC USB Bridge 0.2.0 – mit Ton (15.11.2026)

- Nimmt ein class-compliant USB-Audiogerät (Roland S-1 usw.) über die ALSA-Bibliothek der MPC auf und gibt es als Plugin-Spur aus, mit Drift-Ausgleich (Resampler von MPC Overwitch).
- Neue Regler LEVEL und CARD (Auto oder feste Karte), neue Statuszeilen STREAM und LEVELS; der Bericht aus 0.1 bleibt.
- Desktop-Test mit ALSA-Testgerät in CI. Parameter-Reihenfolge gegenüber 0.1 geändert (Testversion), ab 0.2 fest.

## Prism Wave 0.1.2 / MPC Overwitch 0.4.1 – Korrekturen nach Hardwaretest (15.11.2026)

- Prism Wave FILTER: Die Modustasten LP/BP/HP/NOTCH reagierten nicht und waren unbeschriftet. Sie sind jetzt ein eigener Bedien-Baustein (wie der SYNC-Schalter), die Beschriftung ist ins Bild gezeichnet.
- MPC Overwitch GROUP: Pads schalteten nur ein, nicht wieder aus (fehlende Ein/Aus-Werteliste in der Skin), und ihre Beschriftungen fehlten. Beides behoben; Beschriftung im Bild.
- Ursache auf der MPC: Buttons direkt in einer Seite reagieren nicht, und feste Textbeschriftungen direkt in einer Seite werden nicht gezeichnet. `tools/validate.py` warnt jetzt bei beidem.
- Klang, Parameter und gemeinsames Engine-Layout unverändert; MPC Overwitch Send 0.4.0 passt weiter zu Overwitch 0.4.1.

## MPC USB Bridge 0.1.0 – Ergebnis auf der MPC Live III

- Roland S-1 (AIRA LINK aus) als eigene Soundkarte erkannt, 2 Kanäle S32_LE mit **44,1 kHz** für Aufnahme und Wiedergabe, beide lassen sich öffnen; libasound ist im MPC-Prozess geladen. Ergebnis „PROMISING: audio can be opened“.

## MPC Overwitch 0.4.0 / MPC Overwitch Send 0.4.0 – Gruppen

- Neue Seite GROUP: Track 1–16, Delay, Reverb, Chorus, Input zuschaltbar; SOURCE „Group“ summiert sie zu einem Stereosignal (je Instanz eine Gruppe). Die Gruppe steht in der SOURCE-Anzeige („Group: T1+T2“).
- Engine summiert pro Slot; Test mit synthetischem Overbridge-Transfer prüft die Summe. Gemeinsames Layout v5: beide Plugins müssen 0.4.0 sein.

## MPC USB Bridge 0.1.0 – Test (neu)

- Neues Test-Plugin für class-compliant USB-Audiogeräte (zuerst Roland S-1): zeigt USB-Geräte, ALSA-Karten mit Raten/Formaten, ob Aufnahme/Wiedergabe geöffnet werden können und ob libasound verfügbar ist; erkennt einen Roland ohne Audio-Schnittstelle (AIRA LINK an). Macht keinen Ton.

## MPC Overwitch / MPC Overwitch Send 0.3.1

- Send: neue Zeile LEVELS mit zwei Pegeln. IN zeigt, was am Plugin ankommt (nach SEND), USB zeigt, was die Engine tatsächlich zum Gerät mischt. Beides als Diagnose, weil ein Reverb-Send im ersten Test nicht ankam (auf der Spur lag dabei kein Signal an).
- Gemeinsames Layout geändert (ABI v4): Beide Plugins müssen 0.3.1 sein.

## MPC Overwitch 0.3.0 und MPC Overwitch Send 0.3.0 (neu)

- Mehrere Spuren: bis zu 16 Instanzen gleichzeitig, jede mit eigener SOURCE, über eine gemeinsame USB-Engine pro MPC-Prozess.
- Neues Effekt-Plugin **MPC Overwitch Send**: schickt das Audio einer MPC-Spur an einen Geräteeingang (Digitone II: Main, Delay, Reverb, Chorus), mit Send- und Dry-Pegel. Teilt die USB-Verbindung mit MPC Overwitch.
- Resampler für beide Richtungen; Senderichtung in der Simulation ohne Aussetzer.
- Neue Tests: gemeinsame Engine und 16 Slots, zwei getrennt geladene Bibliotheken teilen eine Engine (auch nach Entladen der erzeugenden), alles in GitHub Actions.
- Noch nicht auf MPC-Hardware getestet. Beide Plugins müssen dieselbe Version haben.

## MPC Overwitch 0.2.1

- Hardwaretest von 0.2.0 auf der MPC Live III mit Digitone II: Ton kommt sauber an, 48,00 kHz gemessen, ~8 ms Puffer, 0 Aussetzer, 0 USB-Fehler.
- Die STREAM-Zeile wird neu gezeichnet, sobald die erste Ratenmessung vorliegt.

## MPC Overwitch 0.2.0 – erster Ton (Vorschau)

- Overbridge-Streaming vom Elektron-Gerät zur MPC: eigene USB-Engine über usbfs (Protokoll nach Overwitch `engine.c`, MIT), Gerätetabelle aus Overwitch `devices.json` für alle Overbridge-2/2.1-Geräte.
- Adaptiver Resampler 48 kHz → MPC-Rate mit Drift-Ausgleich; in der Simulation ohne Aussetzer bei −150…+200 ppm, Jitter bis 2,5 ms und Blockgrößen 32–512.
- Neue Oberfläche: SOURCE (Main, Track 1–16, Delay, Reverb, Chorus, Input), LEVEL, STATUS, STREAM, DROPOUTS, REPORT. Parameter gegenüber 0.1 geändert (Vorabversion).
- Automatische Neuverbindung nach Abziehen oder Moduswechsel; nur eine Instanz belegt das Gerät.
- Noch nicht auf MPC-Hardware mit Ton getestet; Audio zum Gerät folgt später.

## MPC Overwitch 0.1.1

- Statuszeilen aktualisieren sich jetzt von selbst: Bei jeder sichtbaren Änderung bittet das Plugin die MPC um eine Neuanzeige. In 0.1.0 musste man es dafür neu öffnen.
- Hardwaretest mit 0.1.0 auf der MPC Live III: Digitone II im Overbridge-Modus erkannt, Zugriff als Root, Claim der Overbridge-Schnittstelle erfolgreich („PROMISING: claim works“).

## MPC Overwitch 0.1.0 – Stufe 1: USB-Test (neu)

- Neues Projekt: Overbridge-Client für Elektron-Geräte auf der MPC, später auf Basis von Overwitch (GPL-3). Stufe 1 macht keinen Ton und prüft nur, ob ein Plugin das Gerät per USB sehen, öffnen und belegen darf.
- Eine Seite mit STATUS, DEVICE, USB MODE, DRIVER, ACCESS, CLAIM, SYSTEM; REPORT-Regler zum Blättern; optionaler CLAIM TEST (belegen und sofort freigeben, keine Datenübertragung); Bericht `MPC-Overwitch-Report.txt` im Plugin-Ordner.
- Hosttest: 18 bestanden, 1 erwartete Warnung (Stille). Scan mit nachgebautem Digitone-II-Gerätebaum geprüft. Noch nicht auf MPC-Hardware getestet.

## Prism Wave 0.1.1 – Wavetable-Studio-Oberfläche

- Neuer dunkler Skin im Stil moderner Software-Wavetable-Synths mit fünf Seiten: OSC, FILTER, ENV, LFO, MATRIX (vorher sechs generische Rasterseiten).
- OSC A und B nebeneinander mit 3D-Wavetable-Anzeige je Tabelle, die der Position folgt; Wavetable-Auswahl in der Panel-Kopfzeile.
- Filterkurve folgt Cutoff und Modus; Modus als LP/BP/HP/NOTCH-Taster.
- ADSR-Anzeigen, LFO-Formanzeigen, Rate/Division je nach SYNC an derselben Stelle, Modulationsmatrix mit bipolaren Amount-Balken (vorher waren M2–M4 Amount unipolar dargestellt).
- Master-Regler oben rechts auf jeder Seite; Q-Links in Vierer-Reihen passend zum Layout, neues Program-Mode-Q-Link-Set.
- Alle Grafiken neu mit `tools/make_prismwave_skin.py` erzeugt; keine fremden Grafiken, Logos oder Schriften.
- Plugin-ID, Parameterreihenfolge, Presets und DSP unverändert; Hosttest-Audio bytegleich mit 0.1.0. SDK-Hosttest 19/19 bestanden, Skin-Prüfung ohne Fehler.
- Korrektur nach erstem Hardwaretest: Die Regler-Bausteine hießen intern `knob` wie der eingebaute MPC-Widget-Typ. Dadurch zeigten normale Regler (z. B. Attack) keinen Wert und bewegten ihren Parameter nicht. Alle Bausteine heißen jetzt eindeutig (`pwKnobUni`, `pwField…` usw.); `tools/validate.py` meldet solche Namenskollisionen künftig als Fehler.
- ARM32/ARM64 werden per GitHub Actions gebaut (`.github/workflows/build-plugins.yml`).

## 0.1.1 – Acidline Silver Panel

- Eigene silberne Oberfläche im Stil klassischer Bass-Line-Hardware.
- Sechs Tone-Regler in klassischer Reihenfolge: Tuning, Cut Off Freq, Resonance, Env Mod, Decay, Accent.
- Geriffelte schwarze Knöpfe mit 127 Grafikframes, Skalen, Wertfeldern und orangefarbenen Akzenten.
- Echte Waveform-Taster und Legato-Schalter mit Statusanzeige; auf die Anordnung abgestimmte Q-Link-Gruppen.
- Volume, Drive, Slide und Release auf der Hauptseite, vier zusätzliche Einstellungen auf SETUP.
- Alle Grafiken neu gezeichnet. Vorschau aus den tatsächlichen TUI-Komponenten und PNGs.
- Plugin-ID, Parameterreihenfolge, Presets und DSP bleiben erhalten. Audiovergleich mit 0.1.0 ist bytegleich.
- ARM32/ARM64 gebaut, SDK-Validator ohne Fehler, Hosttest mit 19 bestandenen Prüfungen ohne Warnungen/Fehler.
- Neuer Skin noch nicht auf MPC-Hardware geprüft. Nach dem Ersetzen des Plugin-Ordners die MPC neu starten.

## 0.1.0

Erste Versionen von NOXED Acidline und Prism Wave mit WAV-Importwerkzeugen,
MPC-Skins, Presets und Computer-Testberichten.
