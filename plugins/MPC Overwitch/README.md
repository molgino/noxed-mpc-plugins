# NOXED MPC Overwitch 0.3.0 – mehrere Spuren

MPC Overwitch holt den Ton eines Elektron-Geräts per **Overbridge** direkt in die MPC. Zuerst getestet wird der Digitone II, die Tabelle enthält auch Digitakt II, Digitakt, Digitone (Keys), Syntakt, Analog Four/Rytm MKII und Analog Heat. Das Gerät liefert 48 kHz; das Plugin rechnet auf die Samplerate der MPC um (44,1 kHz) und gleicht dabei die kleine Taktabweichung zwischen beiden Geräten laufend aus. Damit fällt das Samplerate-Problem von USB-Audio weg.

Grundlage ist das Open-Source-Projekt [Overwitch](https://github.com/dagargo/overwitch) von David García Goñi. Sein USB-Protokoll (`engine.c`, MIT, Stefan Rehm und David García Goñi) wurde für die MPC neu umgesetzt, die Gerätetabelle ist aus Overwitchs `devices.json` (GPL-3) erzeugt (`tools/gen_overwitch_devices.py`). Code, Grafiken oder Software von Elektron werden nicht verwendet. Lizenz dieses Plugins: GPL-3.0-or-later.

**Stand:** Version 0.2.0 läuft auf der MPC Live III mit dem Digitone II (13.11.2026): STREAMING, gemessen 48,00 kHz, etwa 8 ms Puffer, Drift-Ausgleich bei wenigen ppm, 0 Aussetzer, 0 USB-Fehler, USB-Thread mit Echtzeit-Priorität; der Ton kommt sauber auf der Plugin-Spur an. Längere Sessions, CPU-Last und die anderen Elektron-Geräte sind noch nicht geprüft.

## Bedienung

- **SOURCE** (Doppeltipp öffnet die Liste): welcher Ausgang des Geräts gespielt wird – Main, Track 1–16, Delay, Reverb, Chorus, Input (Namen des Digitone II; andere Geräte zeigen ihre eigenen Ausgänge, nicht vorhandene Einträge bleiben still).
- **LEVEL:** Ausgangspegel (Standard −1,9 dB, Maximum +1 dB).
- **STATUS:** Searching… → Connecting… → Streaming (buffering) → **STREAMING**. Weitere Meldungen: „Set device USB: Overbridge“, „Unsupported Elektron device“, „Error: …“, „Busy: other instance has it“.
- **STREAM:** gemessene Geräte-Samplerate, Puffer-Latenz und Taktabweichung in ppm.
- **DROPOUTS:** Aussetzer im Puffer und USB-Fehler seit dem Laden.
- **REPORT:** Details zum Durchblättern, auch in `MPC-Overwitch-Report.txt` im Plugin-Ordner.

Mehrere Spuren: für jede gewünschte Geräte-Spur eine eigene Plugin-Spur mit MPC Overwitch anlegen und dort SOURCE wählen (bis zu 16 zusammen mit Send-Instanzen).

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
- **Audio zum Gerät (ab 0.3):** mit dem Effekt-Plugin **MPC Overwitch Send** (eigener Plugin-Ordner). Ohne Send-Instanz geht Stille zum Gerät.
- Abziehen des Kabels oder Wechsel des USB-Modus: Das Plugin wird still und verbindet sich automatisch neu.
- **Mehrere Instanzen (ab 0.3):** Bis zu 16 Instanzen von MPC Overwitch und MPC Overwitch Send teilen sich eine USB-Verbindung. Jede Instanz hat ihre eigene SOURCE, z. B. Track 1 auf Plugin-Spur 1, Track 2 auf Plugin-Spur 2. Die Bibliotheken finden sich über eine kleine Ankerdatei in `/dev/shm`, sind gegen Entladen gesichert, und unterschiedliche Versionen beider Plugins verweigern das Teilen mit „version mismatch“.
- CPU: Umrechnung für zwei Kanäle plus Dekodierung; auf der MPC noch zu messen, erwartet deutlich unter einem Prozent eines Kerns.

## Tests am Computer

- `tests/sim_stream.cpp`: simulierter USB-Strom mit Taktabweichung −150…+200 ppm, Jitter bis 2,5 ms, MPC-Blockgrößen 32–512, Host-Raten 44,1 und 48 kHz: keine Aussetzer, saubere Sinuswiedergabe.
- `tests/sim_stream.cpp` prüft auch die Senderichtung (MPC-Blöcke → USB-Transfers bei 48 kHz mit Drift).
- `tests/engine_smoke.cpp`: Gerätetabelle, Blockgrößen (Digitone II: 1012/256 Bytes), Quellen und Ziele, 24-Bit-Dekodierung, gemeinsame Engine mit 16 Slots, Fehlerpfad ohne Gerät.
- `tests/two_libs.cpp`: zwei getrennt geladene Plugin-Bibliotheken teilen eine Engine, auch wenn die erzeugende Bibliothek entladen wird.
- Alles mit `sh tests/run_tests.sh`; läuft auch in GitHub Actions.
- SDK-Hosttest: bestanden. Ohne Gerät ist die Ausgabe still; die Warnung dazu ist erwartet.

## Verlauf

- **0.3.0:** Bis zu 16 Instanzen gleichzeitig mit eigener Quelle; gemeinsame USB-Verbindung mit dem neuen Effekt MPC Overwitch Send (Audio zum Gerät). Noch nicht auf Hardware getestet.
- **0.2.1:** Die STREAM-Zeile aktualisiert sich, sobald die erste Ratenmessung vorliegt (vorher blieb „0.00 kHz“ stehen).
- **0.2.0:** Overbridge-Streaming vom Gerät zur MPC, Quellenwahl, Pegel, Drift-Ausgleich. Auf der MPC Live III mit Digitone II erfolgreich getestet.
- **0.1.1:** Statuszeilen aktualisieren sich selbst.
- **0.1.0:** USB-Test. Ergebnis auf der MPC Live III mit Digitone II im Overbridge-Modus (13.11.2026): Gerät gefunden (1935:0b34), Overbridge-Schnittstelle frei, Zugriff als Root, Claim erfolgreich.

Die Parameter von 0.x können sich bis 1.0 noch ändern.

Voraussetzungen wie bei allen NOXED-Plugins: MPC OS 3.10.1 oder neuer, und unter Preferences → Enhancements (Beta) müssen „Enable Enhancements“ und „External Plugin Support“ an sein.
