# Änderungen

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
