#include "display.h"

// =============================================================================
// Display driver — edit this section to match your display module.
// Common options:
//   Adafruit ST7735:   #include <Adafruit_ST7735.h>
//   Adafruit ST7789:   #include <Adafruit_ST7789.h>
//   Adafruit ILI9341:  #include <Adafruit_ILI9341.h>
// =============================================================================
// Uncomment and adjust the block below for your display:
//
// #include <Adafruit_GFX.h>
// #include <Adafruit_ST7735.h>
// static Adafruit_ST7735 _tft(DISPLAY_CS, DISPLAY_DC, DISPLAY_MOSI,
//                              DISPLAY_SCLK, DISPLAY_RES);
//
// Or using TFT_eSPI (configure User_Setup.h for pin assignments):
// #include <TFT_eSPI.h>
// static TFT_eSPI _tft;
// =============================================================================

// Placeholder colour constants — remove once a real driver is included.
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_GREEN   0x07E0
#define COLOR_RED     0xF800
#define TEXT_SIZE     1
#define ROW_HEIGHT_PX 10

void display_init() {
    // TODO: Replace with driver-specific init, e.g.:
    // _tft.initR(INITR_BLACKTAB);
    // _tft.setRotation(1);
    // _tft.fillScreen(COLOR_BLACK);
    Serial.println("[display] init placeholder — add your driver");
}

void display_clear() {
    // TODO: _tft.fillScreen(COLOR_BLACK);
}

void display_print_line(uint8_t row, const String &text) {
    // TODO:
    // _tft.setCursor(0, row * ROW_HEIGHT_PX);
    // _tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
    // _tft.setTextSize(TEXT_SIZE);
    // _tft.println(text);
    Serial.printf("[display] row %d: %s\n", row, text.c_str());
}

void display_status(uint8_t row, const char *label, float value) {
    String line = String(label) + ": " + String(value, 2);
    display_print_line(row, line);
}

void display_update() {
    // Required only for buffered drivers (e.g., TFT_eSPI with sprites).
    // If using Adafruit GFX direct-draw, this is a no-op.
}
