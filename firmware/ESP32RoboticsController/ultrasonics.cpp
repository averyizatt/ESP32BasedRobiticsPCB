#include "ultrasonics.h"

// Sensor pin table indexed by UltrasonicId
static const uint8_t _trig_pins[4] = {
    ULTRASONIC1_TRIG,
    ULTRASONIC2_TRIG,
    ULTRASONIC3_TRIG,
    ULTRASONIC4_TRIG
};

static const uint8_t _echo_pins[4] = {
    ULTRASONIC1_ECHO,
    ULTRASONIC2_ECHO,
    ULTRASONIC3_ECHO,
    ULTRASONIC4_ECHO
};

void ultrasonics_init() {
    for (int i = 0; i < 4; i++) {
        pinMode(_trig_pins[i], OUTPUT);
        digitalWrite(_trig_pins[i], LOW);
        pinMode(_echo_pins[i], INPUT);
    }
}

float ultrasonic_read_cm(UltrasonicId sensor) {
    uint8_t idx = (uint8_t)sensor;
    uint8_t trig = _trig_pins[idx];
    uint8_t echo = _echo_pins[idx];

    // Ensure TRIG is LOW before pulse
    digitalWrite(trig, LOW);
    delayMicroseconds(2);

    // Send 10 µs trigger pulse
    digitalWrite(trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig, LOW);

    // Measure ECHO pulse width
    unsigned long duration = pulseIn(echo, HIGH, ULTRASONIC_TIMEOUT_US);
    if (duration == 0) {
        return -1.0f; // Timeout — no echo received
    }

    // distance = (duration × speed_of_sound) / 2  (round trip)
    float distance_cm = (duration * SOUND_SPEED_CM_PER_US) / 2.0f;
    return distance_cm;
}

void ultrasonics_read_all_cm(float results[4]) {
    results[0] = ultrasonic_read_cm(UltrasonicId::US1);
    results[1] = ultrasonic_read_cm(UltrasonicId::US2);
    results[2] = ultrasonic_read_cm(UltrasonicId::US3);
    results[3] = ultrasonic_read_cm(UltrasonicId::US4);
}
