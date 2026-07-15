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

// ---- LED ----

void updateStatusLED() {
    // Use full-intensity colors; overall dimming is handled by
    // FastLED.setBrightness() so the channel values must stay high to remain
    // visible after global scaling.
    if (pttActive) {
        leds[0] = CRGB::Red;
    } else if (usbActive) {
        leds[0] = CRGB::Green;  // USB host connected, idle
    } else if (bleConnected) {
        leds[0] = CRGB::Blue;   // BLE connected, idle
    } else {
        // slow blink while advertising
        leds[0] = ((millis() / 600) % 2) ? CRGB::Blue : CRGB::Black;
    }
    FastLED.show();
}

// ---- PTT ----

// Send only over the currently active transport to avoid the host receiving
// duplicate notes: USB when a host is present, otherwise BLE.
void pttOn() {
    if (pttActive) return;
    pttActive = true;
    if (usbActive) USBMIDI.sendNoteOn(99, 127, 1);
    else           MIDI.sendNoteOn(99, 127, 1);
    leds[0] = CRGB::Red;
    FastLED.show();
    Serial.println("PTT ON");
}

void pttOff() {
    if (!pttActive) return;
    pttActive = false;
    if (usbActive) USBMIDI.sendNoteOff(99, 0, 1);
    else           MIDI.sendNoteOff(99, 0, 1);
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

// ---- setup / loop ----

void setup() {
    // WiFi is never initialized (we never call WiFi.begin), so its radio stays
    // off and draws no power — no explicit disable needed on ESP32-S3.

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
    updateStatusLED();

    // Inputs
    pinMode(FOOTSWITCH_PIN, INPUT_PULLUP);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    Serial.println("=== M5PTT ready (AtomS3 Lite) build:debug1 ===");
    Serial.printf("ARDUINO_BOARD=%s  USB product set to M5PTT\n", ARDUINO_BOARD);
}

void loop() {
    MIDI.read();
    USBMIDI.read();

    pollUsbConnection();
    pollBleConnection();

    unsigned long now = millis();

    // Footswitch debounce
    bool fsRaw = digitalRead(FOOTSWITCH_PIN);
    if (fsRaw != fsLastRaw) {
        fsDebounce = now;
        fsLastRaw  = fsRaw;
    }
    if ((now - fsDebounce) > DEBOUNCE_DELAY && fsRaw != fsState) {
        fsState = fsRaw;
        if (fsState == LOW) pttOn(); else pttOff();
    }

    // Button debounce
    bool btnRaw = digitalRead(BUTTON_PIN);
    if (btnRaw != btnLastRaw) {
        btnDebounce = now;
        btnLastRaw  = btnRaw;
    }
    if ((now - btnDebounce) > DEBOUNCE_DELAY && btnRaw != btnState) {
        btnState = btnRaw;
        if (btnState == LOW) pttOn(); else pttOff();
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
        Serial.printf("[dbg] usbActive=%d usbMounted=%d bleConn=%d bleCount=%u "
                      "ptt=%d fsRaw=%d btnRaw=%d\n",
                      usbActive,
                      TinyUSBDevice.mounted(),
                      bleConnected,
                      srv ? srv->getConnectedCount() : 0,
                      pttActive,
                      digitalRead(FOOTSWITCH_PIN),
                      digitalRead(BUTTON_PIN));
    }
}
