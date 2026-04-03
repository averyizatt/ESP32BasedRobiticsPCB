#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"

// =============================================================================
// Servo Control
// =============================================================================
// Uses the ESP32Servo library for standard 50 Hz servo control.
// Pulse range: SERVO_MIN_US (0°) to SERVO_MAX_US (180°).
// =============================================================================

enum class ServoId { SERVO1, SERVO2 };

// Attach servo objects to GPIO pins. Call once in setup().
void servos_init();

// Set servo angle in degrees (0–180).
void servo_set_angle(ServoId servo, int degrees);

// Set servo position using raw pulse width in microseconds.
void servo_set_us(ServoId servo, int pulse_us);

// Detach servo (stops PWM signal, reduces current draw).
void servo_detach(ServoId servo);
