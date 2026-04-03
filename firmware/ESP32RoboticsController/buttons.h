#pragma once

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Push Button Interface — debounced digital inputs
// =============================================================================
// All three buttons use INPUT_PULLUP. Pressed = LOW.
// Software debounce is applied using a timestamp comparison.
// =============================================================================

// Button identifiers
enum class ButtonId { BTN1, BTN2, BTN3 };

// Initialise button GPIO pins.
void buttons_init();

// Update debounce state. Call once per loop() iteration.
void buttons_update();

// Return true if the button is currently held down (after debounce).
bool button_is_pressed(ButtonId btn);

// Return true if the button was just pressed this loop cycle (rising edge).
bool button_just_pressed(ButtonId btn);

// Return true if the button was just released this loop cycle (falling edge).
bool button_just_released(ButtonId btn);
