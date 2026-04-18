#include "led.h"

void led_init() {
    pinMode(LED_PIN, OUTPUT);
    led_off();
}

void led_set(uint8_t r, uint8_t g, uint8_t b) {
    neopixelWrite(LED_PIN, r, g, b);
}

void led_off()    { led_set(0, 0, 0); }
void led_green()  { led_set(0, 25, 0); }
void led_red()    { led_set(25, 0, 0); }
void led_blue()   { led_set(0, 0, 25); }
void led_yellow() { led_set(25, 18, 0); }
void led_cyan()   { led_set(0, 18, 25); }
void led_white()  { led_set(15, 15, 15); }

void led_blink(uint8_t r, uint8_t g, uint8_t b, int count, int on_ms, int off_ms) {
    for (int i = 0; i < count; i++) {
        led_set(r, g, b);
        delay(on_ms);
        led_off();
        if (i < count - 1) delay(off_ms);
    }
}
