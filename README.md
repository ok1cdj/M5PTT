# M5 PTT — BLE & USB MIDI Footswitch

A push-to-talk (PTT) MIDI controller for M5Stack **AtomS3 Lite** (ESP32-S3).
Pressing a footswitch or the built-in button sends a MIDI note over **both**
Bluetooth LE MIDI and native USB MIDI simultaneously. Designed for use with
**SmartSDR on iOS** as a wireless/wired PTT.

## How it works

- **PTT press** → `Note On` (note 99, velocity 127, channel 1)
- **PTT release** → `Note Off` (note 99, channel 1)
- Sent over BLE MIDI (device name `M5PTT`) and USB MIDI at the same time.

## Hardware

| Function | Pin | Notes |
|---|---|---|
| External footswitch | GPIO1 | Grove port, G1 (yellow wire), `INPUT_PULLUP`, active-low |
| Built-in button | GPIO41 | active-low |
| Status LED | GPIO35 | 1× WS2812C RGB |
| USB | USB-C | Native USB (CDC serial + USB MIDI) |

Both inputs are debounced (50 ms).

### Status LED

| Color | Meaning |
|---|---|
| Red | PTT active |
| Dim blue | BLE connected, idle |
| Slow blue blink | Advertising / not connected |

## Building & flashing

Requires [PlatformIO](https://platformio.org/).

```sh
make all      # build
make upload   # flash over USB-C
make clean    # clean build artifacts
```

Serial monitor at 115200 baud (`pio device monitor`).

## Power notes

Intended for USB or external power (AtomS3 Lite has no battery/PMIC).
To stay reliable with iOS BLE (which drops peripherals that miss connection
events), light sleep is **not** used. Power is instead reduced by disabling
WiFi, lowering BLE TX power to −12 dBm, and keeping the LED dim.

## License

Apache 2.0 — see [LICENSE](LICENSE).
