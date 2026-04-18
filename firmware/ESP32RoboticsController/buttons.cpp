#include "buttons.h"

static const uint8_t _btn_pins[2] = {BUTTON1_PIN, BUTTON2_PIN};

struct ButtonState {
    bool current;           // debounced state (true = pressed)
    bool previous;          // debounced state from last update
    bool raw;               // raw GPIO read (true = pressed)
    unsigned long last_change_ms;
    unsigned long pressed_at_ms;  // when debounced press started
};

static ButtonState _states[2] = {};

void buttons_init() {
    for (int i = 0; i < 2; i++) {
        pinMode(_btn_pins[i], INPUT_PULLUP);
        _states[i].raw = false;
        _states[i].current = false;
        _states[i].previous = false;
        _states[i].last_change_ms = 0;
    }
}

void buttons_update() {
    unsigned long now = millis();
    for (int i = 0; i < 2; i++) {
        bool raw = (digitalRead(_btn_pins[i]) == LOW); // Active LOW
        if (raw != _states[i].raw) {
            _states[i].raw = raw;
            _states[i].last_change_ms = now;
        }
        _states[i].previous = _states[i].current;
        bool new_state = _states[i].current;
        if ((now - _states[i].last_change_ms) >= BUTTON_DEBOUNCE_MS) {
            new_state = _states[i].raw;
        }
        if (new_state && !_states[i].current) {
            _states[i].pressed_at_ms = now;  // record when press debounced
        }
        _states[i].current = new_state;
    }
}

bool button_is_pressed(ButtonId btn) {
    return _states[(int)btn].current;
}

bool button_just_pressed(ButtonId btn) {
    return _states[(int)btn].current && !_states[(int)btn].previous;
}

bool button_just_released(ButtonId btn) {
    return !_states[(int)btn].current && _states[(int)btn].previous;
}

unsigned long button_held_ms(ButtonId btn) {
    if (!_states[(int)btn].current) return 0UL;
    return millis() - _states[(int)btn].pressed_at_ms;
}
