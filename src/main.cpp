#include <Adafruit_TinyUSB.h>
#include <BLEMIDI_Transport.h>
#include <hardware/BLEMIDI_ESP32_NimBLE.h>
#include <MIDI.h>
#include <FastLED.h>
#include <WiFi.h>
#include <esp_wifi.h>

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

// ---- LED ----

void updateStatusLED() {
    if (pttActive) {
        leds[0] = CRGB::Red;
    } else if (bleConnected) {
        leds[0] = CRGB(0, 0, 15);  // dim blue = connected idle
    } else {
        // slow blink while advertising
        leds[0] = ((millis() / 600) % 2) ? CRGB(0, 0, 8) : CRGB::Black;
    }
    FastLED.show();
}

// ---- PTT ----

void pttOn() {
    if (pttActive) return;
    pttActive = true;
    MIDI.sendNoteOn(99, 127, 1);
    USBMIDI.sendNoteOn(99, 127, 1);
    leds[0] = CRGB::Red;
    FastLED.show();
    Serial.println("PTT ON");
}

void pttOff() {
    if (!pttActive) return;
    pttActive = false;
    MIDI.sendNoteOff(99, 0, 1);
    USBMIDI.sendNoteOff(99, 0, 1);
    updateStatusLED();
    Serial.println("PTT OFF");
}

// ---- BLE callbacks ----

void OnConnected() {
    bleConnected = true;
    updateStatusLED();
    Serial.println("BLE connected");
}

void OnDisconnected() {
    bleConnected = false;
    if (pttActive) pttOff();
    updateStatusLED();
    Serial.println("BLE disconnected");
}

// ---- setup / loop ----

void setup() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    esp_wifi_stop();

    Serial.begin(115200);

    // USB MIDI must be started before USB bus is up
    usb_transport.begin();
    USBMIDI.begin(MIDI_CHANNEL_OMNI);

    // BLE MIDI
    MIDI.begin(MIDI_CHANNEL_OMNI);
    NimBLEDevice::setPower(ESP_PWR_LVL_N12);  // -12 dBm, adequate for tabletop
    BLEMIDI.setHandleConnected(OnConnected);
    BLEMIDI.setHandleDisconnected(OnDisconnected);

    // LED
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(30);
    updateStatusLED();

    // Inputs
    pinMode(FOOTSWITCH_PIN, INPUT_PULLUP);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    Serial.println("M5PTT ready (AtomS3 Lite)");
}

void loop() {
    MIDI.read();
    USBMIDI.read();

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
}
