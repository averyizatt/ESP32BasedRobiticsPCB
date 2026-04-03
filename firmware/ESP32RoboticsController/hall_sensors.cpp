#include "hall_sensors.h"

static volatile long _count1 = 0;
static volatile long _count2 = 0;
static unsigned long _last_time1 = 0;
static unsigned long _last_time2 = 0;
static long _last_count1 = 0;
static long _last_count2 = 0;

static void IRAM_ATTR _isr_hall1() {
    _count1++;
}

static void IRAM_ATTR _isr_hall2() {
    _count2++;
}

void hall_sensors_init() {
    pinMode(HALL1_PIN, INPUT_PULLUP);
    pinMode(HALL2_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(HALL1_PIN), _isr_hall1, RISING);
    attachInterrupt(digitalPinToInterrupt(HALL2_PIN), _isr_hall2, RISING);
    _last_time1 = millis();
    _last_time2 = millis();
}

long hall_get_count(uint8_t sensor) {
    if (sensor == 1) return _count1;
    if (sensor == 2) return _count2;
    return 0;
}

void hall_reset_count(uint8_t sensor) {
    if (sensor == 1) {
        noInterrupts();
        _count1 = 0;
        _last_count1 = 0;
        interrupts();
    } else if (sensor == 2) {
        noInterrupts();
        _count2 = 0;
        _last_count2 = 0;
        interrupts();
    }
}

float hall_get_pulses_per_second(uint8_t sensor) {
    unsigned long now = millis();
    if (sensor == 1) {
        long current_count = _count1;
        long delta = current_count - _last_count1;
        float elapsed_s = (now - _last_time1) / 1000.0f;
        _last_count1 = current_count;
        _last_time1 = now;
        if (elapsed_s <= 0.0f) return 0.0f;
        return delta / elapsed_s;
    } else if (sensor == 2) {
        long current_count = _count2;
        long delta = current_count - _last_count2;
        float elapsed_s = (now - _last_time2) / 1000.0f;
        _last_count2 = current_count;
        _last_time2 = now;
        if (elapsed_s <= 0.0f) return 0.0f;
        return delta / elapsed_s;
    }
    return 0.0f;
}
