# M5 PTT — BLE & USB MIDI Footswitch

A push-to-talk (PTT) MIDI controller for M5Stack **AtomS3 Lite** (ESP32-S3).
Pressing a footswitch or the built-in button sends a MIDI note over either
Bluetooth LE MIDI or native USB MIDI. Designed for use with **SmartSDR on iOS**
as a wireless/wired PTT.

## How it works

Two operating modes, selected at boot:

- **Voice mode** (default): the footswitch or built-in button is push-to-talk.
- **CW mode**: hold the built-in button while powering on — the LED flashes
  **R** (·−·) to confirm. The built-in button then triggers CWX keyer memories
  by click count (footswitch is ignored).

Power-cycle **without** holding the button to return to voice mode.

The device presents as `M5PTT` over both BLE and USB.

### MIDI notes (channel 1)

All actions are momentary note pulses; map them in SmartSDR's Mapping Editor.

| Action | Note | Map to |
|---|---|---|
| Voice PTT | 99 | PTT (Note On press / Note Off release) |
| CW single click | 100 | CWX macro 1 |
| CW double click | 101 | CWX macro 2 |
| CW triple click | 102 | CWX macro 3 |
| CW long press | 103 | CWX stop / abort |

Single-click resolves after a ~350 ms window (needed to distinguish double/triple
click); a long press (~600 ms) fires the abort note.

### Transport selection (automatic, mutually exclusive)

The active transport is chosen automatically so the host never receives
duplicate notes over two links:

| Power / connection | Active transport | Bluetooth |
|---|---|---|
| Plugged into a USB **host** (computer / iPad) | **USB MIDI** | dropped, advertising off |
| Powered from a **charger / power bank** (no data host) | **BLE MIDI** | advertising on |

When a USB host enumerates the device it becomes the sole transport: any BLE
client is disconnected and advertising stops. Unplug from the host and it falls
back to BLE and resumes advertising.

> **Testing note:** while cabled to a computer for flashing/serial, the device
> is in USB mode and BLE is off. To test the BLE path, power it from a plain
> USB charger or battery (not a data host).

## Hardware

| Function | Pin | Notes |
|---|---|---|
| External footswitch | GPIO1 | Grove port, G1 (yellow wire), `INPUT_PULLUP`, active-low |
| Built-in button | GPIO41 | active-low |
| Status LED | GPIO35 | 1× WS2812C RGB |
| USB | USB-C | Native USB / OTG (CDC serial + USB MIDI) |

Both inputs are debounced (50 ms).

### Status LED

| Color | Meaning |
|---|---|
| Red | PTT active |
| Green | USB host connected (USB MIDI active), idle |
| Solid blue / blue blink | Voice mode: BLE connected / advertising |
| Solid magenta / magenta blink | CW mode: BLE connected / advertising |
| Morse "R" at boot | CW mode confirmed |
| N white blinks | CW memory N sent (1/2/3) |
| Long red blink | CW abort sent |

## Building & flashing

Requires [PlatformIO](https://platformio.org/).

```sh
make all      # build
make upload   # flash over USB-C
make clean    # clean build artifacts
```

Serial monitor at 115200 baud (`pio device monitor`) — outputs a `[dbg]`
status line at 1 Hz plus `[BLE]`/`[USB]` transport-change events.

### Entering download mode

Because the firmware uses **native USB (OTG) mode** for USB MIDI, esptool's
automatic reset-to-bootloader does not work. Before `make upload`, put the
board in download mode manually:

1. Press and hold the top button for ~2 s until the internal LED turns green.
2. Release, then run `make upload`.

## Implementation notes

- **USB mode:** the AtomS3 board defaults to `ARDUINO_USB_MODE=1` (hardware
  USB-Serial/JTAG); this project forces `=0` (OTG/TinyUSB) in `platformio.ini`
  so it can enumerate as a USB MIDI device.
- **BLE name:** the BLE-MIDI library advertises only the service UUID, so the
  device name is injected into the advertisement/scan response manually —
  otherwise iOS shows the peripheral UUID instead of `M5PTT`.
- **BLE connection state:** the BLE-MIDI library's connect callbacks don't fire
  under NimBLE-Arduino 2.x (changed virtual signature), so connection state is
  polled via `getConnectedCount()`.
- **Host-cached USB name:** the USB product string is `M5PTT`, but macOS/iOS
  CoreMIDI caches USB-MIDI names by VID:PID. If an old name persists, remove the
  stale device on the host.

## Power notes

Intended for USB or external power (AtomS3 Lite has no battery/PMIC). To stay
reliable with iOS BLE (which drops peripherals that miss connection events),
light sleep is **not** used. Power is instead reduced by never starting WiFi,
lowering BLE TX power to −12 dBm, shutting down BLE entirely when on USB, and
keeping the LED dim.

## License

Apache 2.0 — see [LICENSE](LICENSE).
