# Author-reported hardware tests

On 17 November 2026, the author reported that all five plugins in release
[v0.2.3](https://github.com/molgino/noxed-mpc-plugins/releases/tag/v0.2.3)
had been tested on an **MPC Live III (Gen2), MPC OS 3.10.1**.

| Catalog ID | Tested plugin version |
| --- | --- |
| `noxed-acidline` | 0.3.0 |
| `noxed-prism-wave` | 0.1.4 |
| `noxed-mpc-overwitch` | 0.5.1 |
| `noxed-mpc-overwitch-send` | 0.5.1 |
| `noxed-mpc-usb-bridge` | 0.2.2 |

These records refer to the published v0.2.3 binaries and their source commit
`40832b26596b1c0228eb97e0dc81c257547a2d1a`, not to the older plugin versions
still on the default branch. They are encoded in root `tested.json` for the
Open MPC catalog. The author supplied the device and MPC OS version directly.

No Gen1 test or measured CPU benchmark is claimed. A successful hardware test
does not establish compatibility with every external USB device or exact
equivalence to the hardware emulated by a sound model.

The additional Open MPC packages for Acidline and MPC USB Bridge are archive
conversions of the existing release: their shared libraries remain byte-for-byte
identical to those in the original SDK packages. Both architectures are checked
with Open MPC's `catalog_check.py --catalog --require-portable`.
