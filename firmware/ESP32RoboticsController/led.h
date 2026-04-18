#pragma once

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Onboard RGB LED — WS2812 (NeoPixel) on GPIO 48
// =============================================================================
// Uses the built-in neopixelWrite() from arduino-esp32.
// No external library needed.
// =============================================================================

// Initialise the LED pin.
void led_init();

// Set the LED to an RGB colour (0–255 per channel).
void led_set(uint8_t r, uint8_t g, uint8_t b);

// Convenience presets
void led_off();
void led_green();
void led_red();
void led_blue();
void led_yellow();
void led_cyan();
void led_white();

// Blink the LED n times with the given colour and period.
void led_blink(uint8_t r, uint8_t g, uint8_t b, int count, int on_ms = 100, int off_ms = 100);
