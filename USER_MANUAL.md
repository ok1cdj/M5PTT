# M5 PTT — User Manual

A wireless/wired **push-to-talk (PTT) and CW keyer footswitch** for use with
**SmartSDR on iOS** (and any MIDI-capable host). Built on an M5Stack **AtomS3
Lite**.

Press a footswitch or the small top button to key your transmitter. The device
talks to SmartSDR over **Bluetooth LE** (cable-free) or **USB** — it picks the
right one for you automatically.

<p align="center">
  <img src="img/atom3lite.webp" alt="M5Stack AtomS3 Lite — the top button and USB-C port" width="320">
</p>

---

## 1. What's in the box / on the device

| Part | Where | Used for |
|---|---|---|
| **Top button** | Small button on top of the unit | PTT (voice mode) or keyer memories (CW mode); also mode selection at power-on |
| **Status LED** | Under the top button | Shows mode and connection state (see §6) |
| **Grove port** | Side socket | Plug in an external footswitch |
| **USB-C port** | Bottom | Power and/or USB-MIDI connection |

An external footswitch plugs into the **Grove port**. Wiring: signal on **G1
(the yellow wire)**, switching to ground. Any simple momentary (normally-open)
switch works.

---

## 2. Powering the device

Use the USB-C port. You have two choices, and they change how the device talks
to SmartSDR:

| You plug it into… | The device uses | Bluetooth |
|---|---|---|
| A **charger or power bank** (power only, no data) | **Bluetooth LE** — wireless | On |
| A **computer or iPad** (a real USB host) | **USB MIDI** — wired | Off |

**This switch is automatic.** When a computer/iPad is connected, the device
uses the wired USB link and turns Bluetooth off, so SmartSDR never hears the
same keypress twice. Unplug from the computer and it goes back to wireless
Bluetooth on its own.

> **For normal wireless use:** power it from a plain USB **charger or battery
> pack** — *not* a computer. From a computer it stays in USB mode with
> Bluetooth off.

### Cable-free option: Atomic Battery Base

For fully untethered operation, stack the device onto an **M5Stack Atomic
Battery Base (200 mAh)** — it fits the Atom form factor and plugs onto the
bottom of the unit, giving you a self-contained wireless footswitch with no
cables at all.

- **Battery:** 3.7 V, 200 mAh built-in LiPo.
- **On/off:** a dip switch on the base selects **discharge** (powers the device
  from the battery — this is the wireless mode you want in use) or **charge**
  (recharge the battery through the device's USB-C port from any charger). The
  two modes can't run at the same time, so flip the switch to *charge* when
  topping up and back to *discharge* to operate.
- **Indicators (on the base):** a four-segment red gauge shows remaining
  charge; a separate LED is **blue while charging** and **green when full**.
- **Behaviour:** because the base delivers power only (no data host), the
  device runs on **Bluetooth LE** — exactly like using a plain charger, so
  pairing and mapping work the same way (§7).

> Product page:
> <https://shop.m5stack.com/products/atomic-battery-base-200mah>
>
> Note: at 200 mAh this is a small battery sized for short, portable sessions.
> For long operating periods, power from a larger USB power bank instead.

---

## 3. Two operating modes

The device has two modes, chosen at power-on:

- **Voice mode** (default) — the footswitch **and** the top button both act as
  push-to-talk. Hold to transmit, release to stop.
- **CW mode** — the top button triggers your **CWX keyer memories** by how many
  times you click it. The footswitch is ignored in this mode.

### Choosing / switching modes

The device **remembers** its mode across power cycles.

- **Normal power-on** → boots into whatever mode you used last.
- **To switch modes:** hold the **top button while plugging in power**, keep it
  held for about a second, then release. This toggles to the other mode and
  saves it. It stays in the new mode on every future power-on until you toggle
  again.

When it powers up in **CW mode**, the LED flashes the Morse letter **R**
(`· — ·`, dot-dash-dot in green) to confirm. Voice mode gives no special flash.

---

## 4. Voice mode (PTT)

1. Power the device from a charger (for wireless) — the LED blinks blue while
   looking for SmartSDR, and goes solid blue once connected.
2. **Press and hold** the footswitch (or the top button) → you transmit. The
   LED turns **red**.
3. **Release** → you stop transmitting. LED returns to blue/green.

That's it — it's a momentary switch, exactly like a hand mic's PTT.

---

## 5. CW mode (keyer memories)

In CW mode the **top button** fires CWX macros based on a **click count**. After
your last click there's a short pause (~⅓ second) while it counts, then it
sends:

| Do this | Sends | Typical use |
|---|---|---|
| **1 click** | CWX macro 1 | e.g. CQ |
| **2 clicks** | CWX macro 2 | e.g. your call / exchange |
| **3 clicks** | CWX macro 3 | e.g. a third message |
| **Hold ~½ second** | CWX stop / abort | Cancel the message being sent |

Feedback on the LED:

- **1 / 2 / 3 white blinks** — memory 1 / 2 / 3 was sent.
- **One long red blink** — abort/stop was sent.

(Clicking more than three times still just sends memory 3.)

---

## 6. Status LED reference

The LED tells you the mode and the connection at a glance.

| LED | Meaning |
|---|---|
| **Red** | Transmitting (PTT active) |
| **Green** (steady) | Connected to a computer/iPad over USB, idle and ready |
| **Solid blue** | Voice mode — connected to SmartSDR over Bluetooth |
| **Blinking blue** | Voice mode — searching for SmartSDR (advertising) |
| **Solid magenta** | CW mode — connected to SmartSDR over Bluetooth |
| **Blinking magenta** | CW mode — searching for SmartSDR (advertising) |
| **Morse "R" in green** (at power-on) | Confirms it booted into CW mode |
| **White blinks (1–3)** | CW memory 1/2/3 was sent |
| **Long red blink** | CW abort/stop was sent |

Colour rule of thumb: **blue = voice, magenta = CW, green = wired USB, red =
keying.**

---

## 7. Pairing & setting up SmartSDR (iOS)

### Connect over Bluetooth

1. Power the device from a **charger/power bank**. The LED blinks blue (voice)
   or magenta (CW).
2. In SmartSDR's Bluetooth/MIDI device setup, look for a device named
   **`M5PTT_XXXX`** (the `XXXX` is unique to your unit, so you can tell two
   apart). Connect to it.
3. The LED goes **solid** blue/magenta once connected.

### Map the buttons in SmartSDR

The device sends standard **MIDI notes on channel 1**. Open SmartSDR's
**Mapping Editor** and map each note to the function you want:

| Action | MIDI note | Map to |
|---|---|---|
| Voice PTT | **99** | PTT (keys on note-on, unkeys on note-off) |
| CW single click | **100** | CWX macro 1 |
| CW double click | **101** | CWX macro 2 |
| CW triple click | **102** | CWX macro 3 |
| CW long press | **103** | CWX stop / abort |

You only need to map the notes for the mode(s) you actually use. PTT (note 99)
is sent as a sustained press/release; the CW notes are short momentary pulses.

---

## 8. Troubleshooting

**SmartSDR doesn't see the device over Bluetooth.**
Make sure it's powered from a **charger, not a computer**. On a computer it
switches to USB and turns Bluetooth off. The LED should be **blinking**
blue/magenta when it's advertising for a connection.

**It shows up but as a strange name / UUID.**
Look for `M5PTT_XXXX`. Each unit's name ends in four characters unique to it.

**Keypresses seem doubled.**
This shouldn't happen — the device only ever uses one link at a time. If you
see it, unplug and re-power from a single source.

**Nothing happens when I press the button in CW mode.**
Remember there's a brief counting pause after your last click before the memory
fires. Also confirm the notes (100–103) are mapped in SmartSDR and that you're
in CW mode (magenta LED, "R" flash at boot).

**It's in the wrong mode.**
Hold the top button while plugging in power for ~1 second to toggle modes; the
choice is saved for next time.

**PTT released by itself.**
If the Bluetooth link drops while transmitting, the device safely releases PTT
so you're not left keyed up.

---

## 9. Quick reference card

```
POWER
  Charger / power bank      → wireless (Bluetooth)  LED blue/magenta
  Atomic Battery Base       → wireless (Bluetooth), no cables
  Computer / iPad           → wired (USB)           LED green

MODE (chosen at power-on, remembered)
  Just power on          → last used mode
  Hold button + power on → switch mode (saved). CW confirms with green "R"

VOICE MODE
  Footswitch or button → hold = transmit (LED red)

CW MODE (top button only)
  1 click  → memory 1     (note 100)
  2 clicks → memory 2     (note 101)
  3 clicks → memory 3     (note 102)
  hold ½s  → stop/abort   (note 103)

MIDI NOTES (channel 1) — map in SmartSDR Mapping Editor
  99 PTT · 100/101/102 CWX macros 1/2/3 · 103 CWX stop
```
