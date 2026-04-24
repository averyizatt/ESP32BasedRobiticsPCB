#include "servos.h"

static Servo _servo1;

static void _ensure_servo1() {
    if (!_servo1.attached()) {
        _servo1.setPeriodHertz(50);
        _servo1.attach(SERVO1_PIN, SERVO_MIN_US, SERVO_MAX_US);
    }
}

void servos_init() {
    // Servo 2 disabled — SERVO2_PIN (GPIO 9) repurposed for potentiometer.
    // Servo 1 attached on-demand via servo_set_angle / servo_set_us to avoid
    // LEDC channel conflicts with motors during boot.
}

void servo_set_angle(ServoId servo, int degrees) {
    if (servo != ServoId::SERVO1) return;  // servo2 repurposed
    degrees = constrain(degrees, 0, 180);
    _ensure_servo1();
    _servo1.write(degrees);
}

void servo_set_us(ServoId servo, int pulse_us) {
    if (servo != ServoId::SERVO1) return;  // servo2 repurposed
    pulse_us = constrain(pulse_us, SERVO_MIN_US, SERVO_MAX_US);
    _ensure_servo1();
    _servo1.writeMicroseconds(pulse_us);
}

void servo_detach(ServoId servo) {
    if (servo == ServoId::SERVO1 && _servo1.attached()) {
        _servo1.detach();
    }
}
