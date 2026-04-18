#pragma once

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Push Button Interface — debounced digital inputs
// =============================================================================
// Navigation uses two buttons:
//   BTN1 (BUTTON1_PIN) — Cycle / Next
//   BTN2 (BUTTON2_PIN) — Select / Confirm
// All buttons use INPUT_PULLUP. Pressed = LOW.
// =============================================================================

// Button identifiers
enum class ButtonId : uint8_t { BTN1 = 0, BTN2 = 1 };

// Convenience aliases for the two navigation buttons
static constexpr ButtonId BTN_CYCLE  = ButtonId::BTN1;
static constexpr ButtonId BTN_SELECT = ButtonId::BTN2;

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

// Return how many ms the button has been continuously held (0 if not pressed).
unsigned long button_held_ms(ButtonId btn);
