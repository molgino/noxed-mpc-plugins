# NOXED MPC Overwitch 0.1.1 – Stufe 1: USB-Test

Ziel des Projekts: Elektron-Geräte (zuerst Digitone II) per Overbridge-Protokoll direkt in der MPC nutzen. Die Samplerate wird im Plugin von 48 kHz auf die 44,1 kHz der MPC umgerechnet. Grundlage ist später das Open-Source-Projekt [Overwitch](https://github.com/dagargo/overwitch) (GPL-3) von David García Goñi. Overwitch beruht auf dem Reverse Engineering von Stefan Rehm (dtdump). Code, Grafiken oder Software von Elektron werden nicht verwendet.

**Diese Version macht noch keinen Ton.** Sie prüft nur, ob die MPC einem Plugin überhaupt Zugriff auf das Gerät am USB-Port erlaubt. Davon hängt ab, ob ein echtes Overbridge-Plugin möglich ist.

## Was geprüft wird

| Zeile | Bedeutung |
|---|---|
| STATUS | Gesamturteil (siehe unten) |
| DEVICE | Gefundenes Elektron-Gerät (Hersteller-ID 1935) und Produkt-ID |
| USB MODE | `Overbridge`, wenn das Gerät eine herstellerspezifische Schnittstelle anbietet; sonst `USB audio` |
| DRIVER | Ob ein Kerneltreiber der MPC diese Schnittstelle schon belegt |
| ACCESS | Ob das Plugin den Geräteknoten `/dev/bus/usb/…` lesen und schreiben darf |
| CLAIM | Nur mit **CLAIM TEST** an: Schnittstelle kurz belegen und sofort wieder freigeben. Es werden keine Daten gesendet. |
| SYSTEM | Prozessor, Benutzer-ID, Kernelversion |

Die Prüfung läuft alle 2 Sekunden in einem eigenen Thread, nie im Audio-Thread. Ein- und Ausstecken oder ein Wechsel des USB-Modus wird also angezeigt. Mit dem REPORT-Regler (erster Q-Link) blätterst du durch den ganzen Bericht. Der komplette Text steht außerdem in `MPC-Overwitch-Report.txt` im Plugin-Ordner auf dem MPC-Datenträger. Er wird nur geschrieben, wenn sich etwas ändert.

Die MPC zeichnet Wert-Texte normalerweise nur neu, wenn sich ein Wert bewegt. Ab 0.1.1 bittet das Plugin die MPC deshalb um eine Neuanzeige (`requestDisplayUpdate`), sobald sich eine sichtbare Statuszeile ändert, zum Beispiel beim Einstecken oder Moduswechsel. In 0.1.0 musste man das Plugin dafür schließen und wieder öffnen. Der REPORT-Regler zeigt in jedem Fall den neuesten Stand.

**Erster Hardwaretest (MPC Live III, 13.11.2026):** Digitone II im Overbridge-Modus gefunden (1935:0b34), Overbridge-Schnittstelle frei, Gerät als Root les- und schreibbar, Claim und Release von Schnittstelle 3 erfolgreich. Ergebnis: „PROMISING: claim works“.

## So testest du

1. Am Digitone II die USB-Konfiguration auf **Overbridge** stellen. Das geht im System-Menü, genauer Pfad siehe Handbuch.
2. Den Digitone per USB an einen USB-Host-Port der MPC anschließen.
3. MPC Overwitch als Plugin-Programm laden. Die Seite **USB** zeigt das Ergebnis.
4. Erst wenn ACCESS „open rw OK“ meldet: **CLAIM TEST** einschalten.
5. Mir den Bericht schicken: Foto vom Bildschirm oder `MPC-Overwitch-Report.txt`.

## Urteile

| STATUS | Bedeutung |
|---|---|
| No Elektron device found | Gerät nicht am Bus (Kabel, Port, Strom) |
| Set Digitone USB: Overbridge | Gerät läuft im Class-Compliant-Modus |
| Seen, but no USB access | Plugin sieht das Gerät, darf es aber nicht öffnen (Rechte). Damit wäre der Plugin-Weg vorerst blockiert. |
| Seen, interface busy | Ein Treiber der MPC hält die Overbridge-Schnittstelle |
| Open OK - try CLAIM TEST | Öffnen klappt; CLAIM TEST einschalten |
| PROMISING: claim works | Bester Fall: Ein Overbridge-Plugin ist sehr wahrscheinlich machbar |
| Open OK, claim failed | Öffnen geht, Belegen nicht; Details im Bericht |

## Nächste Stufen

1. **Stufe 2** (nur wenn Stufe 1 positiv ist): Overwitch-Protokoll einbauen, Stereo-Main vom Digitone abspielen, 48 → 44,1 kHz mit Drift-Ausgleich.
2. **Stufe 3**: Einzelspuren als Auswahl, Audio zum Digitone senden, sauberes Verhalten beim Abziehen.

Die Parameter von 0.x können sich bis 1.0 noch ändern; Projekte mit 0.x-Versionen sind nicht für die Zukunft gedacht.

## Technik und Grenzen

- Liest nur aus `/sys` und `/proc` und öffnet den USB-Geräteknoten. Es verändert nichts am System und installiert nichts. Kein Root-Zugriff und keine Firmware-Änderung.
- Im Host-Simulator auf dem PC geprüft, mit einem nachgebauten Digitone-II-Gerätebaum (Variable `OVERWITCH_SYSFS`). Auf echter MPC-Hardware ist noch nichts getestet.
- Die SDK-Warnung „instrument produced silence“ ist hier gewollt.
- CPU: vernachlässigbar (ein Scan alle 2 s im Hintergrund).

Voraussetzungen wie bei allen NOXED-Plugins: MPC OS 3.10.1 oder neuer, und unter Preferences → Enhancements (Beta) müssen „Enable Enhancements“ und „External Plugin Support“ an sein.

Lizenz: GPL-3.0-or-later.
