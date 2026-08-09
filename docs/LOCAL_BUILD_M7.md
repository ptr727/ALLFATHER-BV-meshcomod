# Elecrow ThinkNode M7

The M7 is an ESP32-S3 + LR1110 board with a 100 Mbps RJ45 (WCH CH390 SPI Ethernet
controller) and optional PoE. `variants/thinknode_m7/` and `boards/thinknode_m7.json`
arrived with the upstream 1.17.0 merge, but the board is not in meshcomod's release matrix,
so there is no prebuilt binary and no web-flasher entry (see [CI releases](CI_RELEASES.md)) —
it has to be built from source.

## Environments

`*_companion_radio_usb_tcp` (meshcomod's USB + TCP + WebSocket companion) does not exist for
this board; these are upstream's environments.

| Env | Transports | Notes |
|-----|-----------|-------|
| `ThinkNode_M7_companion_radio_ethernet` | USB + Ethernet | The PoE wired node. Companion TCP on port 5000 over the wire, USB for config. |
| `ThinkNode_M7_companion_radio_ble` | BLE | Ethernet flags are set but unused — BLE takes precedence. See "Remaining gaps". |
| `ThinkNode_M7_companion_radio_usb` | USB | Simplest bring-up target. |
| `ThinkNode_M7_companion_radio_wifi` | Wi-Fi (TCP 5000) | SSID/password hardcoded in the variant's `platformio.ini`. |
| `ThinkNode_M7_kiss_modem` | USB | KISS TNC. |
| `ThinkNode_M7_repeater` | — | **Does not build.** See "Remaining gaps". |

## Building

```bash
export FIRMWARE_VERSION=v1.17.0
MERGE_BIN=1 sh build.sh build-firmware ThinkNode_M7_companion_radio_ethernet
```

`MERGE_BIN=1` is needed because `build.sh` only produces a merged image automatically for env
names in the release matrix, and no M7 env matches. Without it you get the app image only.

| Output | Flash at | Contents |
|--------|----------|----------|
| `out/<env>-<version>-<sha>-merged.bin` | `0x0` | bootloader + partition table + app |
| `out/<env>-<version>-<sha>.bin` | `0x10000` | app only; keeps partition table and SPIFFS |

Flash with [esptool-js](https://espressif.github.io/esptool-js/) in Chrome, or:

```bash
esptool --chip esp32s3 --port <PORT> --baud 921600 write-flash 0x0 out/<file>-merged.bin
```

The board enumerates as a WCH CH343 bridge (`1a86:7522`), not Espressif native USB, so `<PORT>`
is typically `/dev/ttyUSB0` on Linux (ch341 driver) or `COMx` on Windows — not `/dev/ttyACM0`.
Note also that
`boards/thinknode_m7.json` still declares `hwids` of `0x303A:0x1001`, which breaks PlatformIO's
automatic port detection. Serial console is 115200. There is no reset button; the auto-reset
circuit works, so pulse DTR/RTS (or power cycle) to reset.

A merged image spans `0x0`–`~0x131000`, which covers NVS at `0x9000` but **not** SPIFFS at
`0x670000`. Node preferences live in `/prefs.json` in SPIFFS, so radio settings survive
reflashing.

## Radio region

`[arduino_base]` in `platformio.ini` compiles in EU defaults (869.618 MHz / 62.5 kHz / SF8).
These seed the factory defaults only: companion firmware copies them into `_prefs` on first
boot and uses stored preferences thereafter, and a client app will typically overwrite them
during setup. For the headless roles (repeater, room server, KISS modem) there is no app, so
the compiled values are the operating configuration — set them explicitly when building those
for a non-EU region.

## Hardware validation

Verified on a real ThinkNode M7 (this is the first known instance of meshcomod running on the
board):

- Boots — `[BOOT] board ok`, `[BOOT] radio ok`, LR1110 initialises.
- BLE companion pairs from the MeshCore client.
- USB companion connects from the web client.
- Ethernet: link, DHCP lease, `ETH: listening on TCP port: 5000`, USB live simultaneously.

Not yet verified: a Home Assistant client session over the Ethernet TCP transport.

## Remaining gaps

- **`ThinkNode_M7_repeater` does not compile.** `examples/simple_repeater/UITask.cpp` uses
  `DisplayDriver::BLUE` and `DisplayDriver::LIGHT`, but that `enum Color` is commented out at
  `src/helpers/ui/DisplayDriver.h:20`, superseded by the `UIColor` statics. This breaks the
  plain `*_repeater` envs on **every** board — reproduced identically on
  `Station_G3_ESP32_repeater` — so it is not M7-specific and wants its own fix.
- **`ThinkNode_M7_companion_radio_ble` has unused Ethernet.** The env sets both `BLE_PIN_CODE`
  and the CH390 flags, but the companion registers only BLE. `MultiSerialInterface` supports
  registering BLE, USB and Ethernet together; wiring that up would give the board every
  transport at once.
- **`examples/companion_radio/ui-tiny/UITask.h`** still declares its constructor as taking
  `MultiSerialInterface*`, which nothing constructs on nRF52. `LilyGo_T-Echo_Card_companion_radio_ble`
  and `_usb` carry the same break that this change fixes for `ui-orig`.
- **`boards/thinknode_m7.json` `hwids`** do not match the hardware (see above).
