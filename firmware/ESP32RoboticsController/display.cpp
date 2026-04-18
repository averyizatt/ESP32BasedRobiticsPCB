#include "display.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// =============================================================================
// Display driver — ST7735S 0.96" 160×80, hardware SPI (FSPI)
// Actual rendered resolution after setRotation(3): 160×80 px.
// =============================================================================

static SPIClass        _hspi(FSPI);
static Adafruit_ST7735 _tft(&_hspi, DISPLAY_CS, DISPLAY_DC, DISPLAY_RES);

void display_init() {
    Serial.println("[disp] init — ST7735S HW-SPI (FSPI)");
    Serial.printf("[disp]  MOSI=%d  SCLK=%d  CS=%d  DC=%d  RES=%d\n",
                  DISPLAY_MOSI, DISPLAY_SCLK, DISPLAY_CS, DISPLAY_DC, DISPLAY_RES);

    _hspi.begin(DISPLAY_SCLK, -1, DISPLAY_MOSI, DISPLAY_CS);

    pinMode(DISPLAY_RES, OUTPUT);
    digitalWrite(DISPLAY_RES, HIGH); delay(50);
    digitalWrite(DISPLAY_RES, LOW);  delay(100);
    digitalWrite(DISPLAY_RES, HIGH); delay(200);

    _tft.initR(INITR_MINI160x80);
    _tft.setRotation(3);
    _tft.invertDisplay(true);
    _tft.fillScreen(C_BLACK);
    _tft.setTextWrap(false);
    Serial.println("[disp] ready  160x80");
}

void display_fill(uint16_t colour)  { _tft.fillScreen(colour); }
void display_update()               { /* direct-draw, nothing to flush */ }

// --- Colour utility ----------------------------------------------------------

uint16_t colour_blend(uint16_t a, uint16_t b, uint8_t alpha) {
    uint8_t ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    uint8_t br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    uint8_t r = ar + ((int16_t)(br - ar) * alpha) / 255;
    uint8_t g = ag + ((int16_t)(bg - ag) * alpha) / 255;
    uint8_t bv = ab + ((int16_t)(bb - ab) * alpha) / 255;
    return ((uint16_t)r << 11) | ((uint16_t)g << 5) | bv;
}

// --- Primitives --------------------------------------------------------------

void display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colour) {
    _tft.fillRect(x, y, w, h, colour);
}
void display_hline(int16_t x, int16_t y, int16_t w, uint16_t colour) {
    _tft.drawFastHLine(x, y, w, colour);
}
void display_vline(int16_t x, int16_t y, int16_t h, uint16_t colour) {
    _tft.drawFastVLine(x, y, h, colour);
}
void display_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colour) {
    _tft.drawRect(x, y, w, h, colour);
}
void display_pixel(int16_t x, int16_t y, uint16_t colour) {
    _tft.drawPixel(x, y, colour);
}
void display_circle(int16_t cx, int16_t cy, int16_t r, uint16_t colour) {
    _tft.drawCircle(cx, cy, r, colour);
}
void display_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t colour) {
    _tft.fillCircle(cx, cy, r, colour);
}
void display_triangle(int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t colour) {
    _tft.drawTriangle(x0, y0, x1, y1, x2, y2, colour);
}

void display_text(int16_t x, int16_t y, const char *str,
                  uint16_t fg, uint16_t bg, uint8_t size) {
    _tft.setCursor(x, y);
    _tft.setTextColor(fg, bg);
    _tft.setTextSize(size);
    _tft.print(str);
}

void display_text_centred(int16_t x, int16_t y, int16_t w,
                          const char *str, uint16_t fg, uint16_t bg,
                          uint8_t size) {
    int16_t char_w = 6 * size;
    int16_t tx = x + (w - (int16_t)(strlen(str)) * char_w) / 2;
    display_text(tx < x ? x : tx, y, str, fg, bg, size);
}

// --- High-level UI -----------------------------------------------------------

void display_clear()    { _tft.fillScreen(C_BLACK); }
void display_clear_bg() { _tft.fillScreen(C_BG); }

void display_header(const char *title, uint8_t height) {
    // Flat dark header bar
    display_fill_rect(0, 0, DISP_W, height, C_PANEL);
    // Green accent bottom line
    display_hline(0, height, DISP_W, C_ACCENT);
    // Title centred in white
    int16_t tx = (DISP_W - (int16_t)strlen(title) * 6) / 2;
    int16_t ty = (height - 8) / 2;
    display_text(tx < 2 ? 2 : tx, ty < 0 ? 0 : ty, title, C_WHITE, C_PANEL, 1);
}

void display_separator(int16_t y, uint16_t colour) {
    display_hline(0, y, DISP_W, colour);
}

void display_row(uint8_t row, const char *text, uint16_t fg, uint16_t bg) {
    int16_t y = row * ROW_PX;
    display_fill_rect(0, y, DISP_W, ROW_PX, bg);
    display_text(2, y, text, fg, bg, 1);
}

void display_kv(int16_t px_y, const char *key, const char *value,
                uint16_t val_colour) {
    display_fill_rect(0, px_y, DISP_W, ROW_PX, C_BG);
    display_text(3, px_y, key, C_GREY, C_BG, 1);
    int16_t vx = DISP_W - (int16_t)(strlen(value) * 6) - 3;
    if (vx < 1) vx = 1;
    display_text(vx, px_y, value, val_colour, C_BG, 1);
    // subtle dotted separator between key and value
    display_hline(0, px_y + ROW_PX - 1, DISP_W, C_PANEL);
}

void display_check(uint8_t row, const char *label, bool ok) {
    int16_t y = row * ROW_PX;
    display_fill_rect(0, y, DISP_W, ROW_PX, C_BLACK);
    display_text(2, y, label, C_WHITE, C_BLACK, 1);
    const char *badge  = ok ? "OK " : "ERR";
    uint16_t    badge_c = ok ? C_GREEN : C_RED;
    int16_t     bx      = DISP_W - 3 * 6 - 3;
    display_text(bx, y, badge, badge_c, C_BLACK, 1);
}

void display_bar(int16_t y, uint8_t percent, uint16_t fill_colour) {
    int16_t total_w = DISP_W - 6;
    int16_t filled  = (int16_t)((total_w * (uint16_t)percent) / 100U);
    display_fill_rect(3, y, total_w, 5, C_DKGREY);
    if (filled > 0)
        display_fill_rect(3, y, filled, 5, fill_colour);
    // highlight top edge
    if (filled > 0)
        display_hline(3, y, filled, colour_blend(fill_colour, C_WHITE, 80));
}

void display_footer(const char *left_hint, const char *right_hint) {
    int16_t y = DISP_H - ROW_PX - 1;
    // Thin separator line
    display_hline(0, y, DISP_W, C_DKGREY);
    display_fill_rect(0, y + 1, DISP_W, ROW_PX, C_BG);
    // Left label
    display_text(3, y + 1, left_hint, C_GREY, C_BG, 1);
    // Right label (primary action — accent colour)
    int16_t rx = DISP_W - (int16_t)(strlen(right_hint) * 6) - 3;
    display_text(rx < 3 ? 3 : rx, y + 1, right_hint, C_ACCENT, C_BG, 1);
}

