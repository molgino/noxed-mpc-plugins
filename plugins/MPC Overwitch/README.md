# NOXED MPC Overwitch 0.2.0 – erster Ton (Vorschau)

MPC Overwitch holt den Ton eines Elektron-Geräts per **Overbridge** direkt in die MPC. Zuerst getestet wird der Digitone II, die Tabelle enthält auch Digitakt II, Digitakt, Digitone (Keys), Syntakt, Analog Four/Rytm MKII und Analog Heat. Das Gerät liefert 48 kHz; das Plugin rechnet auf die Samplerate der MPC um (44,1 kHz) und gleicht dabei die kleine Taktabweichung zwischen beiden Geräten laufend aus. Damit fällt das Samplerate-Problem von USB-Audio weg.

Grundlage ist das Open-Source-Projekt [Overwitch](https://github.com/dagargo/overwitch) von David García Goñi. Sein USB-Protokoll (`engine.c`, MIT, Stefan Rehm und David García Goñi) wurde für die MPC neu umgesetzt, die Gerätetabelle ist aus Overwitchs `devices.json` (GPL-3) erzeugt (`tools/gen_overwitch_devices.py`). Code, Grafiken oder Software von Elektron werden nicht verwendet. Lizenz dieses Plugins: GPL-3.0-or-later.

**Stand:** Version 0.2 ist auf der MPC noch nicht mit Ton getestet. Der USB-Zugriff selbst ist auf der Live III bestätigt (siehe Verlauf).

## Bedienung

- **SOURCE** (Doppeltipp öffnet die Liste): welcher Ausgang des Geräts gespielt wird – Main, Track 1–16, Delay, Reverb, Chorus, Input (Namen des Digitone II; andere Geräte zeigen ihre eigenen Ausgänge, nicht vorhandene Einträge bleiben still).
- **LEVEL:** Ausgangspegel (Standard −1,9 dB, Maximum +1 dB).
- **STATUS:** Searching… → Connecting… → Streaming (buffering) → **STREAMING**. Weitere Meldungen: „Set device USB: Overbridge“, „Unsupported Elektron device“, „Error: …“, „Busy: other instance has it“.
- **STREAM:** gemessene Geräte-Samplerate, Puffer-Latenz und Taktabweichung in ppm.
- **DROPOUTS:** Aussetzer im Puffer und USB-Fehler seit dem Laden.
- **REPORT:** Details zum Durchblättern, auch in `MPC-Overwitch-Report.txt` im Plugin-Ordner.

Q-Links: erste Reihe Source und Level, zweite Reihe Report. Im Program-Modus Q-Link 1 Level, Q-Link 2 Source.

## So testest du

1. Am Digitone II die USB-Konfiguration auf **Overbridge** stellen und per USB an die MPC hängen.
2. MPC Overwitch als Plugin-Programm laden. STATUS sollte nach ein bis zwei Sekunden **STREAMING** zeigen.
3. Am Digitone spielen; der Ton kommt auf der Plugin-Spur. SOURCE umschalten, um einzelne Spuren zu hören.
4. STATUS, STREAM und DROPOUTS notieren (oder ein Foto machen), auch wenn es knackt oder still bleibt.

## Technik

- Eine eigene Engine spricht direkt mit der Linux-USB-Schnittstelle der MPC (`/dev/bus/usb`, usbfs); es wird keine zusätzliche Bibliothek gebraucht. Interfaces 1 und 2, Alt-Setting 3, Interrupt-Transfers mit 24 Blöcken à 7 Frames.
- Die USB-Arbeit läuft in einem eigenen Thread (wenn möglich mit Echtzeit-Priorität), nie im Audio-Thread. Der Audio-Thread liest nur einen lock-freien Ringpuffer.
- Resampler: 4-Punkt-Hermite-Interpolation mit adaptivem Verhältnis. Er hält den Puffer auf etwa 6–17 ms (je nach MPC-Blockgröße) und korrigiert höchstens ±0,2 % (unhörbar). Geht der Puffer leer, wird kurz stummgeschaltet, neu gefüllt und der Sicherheitsabstand vergrößert.
- Zum Gerät wird bisher nur Stille gesendet (Audio von der MPC zum Digitone kommt später).
- Abziehen des Kabels oder Wechsel des USB-Modus: Das Plugin wird still und verbindet sich automatisch neu.
- Nur eine Instanz kann das Gerät belegen; weitere zeigen „Busy“. Mehrere Spuren gleichzeitig (eine Instanz pro Spur) sind für eine spätere Version geplant.
- CPU: Umrechnung für zwei Kanäle plus Dekodierung; auf der MPC noch zu messen, erwartet deutlich unter einem Prozent eines Kerns.

## Tests am Computer

- `tests/sim_stream.cpp`: simulierter USB-Strom mit Taktabweichung −150…+200 ppm, Jitter bis 2,5 ms, MPC-Blockgrößen 32–512, Host-Raten 44,1 und 48 kHz: keine Aussetzer, saubere Sinuswiedergabe.
- `tests/engine_smoke.cpp`: Gerätetabelle, Blockgrößen (Digitone II: 1012/256 Bytes), Quellen-Zuordnung, 24-Bit-Dekodierung, Fehlerpfad ohne Gerät.
- SDK-Hosttest: bestanden. Ohne Gerät ist die Ausgabe still; die Warnung dazu ist erwartet.

## Verlauf

- **0.2.0:** Overbridge-Streaming vom Gerät zur MPC, Quellenwahl, Pegel, Drift-Ausgleich.
- **0.1.1:** Statuszeilen aktualisieren sich selbst.
- **0.1.0:** USB-Test. Ergebnis auf der MPC Live III mit Digitone II im Overbridge-Modus (13.11.2026): Gerät gefunden (1935:0b34), Overbridge-Schnittstelle frei, Zugriff als Root, Claim erfolgreich.

Die Parameter von 0.x können sich bis 1.0 noch ändern.

Voraussetzungen wie bei allen NOXED-Plugins: MPC OS 3.10.1 oder neuer, und unter Preferences → Enhancements (Beta) müssen „Enable Enhancements“ und „External Plugin Support“ an sein.
