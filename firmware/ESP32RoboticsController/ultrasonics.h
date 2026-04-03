#pragma once

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Ultrasonic Sensor Interface — HC-SR04 (4 sensors)
// =============================================================================
// Each sensor uses one TRIG output and one ECHO input.
// Measurement: send a 10 µs trigger pulse, measure the ECHO pulse width,
// then convert to distance using the speed of sound.
// =============================================================================

enum class UltrasonicId { US1, US2, US3, US4 };

// Configure all TRIG and ECHO pins.
void ultrasonics_init();

// Trigger a single measurement and return distance in centimetres.
// Returns -1.0 if the measurement timed out (object out of range).
float ultrasonic_read_cm(UltrasonicId sensor);

// Read all four sensors and store results into the provided array (size 4).
void ultrasonics_read_all_cm(float results[4]);
