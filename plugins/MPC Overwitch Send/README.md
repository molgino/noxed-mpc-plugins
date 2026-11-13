# NOXED MPC Overwitch Send 0.3.1 – MPC-Audio zum Elektron-Gerät (Vorschau)

Effekt-Plugin zu [MPC Overwitch](../MPC%20Overwitch/README.md). Als Insert auf einer beliebigen MPC-Spur schickt es deren Audio per Overbridge an einen Eingang des Elektron-Geräts. Beim Digitone II sind das Main, Delay, Reverb oder Chorus, also auch die Effekt-Sends des Digitone. Die Umrechnung von der MPC-Rate auf 48 kHz folgt dabei der Taktabweichung, genau wie beim Empfang.

Send und MPC Overwitch teilen sich eine USB-Verbindung: Beide Plugins können gleichzeitig laufen, zusammen bis zu 16 Instanzen. Mehrere Sends auf dieselbe DESTINATION werden addiert.

**Stand:** Noch nicht auf der MPC getestet. Bitte **beide Plugins in derselben Version** installieren; sonst meldet STATUS „version mismatch: update both plugins“.

## Bedienung

- **DESTINATION** (Doppeltipp): Eingang am Gerät – Main, Delay, Reverb, Chorus (Digitone II). Andere Overbridge-Geräte zeigen ihre eigenen Eingänge.
- **SEND:** Pegel zum Gerät (Standard −1,9 dB).
- **DRY:** was auf der MPC-Spur bleibt (Standard 0 dB). Ganz zu, wenn die Spur nur noch über das Gerät klingen soll.
- **STATUS / DEVICE / STREAM:** Verbindung, Gerät, Puffer-Latenz und Aussetzer der Senderichtung.
- **LEVELS:** `IN` = Spitzenpegel, der am Plugin ankommt (nach SEND); `USB` = was die Engine tatsächlich zum Gerät mischt. Steht IN auf „silent“, liegt auf der Spur kein Signal an. Steht IN auf einem Pegel und USB auf „silent“, liegt das Problem in der Engine. Zeigen beide einen Pegel und am Gerät kommt nichts an, liegt es am Gerät (Routing/Einstellung) oder an der Kodierung.

Q-Links: Destination, Send, Dry.

## So testest du

1. Digitone II auf Overbridge, per USB an der MPC.
2. MPC Overwitch Send als Insert auf eine Spur mit Material legen, DESTINATION z. B. **Reverb**.
3. Den Effekt hören: über eine Plugin-Spur mit MPC Overwitch (SOURCE **Main** oder **Reverb**) oder über die Audioausgänge des Digitone.
4. Achtung Rückkopplung: Lege MPC Overwitch Send nicht auf eine Spur, auf der MPC Overwitch läuft. Der zurückgeholte Ton würde wieder zum Gerät gehen, und das ergibt eine Schleife.

## Technik

- Der Audio-Thread schreibt nur in einen lock-freien Ringpuffer. Die gemeinsame USB-Engine (ein Thread pro MPC-Prozess) liest ihn mit 48 kHz, gleicht Drift aus, mischt alle Sends und kodiert die Overbridge-Blöcke (24-Bit auf Overbridge 2.1).
- Die Senderichtung ist in der Simulation geprüft: keine Aussetzer bei −150…+120 ppm, MPC-Blockgrößen 32–512 und Timing-Schwankungen bis 1,5 ms. Die Latenz liegt je nach Blockgröße bei etwa 6–18 ms.
- Code teilt sich mit MPC Overwitch (`plugins/MPC Overwitch/src/ob_*.h`). Protokoll nach [Overwitch](https://github.com/dagargo/overwitch) (`engine.c`, MIT), Gerätetabelle aus Overwitch (GPL-3).

Lizenz: GPL-3.0-or-later.

Voraussetzungen wie bei allen NOXED-Plugins: MPC OS 3.10.1 oder neuer, und unter Preferences → Enhancements (Beta) müssen „Enable Enhancements“ und „External Plugin Support“ an sein.
