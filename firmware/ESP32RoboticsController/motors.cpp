#include "motors.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void _motor_set_raw(uint8_t chA, uint8_t chB, int speed) {
    speed = constrain(speed, -255, 255);
    if (speed > 0) {
        ledcWrite(chA, (uint8_t)speed);
        ledcWrite(chB, 0);
    } else if (speed < 0) {
        ledcWrite(chA, 0);
        ledcWrite(chB, (uint8_t)(-speed));
    } else {
        ledcWrite(chA, 0);
        ledcWrite(chB, 0);
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void motors_init() {
    // Motor 1
    ledcSetup(MOTOR1A_CHANNEL, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
    ledcSetup(MOTOR1B_CHANNEL, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
    ledcAttachPin(MOTOR1_PWM_A, MOTOR1A_CHANNEL);
    ledcAttachPin(MOTOR1_PWM_B, MOTOR1B_CHANNEL);

    // Motor 2
    ledcSetup(MOTOR2A_CHANNEL, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
    ledcSetup(MOTOR2B_CHANNEL, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
    ledcAttachPin(MOTOR2_PWM_A, MOTOR2A_CHANNEL);
    ledcAttachPin(MOTOR2_PWM_B, MOTOR2B_CHANNEL);

    // Start coasted
    motor_coast(MotorId::MOTOR1);
    motor_coast(MotorId::MOTOR2);
}

void motor_set(MotorId motor, int speed) {
    if (motor == MotorId::MOTOR1) {
        _motor_set_raw(MOTOR1A_CHANNEL, MOTOR1B_CHANNEL, speed);
    } else {
        _motor_set_raw(MOTOR2A_CHANNEL, MOTOR2B_CHANNEL, speed);
    }
}

void motor_brake(MotorId motor) {
    uint8_t chA = (motor == MotorId::MOTOR1) ? MOTOR1A_CHANNEL : MOTOR2A_CHANNEL;
    uint8_t chB = (motor == MotorId::MOTOR1) ? MOTOR1B_CHANNEL : MOTOR2B_CHANNEL;
    ledcWrite(chA, 255);
    ledcWrite(chB, 255);
}

void motor_coast(MotorId motor) {
    uint8_t chA = (motor == MotorId::MOTOR1) ? MOTOR1A_CHANNEL : MOTOR2A_CHANNEL;
    uint8_t chB = (motor == MotorId::MOTOR1) ? MOTOR1B_CHANNEL : MOTOR2B_CHANNEL;
    ledcWrite(chA, 0);
    ledcWrite(chB, 0);
}
