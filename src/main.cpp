#include <Adafruit_TinyUSB.h>
#include <BLEMIDI_Transport.h>
#include <hardware/BLEMIDI_ESP32_NimBLE.h>
#include <MIDI.h>
#include <FastLED.h>

// BLE MIDI — macro creates BLEMIDI (transport) and MIDI (BLE MIDI interface)
BLEMIDI_CREATE_INSTANCE("M5PTT", MIDI)

// USB MIDI
Adafruit_USBD_MIDI usb_transport;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_transport, USBMIDI)

// LED — 1x WS2812C on GPIO35
#define LED_PIN  35
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];

// Pins
const int FOOTSWITCH_PIN = 1;   // Grove G1 (yellow wire)
const int BUTTON_PIN     = 41;  // built-in button

// MIDI notes (map these in SmartSDR's Mapping Editor, channel 1)
#define PTT_NOTE       99   // voice PTT
#define CW_MEM1_NOTE  100   // single click  -> CWX macro 1
#define CW_MEM2_NOTE  101   // double click  -> CWX macro 2
#define CW_MEM3_NOTE  102   // triple click  -> CWX macro 3
#define CW_ABORT_NOTE 103   // long press    -> CWX stop/abort

// CW-mode click timing
const unsigned long CLICK_WINDOW  = 350;  // ms after last release to settle count
const unsigned long LONG_PRESS_MS = 600;  // ms held -> abort
const unsigned long MORSE_UNIT    = 120;  // ms per Morse dit

// Debounce — footswitch
bool fsState   = HIGH;
bool fsLastRaw = HIGH;
unsigned long fsDebounce = 0;

// Debounce — button
bool btnState   = HIGH;
bool btnLastRaw = HIGH;
unsigned long btnDebounce = 0;

const unsigned long DEBOUNCE_DELAY = 50;

// State
bool pttActive    = false;
bool bleConnected = false;
bool usbActive    = false;  // true once a USB host has enumerated us; USB then
                            // becomes the sole MIDI transport and BLE is shut down
bool cwMode       = false;  // chosen at boot by holding the button; CW keyer mode

// ---- LED ----

void updateStatusLED() {
    // Use full-intensity colors; overall dimming is handled by
    // FastLED.setBrightness() so the channel values must stay high to remain
    // visible after global scaling.
    // CW mode uses a magenta palette for BLE states so the operating mode is
    // obvious at a glance; voice mode uses blue. USB-active is green in both.
    const CRGB connColor = cwMode ? CRGB::Magenta : CRGB::Blue;
    if (pttActive) {
        leds[0] = CRGB::Red;
    } else if (usbActive) {
        leds[0] = CRGB::Green;  // USB host connected, idle
    } else if (bleConnected) {
        leds[0] = connColor;    // BLE connected, idle
    } else {
        // slow blink while advertising
        leds[0] = ((millis() / 600) % 2) ? connColor : CRGB::Black;
    }
    FastLED.show();
}

// ---- PTT ----

// Send only over the currently active transport to avoid the host receiving
// duplicate notes: USB when a host is present, otherwise BLE.
void pttOn() {
    if (pttActive) return;
    pttActive = true;
    if (usbActive) USBMIDI.sendNoteOn(PTT_NOTE, 127, 1);
    else           MIDI.sendNoteOn(PTT_NOTE, 127, 1);
    leds[0] = CRGB::Red;
    FastLED.show();
    Serial.println("PTT ON");
}

void pttOff() {
    if (!pttActive) return;
    pttActive = false;
    if (usbActive) USBMIDI.sendNoteOff(PTT_NOTE, 0, 1);
    else           MIDI.sendNoteOff(PTT_NOTE, 0, 1);
    updateStatusLED();
    Serial.println("PTT OFF");
}

// ---- BLE connection state ----

// The BLE-MIDI library's onConnect/onDisconnect callbacks don't fire under
// NimBLE-Arduino 2.x (its virtual signature changed to include NimBLEConnInfo),
// so the library's setHandleConnected() is dead. Poll the server's connection
// count directly instead.
void pollBleConnection() {
    NimBLEServer* srv = NimBLEDevice::getServer();
    bool nowConnected = srv && srv->getConnectedCount() > 0;
    if (nowConnected == bleConnected) return;

    bleConnected = nowConnected;
    if (!bleConnected && pttActive) pttOff();
    updateStatusLED();
    Serial.printf("[BLE] %s (count=%u)\n",
                  bleConnected ? "connected" : "disconnected",
                  srv ? srv->getConnectedCount() : 0);
}

// ---- USB connection state ----

// When a USB host enumerates the device, make USB the sole transport: drop any
// BLE client and stop advertising so SmartSDR can't receive doubled notes over
// two links. When USB is unplugged, fall back to BLE and resume advertising.
void pollUsbConnection() {
    bool nowMounted = TinyUSBDevice.mounted();
    if (nowMounted == usbActive) return;

    // Release PTT on the *current* transport before switching.
    if (pttActive) pttOff();

    usbActive = nowMounted;
    NimBLEServer* srv      = NimBLEDevice::getServer();
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();

    if (usbActive) {
        // Prevent the stack from auto-restarting advertising when we drop the
        // client (the BLE-MIDI library enables advertiseOnDisconnect).
        if (srv) {
            srv->advertiseOnDisconnect(false);
            for (uint8_t i = srv->getConnectedCount(); i > 0; --i)
                srv->disconnect(srv->getPeerInfo(i - 1));
        }
        if (adv) adv->stop();
        bleConnected = false;
    } else {
        if (srv) srv->advertiseOnDisconnect(true);
        if (adv) adv->start();
    }

    updateStatusLED();
    Serial.printf("[USB] %s -> transport=%s\n",
                  usbActive ? "mounted" : "unmounted",
                  usbActive ? "USB" : "BLE");
}

// ---- CW mode ----

// Momentary note pulse (press+release) on the active transport, used to trigger
// a mapped CWX macro / stop.
void sendMidiPulse(uint8_t note) {
    if (usbActive) {
        USBMIDI.sendNoteOn(note, 127, 1);
        USBMIDI.sendNoteOff(note, 0, 1);
    } else {
        MIDI.sendNoteOn(note, 127, 1);
        MIDI.sendNoteOff(note, 0, 1);
    }
}

// Blocking LED confirmation blink (short; only used for user feedback).
void ledBlink(uint8_t count, const CRGB& color, uint16_t onMs, uint16_t offMs) {
    for (uint8_t i = 0; i < count; ++i) {
        leds[0] = color; FastLED.show(); delay(onMs);
        leds[0] = CRGB::Black; FastLED.show();
        if (i + 1 < count) delay(offMs);
    }
    updateStatusLED();
}

// Flash "R" (.-.) in Morse on the LED to confirm CW mode at boot.
void flashMorseR() {
    const uint8_t U = 1;   // dit
    const uint8_t D = 3;   // dah
    const uint8_t pattern[] = { U, D, U };
    for (uint8_t i = 0; i < 3; ++i) {
        leds[0] = CRGB::Green; FastLED.show();
        delay(pattern[i] * MORSE_UNIT);
        leds[0] = CRGB::Black; FastLED.show();
        if (i + 1 < 3) delay(MORSE_UNIT);   // intra-character gap
    }
}

// CW click/long-press state
uint8_t       cwClickCount   = 0;
unsigned long cwLastRelease  = 0;
unsigned long cwPressStart   = 0;
bool          cwPressConsumed = false;  // long press already fired for this press

void handleCwButton() {
    unsigned long now = millis();

    // Debounce (same pattern as voice mode).
    bool raw = digitalRead(BUTTON_PIN);
    if (raw != btnLastRaw) {
        btnDebounce = now;
        btnLastRaw  = raw;
    }
    if ((now - btnDebounce) > DEBOUNCE_DELAY && raw != btnState) {
        btnState = raw;
        if (btnState == LOW) {              // pressed
            cwPressStart    = now;
            cwPressConsumed = false;
        } else {                            // released
            if (!cwPressConsumed) {
                cwClickCount++;
                cwLastRelease = now;
            }
        }
    }

    // Long press held -> abort (fires once, before release).
    if (btnState == LOW && !cwPressConsumed &&
        (now - cwPressStart) > LONG_PRESS_MS) {
        cwPressConsumed = true;
        cwClickCount    = 0;
        sendMidiPulse(CW_ABORT_NOTE);
        Serial.println("[CW] abort");
        ledBlink(1, CRGB::Red, 400, 0);
    }

    // Click window elapsed -> fire the memory for the accumulated count.
    if (cwClickCount > 0 && btnState == HIGH &&
        (now - cwLastRelease) > CLICK_WINDOW) {
        uint8_t n = cwClickCount > 3 ? 3 : cwClickCount;
        const uint8_t notes[3] = { CW_MEM1_NOTE, CW_MEM2_NOTE, CW_MEM3_NOTE };
        sendMidiPulse(notes[n - 1]);
        Serial.printf("[CW] memory %u (note %u)\n", n, notes[n - 1]);
        cwClickCount = 0;
        ledBlink(n, CRGB::White, 120, 120);
    }
}

// ---- setup / loop ----

void setup() {
    // WiFi is never initialized (we never call WiFi.begin), so its radio stays
    // off and draws no power — no explicit disable needed on ESP32-S3.

    // Inputs first, so the button can be sampled for boot-time mode selection.
    pinMode(FOOTSWITCH_PIN, INPUT_PULLUP);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    Serial.begin(115200);

    // USB MIDI must be started before USB bus is up.
    // Name the device/port "M5PTT" so it matches the BLE name in hosts.
    TinyUSBDevice.setManufacturerDescriptor("M5");
    TinyUSBDevice.setProductDescriptor("M5PTT");
    usb_transport.setCableName(1, "M5PTT");
    usb_transport.begin();
    USBMIDI.begin(MIDI_CHANNEL_OMNI);

    // BLE MIDI. MIDI.begin() starts advertising, but the library only puts the
    // service UUID in the advertisement — not the name. Without a Complete Local
    // Name in the advertisement, iOS shows the peripheral's UUID instead of
    // "M5PTT". Inject the name (using the scan response for headroom) and
    // restart advertising.
    MIDI.begin(MIDI_CHANNEL_OMNI);
    NimBLEDevice::setPower(ESP_PWR_LVL_N12);  // -12 dBm, adequate for tabletop

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->stop();
    adv->setName("M5PTT");
    adv->setAppearance(0x03C0);  // Generic MIDI Device
    adv->enableScanResponse(true);
    adv->start();

    // LED
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(30);

    // Mode select: holding the built-in button during boot enters CW mode.
    // (Button was set to INPUT_PULLUP at the very top of setup so it reads here.)
    cwMode = (digitalRead(BUTTON_PIN) == LOW);
    if (cwMode) {
        flashMorseR();                 // confirm CW mode with "R" (.-.)
        // The button is still held here. Seed the debounce state as "pressed"
        // and mark this press consumed so releasing it after boot is neither
        // counted as a click nor fires the long-press abort.
        btnState = LOW; btnLastRaw = LOW;
        cwPressConsumed = true;
    }
    updateStatusLED();

    Serial.printf("=== M5PTT ready (AtomS3 Lite) build:debug1 mode=%s ===\n",
                  cwMode ? "CW" : "VOICE");
    Serial.printf("ARDUINO_BOARD=%s  USB product set to M5PTT\n", ARDUINO_BOARD);
}

void loop() {
    MIDI.read();
    USBMIDI.read();

    pollUsbConnection();
    pollBleConnection();

    unsigned long now = millis();

    if (cwMode) {
        // CW mode: only the built-in button is used (footswitch ignored).
        handleCwButton();
    } else {
        // Voice mode: footswitch + button act as PTT.
        bool fsRaw = digitalRead(FOOTSWITCH_PIN);
        if (fsRaw != fsLastRaw) {
            fsDebounce = now;
            fsLastRaw  = fsRaw;
        }
        if ((now - fsDebounce) > DEBOUNCE_DELAY && fsRaw != fsState) {
            fsState = fsRaw;
            if (fsState == LOW) pttOn(); else pttOff();
        }

        bool btnRaw = digitalRead(BUTTON_PIN);
        if (btnRaw != btnLastRaw) {
            btnDebounce = now;
            btnLastRaw  = btnRaw;
        }
        if ((now - btnDebounce) > DEBOUNCE_DELAY && btnRaw != btnState) {
            btnState = btnRaw;
            if (btnState == LOW) pttOn(); else pttOff();
        }
    }

    // Keep LED blink updated when idle
    if (!pttActive) {
        updateStatusLED();
    }

    // Periodic debug (1 Hz)
    static unsigned long lastDebug = 0;
    if (now - lastDebug > 1000) {
        lastDebug = now;
        NimBLEServer* srv = NimBLEDevice::getServer();
        Serial.printf("[dbg] mode=%s usbActive=%d usbMounted=%d bleConn=%d "
                      "bleCount=%u ptt=%d cwClicks=%u fsRaw=%d btnRaw=%d\n",
                      cwMode ? "CW" : "VOICE",
                      usbActive,
                      TinyUSBDevice.mounted(),
                      bleConnected,
                      srv ? srv->getConnectedCount() : 0,
                      pttActive,
                      cwClickCount,
                      digitalRead(FOOTSWITCH_PIN),
                      digitalRead(BUTTON_PIN));
    }
}
