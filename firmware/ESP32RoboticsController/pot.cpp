#include "pot.h"
#include "config.h"

// =============================================================================
// Potentiometer driver — EMA-smoothed ADC with hysteresis mapping
// =============================================================================

static uint16_t _smooth  = 2048;    // EMA-filtered ADC value
static uint8_t  _mapped  = 0xFF;    // current mapped index (0xFF = uninitialised)
static uint16_t _idle_ref = 2048;   // reference for pot_moved()

void pot_init() {
    analogReadResolution(12);       // 0–4095

    // Seed the filter with an averaged burst read
    uint32_t sum = 0;
    for (int i = 0; i < 32; i++) sum += analogRead(POT_PIN);
    _smooth   = (uint16_t)(sum >> 5);
    _idle_ref = _smooth;

    if (_smooth == 0) {
        Serial.printf("[pot] WARNING: pin %d reads 0 — check wiring.\n", POT_PIN);
    }
    Serial.printf("[pot] init  pin=%d  raw=%u\n", POT_PIN, _smooth);
}

void pot_update() {
    uint16_t raw = (uint16_t)analogRead(POT_PIN);
    // Exponential moving average  (alpha ≈ 1/8 → smooth, low-jitter)
    _smooth = (uint16_t)(((uint32_t)_smooth * 7 + raw) >> 3);
}

uint16_t pot_raw() {
    return _smooth;
}

uint8_t pot_position(uint8_t count) {
    if (count == 0) return 0;

    uint8_t idx = (uint8_t)(((uint32_t)_smooth * count) / 4096);
    if (idx >= count) idx = count - 1;

    // First call or count changed — accept directly
    if (_mapped >= count) {
        _mapped = idx;
        return _mapped;
    }

    if (idx != _mapped) {
        // Hysteresis: require moving 62 % of a zone away from current centre
        // before accepting a new index (≈ 25 % dead-band at each boundary).
        uint16_t zone       = 4096 / count;
        uint16_t cur_centre = (uint16_t)_mapped * zone + zone / 2;
        int16_t  dist       = abs((int16_t)_smooth - (int16_t)cur_centre);
        if (dist > (int16_t)(zone * 5 / 8)) {
            _mapped = idx;
        }
    }
    return _mapped;
}

bool pot_moved() {
    int16_t delta = (int16_t)_smooth - (int16_t)_idle_ref;
    if (abs(delta) > 150) {          // ~3.5 % of full range
        _idle_ref = _smooth;
        return true;
    }
    return false;
}
