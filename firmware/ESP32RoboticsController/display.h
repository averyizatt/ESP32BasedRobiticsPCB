#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"

// =============================================================================
// SPI Display Interface — 0.96" TFT (160×80, ST7735S)
// =============================================================================

// --- Panel geometry ----------------------------------------------------------
static constexpr int16_t  DISP_W    = 160;   // pixels wide
static constexpr int16_t  DISP_H    =  80;   // pixels tall
static constexpr uint8_t  ROW_PX    =   8;   // pixels per text row (size-1 font)

// --- Colour palette (RGB565) — Material Design dark theme ------------------
// Values are pre-inverted (XOR 0xFFFF) to compensate for the ST7735S panel's
// hardware colour inversion that cannot be overridden via software INVON.
static constexpr uint16_t C_BLACK    = 0xFFFF;   // sends white → displays black
static constexpr uint16_t C_WHITE    = 0x0000;   // sends black → displays white
static constexpr uint16_t C_GREEN    = 0xF81F;
static constexpr uint16_t C_RED      = 0x07FF;
static constexpr uint16_t C_CYAN     = 0xF800;
static constexpr uint16_t C_YELLOW   = 0x001F;
static constexpr uint16_t C_MAGENTA  = 0x07E0;
static constexpr uint16_t C_PURPLE   = 0x7FE0;
static constexpr uint16_t C_ORANGE   = 0x02DF;
static constexpr uint16_t C_GREY     = 0x8410;   // medium emphasis text
static constexpr uint16_t C_DKGREY   = 0xC618;   // borders, disabled text
static constexpr uint16_t C_LTGREY   = 0x39E7;   // secondary text
static constexpr uint16_t C_NAVY     = 0xFFF0;
static constexpr uint16_t C_DKBLUE   = 0xFCE7;
static constexpr uint16_t C_BLUE     = 0xFFE0;
static constexpr uint16_t C_ACCENT   = 0xA325;   // steel blue  (#5B99D5)
static constexpr uint16_t C_ACCENT2  = 0x12B9;   // warm amber  (#EEAA31)
static constexpr uint16_t C_BG       = 0xEF7D;   // charcoal    (#121212)
static constexpr uint16_t C_PANEL    = 0xD6BA;   // elevated surface (#282830)
static constexpr uint16_t C_HILIGHT  = 0x81C2;   // light blue  (#7BC6EE)

// Helper: make RGB565 from 5-bit R, 6-bit G, 5-bit B
static inline constexpr uint16_t RGB565(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint16_t)(r & 0x1F) << 11) | ((uint16_t)(g & 0x3F) << 5) | (b & 0x1F);
}

// Blend two RGB565 colours — alpha 0=a, 255=b
uint16_t colour_blend(uint16_t a, uint16_t b, uint8_t alpha);

// --- Low-level driver --------------------------------------------------------
void display_init();
void display_fill(uint16_t colour);
void display_update();

// --- Primitives --------------------------------------------------------------
void display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colour);
void display_hline(int16_t x, int16_t y, int16_t w, uint16_t colour);
void display_vline(int16_t x, int16_t y, int16_t h, uint16_t colour);
void display_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colour);
void display_pixel(int16_t x, int16_t y, uint16_t colour);
void display_circle(int16_t cx, int16_t cy, int16_t r, uint16_t colour);
void display_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t colour);
void display_triangle(int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t colour);
void display_fill_triangle(int16_t x0, int16_t y0,
                           int16_t x1, int16_t y1,
                           int16_t x2, int16_t y2, uint16_t colour);

// Draw a string at pixel coordinates. size 1=6×8, 2=12×16.
void display_text(int16_t x, int16_t y, const char *str,
                  uint16_t fg, uint16_t bg, uint8_t size = 1);

// Draw centred string inside a pixel-coordinate box [x, x+w).
void display_text_centred(int16_t x, int16_t y, int16_t w,
                          const char *str, uint16_t fg, uint16_t bg,
                          uint8_t size = 1);

// --- High-level UI -----------------------------------------------------------
void display_clear();
void display_clear_bg();   // fill with C_BG instead of black

// Frame batching — bracket a block of draw calls to hold SPI CS asserted
// for the whole frame. TFT_eSPI nests these safely so inner primitives
// don't pay the CS assert/deassert overhead on each call.
void display_begin_frame();
void display_end_frame();

// Header bar with gradient-look (two-tone) and bottom accent line.
void display_header(const char *title, uint8_t height = 13);

// Thin separator line.
void display_separator(int16_t y, uint16_t colour = C_DKGREY);

// Print a raw text row (row × ROW_PX pixels from top).
void display_row(uint8_t row, const char *text,
                 uint16_t fg = C_WHITE, uint16_t bg = C_BG);

// Styled key-value row with right-aligned coloured value.
void display_kv(int16_t px_y, const char *key, const char *value,
                uint16_t val_colour = C_ACCENT);

// Boot self-test row: spinner → OK/ERR badge.
void display_check(uint8_t row, const char *label, bool ok);

// Smooth horizontal progress bar at pixel y.
void display_bar(int16_t y, uint8_t percent, uint16_t fill_colour = C_ACCENT);

// Styled footer with two labelled button hints.
void display_footer(const char *left_hint, const char *right_hint);

// Expose raw TFT handle for sprite-based rendering (e.g. games).
class TFT_eSPI;
TFT_eSPI *display_get_tft();

// Raw TFT handle — for TFT_eSprite creation in games
TFT_eSPI *display_get_tft();
