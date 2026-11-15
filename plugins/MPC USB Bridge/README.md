# NOXED MPC USB Bridge 0.1.0 – Stufe 1: Test (Roland S-1 & Co.)

Ziel: Class-compliant USB-Audiogeräte, die die MPC nicht als Interface nimmt (meist weil sie nur mit 48 kHz laufen), als Plugin-Spur in die MPC holen und Ton zu ihnen schicken. Zuerst der **Roland AIRA Compact S-1**, später auch T-8, J-6, P-6, E-4 und Elektron-Geräte im normalen USB-Audio-Modus. Die Umrechnung auf 44,1 kHz soll derselbe Resampler mit Drift-Ausgleich übernehmen wie bei MPC Overwitch.

**Diese Version macht noch keinen Ton.** Sie prüft auf der MPC:

| Zeile | Bedeutung |
|---|---|
| STATUS | Gesamturteil (siehe unten) |
| DEVICE | Gefundene USB-Soundkarte (ALSA-Name), Roland bevorzugt |
| AUDIO | Angebotene Sampleraten (44,1k / 48k) |
| ACCESS | Ob das Plugin Aufnahme (`in`) und Wiedergabe (`out`) der Karte öffnen darf |
| SYSTEM | Prozessor, Benutzer-ID, Kernel |

Mit dem REPORT-Regler (erster Q-Link) blätterst du durch die Details: alle USB-Geräte mit Schnittstellen und Treibern, alle ALSA-Karten mit Formaten, Kanälen und Raten, Belegung der Audiogeräte und ob die ALSA-Bibliothek im MPC-Prozess verfügbar ist. Der vollständige Bericht steht in `MPC-USB-Bridge-Report.txt` im Plugin-Ordner auf dem MPC-Datenträger.

Es wird nur gelesen (`/sys`, `/proc/asound`) und die Audiogeräte werden kurz geöffnet und wieder geschlossen. Nichts wird eingestellt, gestreamt oder verändert.

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

## Tests am Computer

Mit nachgebautem `/proc/asound` (Variable `USB_BRIDGE_ASOUND`) und einer S-1-Beispielbeschreibung geprüft; SDK-Hosttest bestanden (die Warnung zu Stille ist erwartet). Auf der MPC noch nicht getestet.

Lizenz: GPL-3.0-or-later.

Voraussetzungen wie bei allen NOXED-Plugins: MPC OS 3.10.1 oder neuer, und unter Preferences → Enhancements (Beta) müssen „Enable Enhancements“ und „External Plugin Support“ an sein.
