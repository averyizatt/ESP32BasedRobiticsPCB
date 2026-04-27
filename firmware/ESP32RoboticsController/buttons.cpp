#include "buttons.h"

static const uint8_t _btn_pins[] = {BUTTON1_PIN, BUTTON2_PIN};
static constexpr uint8_t BTN_COUNT = sizeof(_btn_pins) / sizeof(_btn_pins[0]);

struct ButtonState {
    bool current;           // debounced state (true = pressed)
    bool previous;          // debounced state from last update
    bool raw;               // raw GPIO read (true = pressed)
    bool press_event;       // latched debounced press edge
    bool release_event;     // latched debounced release edge
    unsigned long last_change_ms;
    unsigned long pressed_at_ms;  // when debounced press started
    unsigned long press_event_ms;
    unsigned long release_event_ms;
};

static ButtonState _states[BTN_COUNT] = {};

void buttons_init() {
    for (int i = 0; i < BTN_COUNT; i++) {
        pinMode(_btn_pins[i], INPUT_PULLUP);
        _states[i].raw = false;
        _states[i].current = false;
        _states[i].previous = false;
        _states[i].press_event = false;
        _states[i].release_event = false;
        _states[i].last_change_ms = 0;
        _states[i].pressed_at_ms = 0;
        _states[i].press_event_ms = 0;
        _states[i].release_event_ms = 0;
    }
}

void buttons_update() {
    unsigned long now = millis();
    for (int i = 0; i < BTN_COUNT; i++) {
        bool raw = (digitalRead(_btn_pins[i]) == LOW); // Active LOW with pull-up
        if (raw != _states[i].raw) {
            _states[i].raw = raw;
            _states[i].last_change_ms = now;
        }
        bool old_current = _states[i].current;
        _states[i].previous = old_current;
        bool new_state = _states[i].current;
        if ((now - _states[i].last_change_ms) >= BUTTON_DEBOUNCE_MS) {
            new_state = _states[i].raw;
        }
        if (new_state && !old_current) {
            _states[i].pressed_at_ms = now;  // record when press debounced
            _states[i].press_event = true;
            _states[i].press_event_ms = now;
            Serial.printf("[buttons] BTN%d press\n", i + 1);
        }
        if (!new_state && old_current) {
            _states[i].release_event = true;
            _states[i].release_event_ms = now;
            Serial.printf("[buttons] BTN%d release\n", i + 1);
        }
        _states[i].current = new_state;
    }
}

bool button_is_pressed(ButtonId btn) {
    uint8_t idx = (uint8_t)btn;
    if (idx >= BTN_COUNT) return false;
    return _states[idx].current;
}

bool button_just_pressed(ButtonId btn) {
    uint8_t idx = (uint8_t)btn;
    if (idx >= BTN_COUNT) return false;
    ButtonState &s = _states[idx];
    if (!s.press_event) return false;
    s.press_event = false;
    return (millis() - s.press_event_ms) <= 250UL;
}

bool button_just_released(ButtonId btn) {
    uint8_t idx = (uint8_t)btn;
    if (idx >= BTN_COUNT) return false;
    ButtonState &s = _states[idx];
    if (!s.release_event) return false;
    s.release_event = false;
    return (millis() - s.release_event_ms) <= 250UL;
}

unsigned long button_held_ms(ButtonId btn) {
    uint8_t idx = (uint8_t)btn;
    if (idx >= BTN_COUNT || !_states[idx].current) return 0UL;
    return millis() - _states[idx].pressed_at_ms;
}

bool nav_enter_is_pressed() {
    return button_is_pressed(BTN_ENTER);
}

bool nav_enter_just_pressed() {
    return button_just_pressed(BTN_ENTER);
}

bool nav_back_is_pressed() {
    return button_is_pressed(BTN_BACK);
}

bool nav_back_just_pressed() {
    return button_just_pressed(BTN_BACK);
}
