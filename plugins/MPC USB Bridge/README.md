# NOXED MPC USB Bridge 0.2.0 – USB-Audiogerät als Plugin-Spur (Roland S-1 & Co.)

Holt ein class-compliant USB-Audiogerät, das die MPC nicht als Interface verwendet (zuerst der **Roland AIRA Compact S-1**, ebenso T-8, J-6 und ähnliche), als Instrument-Plugin-Spur in die MPC. Das Plugin nimmt das Gerät über die ALSA-Bibliothek der MPC selbst auf (`libasound`, zur Laufzeit geladen) und rechnet den Ton mit demselben Resampler mit Drift-Ausgleich wie MPC Overwitch auf die Rate der MPC um.

**Einrichten (S-1):** AIRA LINK am S-1 **aus**, S-1 per USB an die MPC, Plugin „MPC USB Bridge“ auf eine Spur laden. Nach etwa einer Sekunde steht STATUS auf **STREAMING**.

| Bedienung | Bedeutung |
|---|---|
| LEVEL (Q-Link 13) | Ausgangspegel |
| CARD (Q-Link 14) | **Auto** = erstes USB-Audiogerät, das nicht die MPC ist (Roland bevorzugt), oder eine feste ALSA-Karte |
| REPORT (Q-Link 15) | blättert durch den USB-/ALSA-Bericht (wie 0.1) |

| Zeile | Bedeutung |
|---|---|
| STATUS | STREAMING / Streaming (buffering) / Waiting / ERROR mit Grund |
| DEVICE | ALSA-Name und USB-ID des Geräts |
| STREAM | Format, Rate des Geräts (eingestellt und gemessen) → Rate der MPC, Periodengröße |
| LEVELS | Spitzenpegel, Latenz, Drift in ppm, Aussetzer, Überläufe |
| SYSTEM | Prozessor, Benutzer-ID, Kernel |

**Was geht, was nicht (0.2):**
- Nur **Aufnahme** vom Gerät in die MPC (Stereo; ein Mono-Gerät wird auf beide Seiten gelegt). Ton von der MPC zum Gerät gibt es noch nicht.
- Ein Gerät kann nur von **einer** Plugin-Instanz gleichzeitig geöffnet werden; eine zweite Instanz zeigt „ERROR: open: Device or resource busy“.
- Abziehen und wieder Einstecken funktioniert: Das Plugin sucht danach selbst weiter.
- Latenz etwa 5–15 ms. CPU gering: ein Aufnahme-Thread plus Resampler.
- **Parameteränderung gegenüber 0.1:** LEVEL und CARD sind neu, die Reihenfolge ist anders. Eine Spur mit der 0.1-Testversion bitte neu laden. Ab 0.2 bleiben IDs und Reihenfolge fest.
- Nicht auf der MPC getestet, nur am Computer (siehe unten).

## Bericht aus 0.1 (weiterhin enthalten)

Mit REPORT blätterst du durch alle USB-Geräte mit Schnittstellen und Treibern, alle ALSA-Karten mit Formaten, Kanälen und Raten, die Belegung der Audiogeräte und ob die ALSA-Bibliothek im MPC-Prozess verfügbar ist. Der vollständige Bericht steht in `MPC-USB-Bridge-Report.txt` im Plugin-Ordner (alle 10 s aktualisiert).

## So testest du (S-1)

1. Am S-1 **AIRA LINK auf OFF** stellen: [SHIFT] + Pad [15] (MENU), „A.Lnk“ wählen, OFF. So steht es im Roland-Handbuch für den Anschluss an Computer und andere Geräte.
2. S-1 per USB-C an einen USB-Host-Port der MPC anschließen.
3. MPC USB Bridge als Plugin-Programm laden, Seite **USB** ansehen.
4. Bonus: AIRA LINK auf ON stellen, neu anstecken und nochmals schauen. Der Bericht zeigt dann, wie sich der S-1 in diesem Modus meldet.

## Urteile

| STATUS | Bedeutung |
|---|---|
| No USB audio device found | Kein USB-Audiogerät als Soundkarte erkannt (Kabel, AIRA LINK, Strom) |
| Roland without audio: AIRA LINK on? | Roland-Gerät da, aber ohne Audio-Schnittstelle |
| Seen, but cannot open audio | Karte da, aber das Plugin darf sie nicht öffnen (Rechte) |
| Busy: card in use | Die Karte wird schon benutzt (z. B. von der MPC selbst) |
| PROMISING: audio can be opened | Bester Fall: Das Brücken-Plugin ist sehr wahrscheinlich machbar |

## AIRA LINK

AIRA LINK ist ein nicht offengelegtes Roland-Protokoll für Audio und Tempo über USB zwischen Roland-Geräten (Hosts sind z. B. der MX-1-Mixer und der DJ-808). Es wird nicht verwendet: Ton und MIDI laufen im normalen USB-Modus (AIRA LINK OFF), das Tempo kommt per MIDI-Clock von der MPC.

## Ergebnis auf der MPC Live III (15.11.2026)

Roland S-1 mit AIRA LINK aus: eigene Soundkarte „S1“, Aufnahme und Wiedergabe je 2 Kanäle S32_LE mit **44,1 kHz** (nicht 48 kHz), beide lassen sich vom Plugin öffnen; libasound ist im MPC-Prozess bereits geladen. STATUS: „PROMISING: audio can be opened“. Damit ist das Brücken-Plugin mit Ton machbar. Weil der S-1 44,1 kHz anbietet, kann die MPC ihn eventuell auch direkt als Audio-Interface nutzen; das Plugin bleibt nützlich, weil die MPC nur ein Interface gleichzeitig verwendet.

## Tests am Computer

`sh tests/run_tests.sh` (Linux mit libasound): Die Aufnahme-Engine nimmt ein ALSA-„file“-Gerät auf, das einen 1-kHz-Testton mit 44,1 kHz / S32 liefert, und liest ihn in Echtzeit mit 48 kHz aus. Geprüft werden Pegel links/rechts, Frequenz und Aussetzer (Ergebnis: 1000,2 Hz, 0 Aussetzer, ~10 ms Latenz). Läuft auch in GitHub Actions.


Mit nachgebautem `/proc/asound` (Variable `USB_BRIDGE_ASOUND`) und einer S-1-Beispielbeschreibung geprüft; SDK-Hosttest bestanden (die Warnung zu Stille ist erwartet). Auf der MPC noch nicht getestet.

Lizenz: GPL-3.0-or-later.

Voraussetzungen wie bei allen NOXED-Plugins: MPC OS 3.10.1 oder neuer, und unter Preferences → Enhancements (Beta) müssen „Enable Enhancements“ und „External Plugin Support“ an sein.
