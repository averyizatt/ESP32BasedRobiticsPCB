#pragma once

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Motor Control — DRV8871 dual H-bridge driver
// =============================================================================
// Each motor is controlled by two LEDC PWM channels.
// Direction is determined by which channel carries the duty cycle signal.
//
// Forward:  PWM A = duty, PWM B = 0
// Reverse:  PWM A = 0,    PWM B = duty
// Coast:    PWM A = 0,    PWM B = 0
// Brake:    PWM A = full, PWM B = full
// =============================================================================

enum class MotorId { MOTOR1, MOTOR2 };

// Initialise both motor driver GPIO pins and LEDC channels.
void motors_init();

// Set motor speed and direction.
// speed: -255 (full reverse) to +255 (full forward). 0 = coast.
void motor_set(MotorId motor, int speed);

// Actively brake the motor (both inputs HIGH).
void motor_brake(MotorId motor);

// Coast the motor (both inputs LOW, driver disabled).
void motor_coast(MotorId motor);
