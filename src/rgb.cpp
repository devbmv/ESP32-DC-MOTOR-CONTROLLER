#include <Arduino.h>
#include "project_config.h"

// ---------------------------------------------------------
// 💡 Heartbeat callback — toggles LED color periodically
// ---------------------------------------------------------
void hbCb(TimerHandle_t)
{
    static bool on = false;
    on = !on;

    // LED color: soft blue pulse when "on", off when "off"
    setLed(on ? 0 : 0, on ? 4 : 0, on ? 24 : 0);

    // Notify system that a new sensor sample is available
    xEventGroupSetBits(egFlags, EV_NEW_SAMPLE);
}

// ---------------------------------------------------------
// 🌈 Initialize RGB LED (FastLED)
// ---------------------------------------------------------
void initRGB()
{
    // Initialize FastLED for onboard RGB (NeoPixel type)
    FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);

    // Set initial LED color (red glow at startup)
    setLed(12, 0, 0);
    delay(10);
}
