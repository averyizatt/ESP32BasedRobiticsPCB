#include "servos.h"

static Servo _servo1;
static Servo _servo2;

void servos_init() {
    _servo1.setPeriodHertz(50);
    _servo2.setPeriodHertz(50);
    _servo1.attach(SERVO1_PIN, SERVO_MIN_US, SERVO_MAX_US);
    _servo2.attach(SERVO2_PIN, SERVO_MIN_US, SERVO_MAX_US);
}

void servo_set_angle(ServoId servo, int degrees) {
    degrees = constrain(degrees, 0, 180);
    if (servo == ServoId::SERVO1) {
        _servo1.write(degrees);
    } else {
        _servo2.write(degrees);
    }
}

void servo_set_us(ServoId servo, int pulse_us) {
    pulse_us = constrain(pulse_us, SERVO_MIN_US, SERVO_MAX_US);
    if (servo == ServoId::SERVO1) {
        _servo1.writeMicroseconds(pulse_us);
    } else {
        _servo2.writeMicroseconds(pulse_us);
    }
}

void servo_detach(ServoId servo) {
    if (servo == ServoId::SERVO1) {
        _servo1.detach();
    } else {
        _servo2.detach();
    }
}
