#pragma once

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Hall Effect Sensor Interface
// =============================================================================
// Two hall effect sensors used for wheel speed, rotation counting, or
// position feedback. Each sensor is read as a digital input and interrupt-
// driven pulse counting is used for accurate speed measurement.
// =============================================================================

// Initialise hall sensor pins and attach interrupt handlers.
void hall_sensors_init();

// Return the current pulse count for a given sensor (cumulative since init).
long hall_get_count(uint8_t sensor); // sensor: 1 or 2

// Reset the pulse count for a given sensor to zero.
void hall_reset_count(uint8_t sensor);

// Calculate speed in pulses per second for a given sensor.
// Call periodically (e.g., every 100 ms) for stable readings.
float hall_get_pulses_per_second(uint8_t sensor);
