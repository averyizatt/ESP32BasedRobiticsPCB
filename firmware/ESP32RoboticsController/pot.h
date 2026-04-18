#pragma once

#include <Arduino.h>

// =============================================================================
// Potentiometer Input — analog dial for menu navigation
// =============================================================================
// Reads an analog potentiometer via analogRead() with EMA smoothing
// and hysteresis-based index mapping for jitter-free menu selection.
//
// NOTE: On ESP32-S3, only GPIO 1–20 support ADC.  If POT_PIN is outside
//       that range, analogRead() will return 0 and pot_position() will
//       always return 0.  Rewire the pot to an ADC-capable pin if needed.
// =============================================================================

/// Initialise ADC and seed the smoothing filter.
void pot_init();

/// Read the ADC and update the smoothed value.  Call once per loop().
void pot_update();

/// Return the smoothed ADC value (0–4095).
uint16_t pot_raw();

/// Map the smoothed value to an index in [0, count-1] with hysteresis.
/// Call every frame when the menu is active.
uint8_t pot_position(uint8_t count);

/// Returns true (once) if the raw pot value moved significantly since the
/// last call.  Useful for waking from idle without knowing the item count.
bool pot_moved();
