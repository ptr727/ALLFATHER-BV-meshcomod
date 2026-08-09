# Elecrow ThinkNode M7

The M7 is an ESP32-S3 + LR1110 board with a 100 Mbps RJ45 (WCH CH390 SPI Ethernet
controller) and optional PoE. `variants/thinknode_m7/` and `boards/thinknode_m7.json`
arrived with the upstream 1.17.0 merge, but the board is not in meshcomod's release matrix,
so there is no prebuilt binary and no web-flasher entry (see [CI releases](CI_RELEASES.md)).
It has to be built from source.

## Environments

`*_companion_radio_usb_tcp` (meshcomod's USB + TCP + WebSocket companion) does not exist for
this board, so these are upstream's environments.

| Env | Transports | Notes |
|-----|-----------|-------|
| `ThinkNode_M7_companion_radio_ethernet` | USB + Ethernet | The PoE wired node. Companion TCP on port 5000 over the wire, USB for config. |
| `ThinkNode_M7_companion_radio_ble` | BLE | Ethernet flags are set but unused, since BLE takes precedence. See "Remaining gaps". |
| `ThinkNode_M7_companion_radio_usb` | USB | Simplest bring-up target. |
| `ThinkNode_M7_companion_radio_wifi` | Wi-Fi (TCP 5000) | SSID/password hardcoded in the variant's `platformio.ini`. |
| `ThinkNode_M7_kiss_modem` | USB | KISS TNC. |
| `ThinkNode_M7_repeater` | none | **Does not build.** See "Remaining gaps". |

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
| `out/<env>-<version>-<sha>.bin` | `0x10000` | app only, keeps partition table and SPIFFS |

Flash with [esptool-js](https://espressif.github.io/esptool-js/) in Chrome, or:

```bash
esptool --chip esp32s3 --port <PORT> --baud 921600 write-flash 0x0 out/<file>-merged.bin
```

The board enumerates as a WCH CH343 bridge (`1a86:7522`), not Espressif native USB, so `<PORT>`
is typically `/dev/ttyUSB0` on Linux (ch341 driver) or `COMx` on Windows, not `/dev/ttyACM0`.
Note also that
`boards/thinknode_m7.json` still declares `hwids` of `0x303A:0x1001`, which breaks PlatformIO's
automatic port detection. Serial console is 115200. There is no reset button, and the auto-reset
circuit works, so pulse DTR/RTS (or power cycle) to reset.

A merged image spans `0x0` to `~0x131000`, which covers NVS at `0x9000` but **not** SPIFFS at
`0x670000`. Node preferences live in `/prefs.json` in SPIFFS, so radio settings survive
reflashing.

## Radio region

`[arduino_base]` in `platformio.ini` compiles in EU defaults (869.618 MHz / 62.5 kHz / SF8).
These seed the factory defaults only: companion firmware copies them into `_prefs` on first
boot and uses stored preferences thereafter, and a client app will typically overwrite them
during setup. For the headless roles (repeater, room server, KISS modem) there is no app, so
the compiled values are the operating configuration. Set them explicitly when building those
for a non-EU region.

## Hardware validation

Verified on a real ThinkNode M7 (this is the first known instance of meshcomod running on the
board):

- Boots, reporting `[BOOT] board ok` and `[BOOT] radio ok`, so the LR1110 initializes.
- BLE companion pairs from the MeshCore client.
- USB companion connects from the web client.
- Ethernet: link, DHCP lease, `ETH: listening on TCP port: 5000`, USB live simultaneously.
- Home Assistant connects over the Ethernet TCP transport and reads telemetry.
- Both LEDs work. Green shows the firmware heartbeat, a 20 ms blip every 4 s that lengthens to
  200 ms while messages are unread. Blue follows LoRa transmit.
- The compiled radio defaults seed a fresh device. After the store was cleared the node came up
  on 927.875 MHz, 62.5 kHz, SF7, CR5 with no client involvement.
- DHCP-to-DNS registration works. A rename propagated to DNS with no measurable delay, on the
  same MAC and the same lease.

## Remaining gaps

- **`ThinkNode_M7_repeater` does not compile.** `examples/simple_repeater/UITask.cpp` uses
  `DisplayDriver::BLUE` and `DisplayDriver::LIGHT`, but that `enum Color` is commented out at
  `src/helpers/ui/DisplayDriver.h:20`, superseded by the `UIColor` statics. This breaks the
  plain `*_repeater` envs on **every** board, reproduced identically on
  `Station_G3_ESP32_repeater`, so it is not M7-specific and wants its own fix.
- **`ThinkNode_M7_companion_radio_ble` has unused Ethernet.** The env sets both `BLE_PIN_CODE`
  and the CH390 flags, but the companion registers only BLE. `MultiSerialInterface` supports
  registering BLE, USB and Ethernet together, and wiring that up would give the board every
  transport at once.
- **`examples/companion_radio/ui-tiny/UITask.h`** still declares its constructor as taking
  `MultiSerialInterface*`, which nothing constructs on nRF52. `LilyGo_T-Echo_Card_companion_radio_ble`
  and `_usb` carry the same break that this change fixes for `ui-orig`.
- **`boards/thinknode_m7.json` `hwids`** do not match the hardware (see above).
- **The ethernet MAC is locally administered.** The CH390 driver invents `E2:72:A1:F1:FE:49`
  rather than reading efuse, where ESPHome on the same board reports `E0:72:A1:F1:FE:4B`. This
  was investigated as the cause of a missing DNS record and is not: the record was minted with
  the locally administered address in place. Aligning it should not be attempted in a release,
  because a new address on upgrade is an unfamiliar device to any network that quarantines
  those, which would drop deployed nodes off DNS.
- **No battery reading.** `ESP32Board::getBattMilliVolts()` returns 0 unless `PIN_VBAT_READ` is
  defined, and the M7 variant does not define it, so clients render 0 percent and 0.000 V. That is
  accurate for a PoE gateway carrying no battery, but a client cannot tell it apart from a flat one.
- **The meshcomod web client mis-parses the device info frame, and the offset is known.** Byte 2
  of `RESP_CODE_DEVICE_INFO` is `MAX_CONTACTS / 2`, 175, which is invalid UTF-8, and byte 3 is
  `MAX_GROUP_CHANNELS`, 40. Those are what the mojibake and the stray bracket are. Bytes 8 to 19
  hold the build date and bytes 20 on the manufacturer name, which is why they appear run
  together. The client starts reading at offset 2 rather than offset 8, skipping the two byte
  header but neither the two v3+ count bytes nor the four byte BLE pin, and does not stop at the
  null separators. The same client reads `Device model` and `Firmware` correctly from that frame,
  and Home Assistant renders it correctly, so the frame is well formed and `StrHelper::strzcpy`
  pads every field. The fix belongs in that client.
