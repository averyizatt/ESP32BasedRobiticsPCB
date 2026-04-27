#include "ui.h"
#include "display.h"
#include "buttons.h"
#include "robot_modes.h"
#include "games.h"
#include "motors.h"
#include "servos.h"
#include "ultrasonics.h"
#include "hall_sensors.h"
#include "imu.h"
#include "led.h"
#include "pot.h"
#include "wifi_server.h"
#include <WiFi.h>
#include <math.h>

// =============================================================================
// UI — 160×80 Full-colour TFT  |  2-button navigation
// =============================================================================
//  Layout conventions:
//    Header:    y = 0 – 12   (13 px)
//    Accent line: y = 13     (1 px, drawn by display_header)
//    Content:   y = 14 – 70
//    Footer:    y = 71 – 79  (9 px, drawn by display_footer)
//
//  Buttons:
//    BTN_CYCLE  (TOP,    GPIO 41) → short press = next/advance | long press = enter/select
//    BTN_SELECT (BOTTOM, GPIO 42) → press = back (all screens)
// =============================================================================

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------
static constexpr int16_t HDR_H    = 13;   // header height
static constexpr int16_t HDR_BOT  = HDR_H + 1;   // first content pixel y
static constexpr int16_t FOOT_Y   = DISP_H - ROW_PX - 1;  // footer top y
static constexpr int16_t CONTENT_H = FOOT_Y - HDR_BOT;    // usable content height

// ---------------------------------------------------------------------------
// App states
// ---------------------------------------------------------------------------
enum class UiState : uint8_t {
    MENU,
    SCREEN_SENSORS,
    SCREEN_IMU,
    SCREEN_IMU_DEMO,    // liquid / tilt demo
    SCREEN_MOTORS,
    SCREEN_SYSTEM,
    DEMO_ULTRASONIC,    // ultrasonic radar demo
    DEMO_HALL,          // hall effect / wheel speed demo
    DEMO_SERVO,         // servo sweep demo
    SCREEN_GAMES,       // arcade games sub-menu
    SCREEN_ROBOT_MODES, // autonomous robot mode selector
    SCREEN_WIFI,        // WiFi AP info
    SCREEN_WIFI_SCAN,   // WiFi network scanner
    SCREEN_BLE_SCAN,    // BLE device radar
    IDLE,
};

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static UiState     _state        = UiState::MENU;
static bool        _imu_ok       = false;
static bool        _needs_redraw = true;

static constexpr unsigned long IDLE_TIMEOUT_MS = 20000UL;
static unsigned long _last_activity = 0;

static inline void _mark_active() {
    _last_activity = millis();
    if (_state == UiState::IDLE) {
        _state        = UiState::MENU;
        _needs_redraw = true;
        led_green();
    }
}

// ---------------------------------------------------------------------------
// Menu definition
// ---------------------------------------------------------------------------
struct MenuItem {
    const char *label;
    const char *icon;    // 1-char glyph (ASCII art stand-in)
    UiState     target;
};

static const MenuItem MENU_ITEMS[] = {
    { "Sensors",   "~", UiState::SCREEN_SENSORS   },
    { "IMU Data",  "#", UiState::SCREEN_IMU       },
    { "Tilt Demo", "*", UiState::SCREEN_IMU_DEMO  },
    { "Motors",    ">", UiState::SCREEN_MOTORS    },
    { "Ultrasonic","U", UiState::DEMO_ULTRASONIC  },
    { "Hall Demo", "H", UiState::DEMO_HALL        },
    { "Servo Demo","S", UiState::DEMO_SERVO       },
    { "System",    "i", UiState::SCREEN_SYSTEM    },
    { "Games",      "G", UiState::SCREEN_GAMES      },
    { "Robot Modes", "R", UiState::SCREEN_ROBOT_MODES },
    { "WiFi Info",   "W", UiState::SCREEN_WIFI        },
    { "WiFi Scan",  "w", UiState::SCREEN_WIFI_SCAN  },
    { "BLE Radar",  "B", UiState::SCREEN_BLE_SCAN   },
};
static constexpr uint8_t MENU_LEN = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);
static uint8_t _menu_idx = 0;
static uint8_t _menu_scroll = 0;

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Splash starfield — shared between boot animation and idle screen
// ---------------------------------------------------------------------------
struct _SStar { int16_t x; int8_t y; uint8_t spd; uint8_t bri; };
static _SStar _ss[32];
static bool   _ss_init = false;

static void _splash_stars_reset() {
    for (auto &s : _ss) {
        s.x   = (int16_t)random(0, DISP_W);
        s.y   = (int8_t) random(0, DISP_H);
        s.spd = (uint8_t)random(1, 4);
        s.bri = (uint8_t)random(25, 140);
    }
    _ss_init = true;
}

static void _splash_stars_tick(TFT_eSprite &spr) {
    for (auto &s : _ss) {
        s.x -= s.spd;
        if (s.x < 0) {
            s.x   = DISP_W - 1;
            s.y   = (int8_t)random(0, DISP_H);
            s.bri = (uint8_t)random(25, 140);
            s.spd = (uint8_t)random(1, 4);
        }
        uint8_t b = s.bri + (s.spd - 1) * 25;
        spr.drawPixel(s.x, s.y, colour_blend(C_BG, C_GREY, b));
    }
}

// Direct-draw (no sprite) version — used by boot splash
static void _stars_tick_direct() {
    TFT_eSPI *tft = display_get_tft();
    for (auto &s : _ss) {
        // Erase full trail — spd pixels wide, otherwise fast stars leave streaks
        for (uint8_t dx = 0; dx < s.spd; dx++) {
            int16_t ex = s.x - (int16_t)dx;
            if (ex >= 0) tft->drawPixel(ex, s.y, C_BG);
        }
        s.x -= s.spd;
        if (s.x < 0) {
            s.x   = DISP_W - 1;
            s.y   = (int8_t)random(0, DISP_H);
            s.bri = (uint8_t)random(25, 140);
            s.spd = (uint8_t)random(1, 4);
        }
        uint8_t b = s.bri + (s.spd - 1) * 25;
        tft->drawPixel(s.x, s.y, colour_blend(C_BG, C_GREY, b));
    }
}

// Procedural IC / chip logo — direct-draw (no sprite, works on all panels)
static void _logo_draw_direct(int16_t cx, int16_t cy) {
    TFT_eSPI *t = display_get_tft();
    t->drawCircle(cx, cy, 16, colour_blend(C_BG, C_ACCENT, 40));
    t->drawCircle(cx, cy, 15, colour_blend(C_BG, C_ACCENT, 75));
    t->fillRect(cx - 10, cy - 7, 20, 14, C_PANEL);
    t->drawRect(cx - 10, cy - 7, 20, 14, C_ACCENT);
    t->fillRect(cx - 5, cy - 3, 10, 6, colour_blend(C_BG, C_PANEL, 180));
    t->drawFastHLine(cx - 3, cy,     6, C_ACCENT);
    t->drawFastVLine(cx,     cy - 2, 5, C_ACCENT);
    t->drawPixel(cx, cy, C_ACCENT2);
    for (int8_t d = -4; d <= 4; d += 4) {
        t->drawFastHLine(cx - 13, cy + d, 3, C_ACCENT);
        t->drawFastHLine(cx + 10, cy + d, 3, C_ACCENT);
    }
    t->drawFastVLine(cx - 4, cy - 10, 3, C_ACCENT);
    t->drawFastVLine(cx + 4, cy - 10, 3, C_ACCENT);
    t->drawFastVLine(cx - 4, cy +  7, 3, C_ACCENT);
    t->drawFastVLine(cx + 4, cy +  7, 3, C_ACCENT);
    const int16_t  vx[4] = { (int16_t)(cx-18), (int16_t)(cx+18), (int16_t)(cx-18), (int16_t)(cx+18) };
    const int16_t  vy[4] = { (int16_t)(cy-12), (int16_t)(cy-12), (int16_t)(cy+12), (int16_t)(cy+12) };
    const int16_t  px[4] = { (int16_t)(cx-10), (int16_t)(cx+10), (int16_t)(cx-10), (int16_t)(cx+10) };
    const int16_t  py[4] = { (int16_t)(cy- 7), (int16_t)(cy- 7), (int16_t)(cy+ 7), (int16_t)(cy+ 7) };
    const uint16_t vc[4] = { C_ACCENT2, C_ACCENT2, C_DKGREY, C_DKGREY };
    for (uint8_t i = 0; i < 4; i++) {
        t->drawLine(px[i], py[i], vx[i], vy[i], colour_blend(C_BG, C_DKGREY, 180));
        t->fillCircle(vx[i], vy[i], 2, C_BG);
        t->drawCircle(vx[i], vy[i], 2, vc[i]);
    }
}

// Procedural IC / chip logo drawn into a sprite
static void _logo_draw(TFT_eSprite &spr, int16_t cx, int16_t cy) {
    // Subtle outer glow rings
    spr.drawCircle(cx, cy, 16, colour_blend(C_BG, C_ACCENT, 40));
    spr.drawCircle(cx, cy, 15, colour_blend(C_BG, C_ACCENT, 75));
    // Chip body
    spr.fillRect(cx - 10, cy - 7, 20, 14, C_PANEL);
    spr.drawRect(cx - 10, cy - 7, 20, 14, C_ACCENT);
    // Inner die
    spr.fillRect(cx - 5, cy - 3, 10, 6, colour_blend(C_BG, C_PANEL, 180));
    // Center crosshair
    spr.drawFastHLine(cx - 3, cy,     6, C_ACCENT);
    spr.drawFastVLine(cx,     cy - 2, 5, C_ACCENT);
    spr.drawPixel(cx, cy, C_ACCENT2);
    // Pins — 3 on each long side
    for (int8_t d = -4; d <= 4; d += 4) {
        spr.drawFastHLine(cx - 13, cy + d, 3, C_ACCENT);
        spr.drawFastHLine(cx + 10, cy + d, 3, C_ACCENT);
    }
    // Pins — 2 on each short side
    spr.drawFastVLine(cx - 4, cy - 10, 3, C_ACCENT);
    spr.drawFastVLine(cx + 4, cy - 10, 3, C_ACCENT);
    spr.drawFastVLine(cx - 4, cy +  7, 3, C_ACCENT);
    spr.drawFastVLine(cx + 4, cy +  7, 3, C_ACCENT);
    // Corner vias + diagonal traces
    const int16_t  vx[4] = { (int16_t)(cx-18), (int16_t)(cx+18), (int16_t)(cx-18), (int16_t)(cx+18) };
    const int16_t  vy[4] = { (int16_t)(cy-12), (int16_t)(cy-12), (int16_t)(cy+12), (int16_t)(cy+12) };
    const int16_t  px[4] = { (int16_t)(cx-10), (int16_t)(cx+10), (int16_t)(cx-10), (int16_t)(cx+10) };
    const int16_t  py[4] = { (int16_t)(cy- 7), (int16_t)(cy- 7), (int16_t)(cy+ 7), (int16_t)(cy+ 7) };
    const uint16_t vc[4] = { C_ACCENT2, C_ACCENT2, C_DKGREY, C_DKGREY };
    for (uint8_t i = 0; i < 4; i++) {
        spr.drawLine(px[i], py[i], vx[i], vy[i], colour_blend(C_BG, C_DKGREY, 180));
        spr.fillCircle(vx[i], vy[i], 2, C_BG);
        spr.drawCircle(vx[i], vy[i], 2, vc[i]);
    }
}

// ---------------------------------------------------------------------------
// Boot helpers
// ---------------------------------------------------------------------------

static void _boot_bar_anim(int16_t bar_y, uint8_t from_pct, uint8_t to_pct) {
    for (uint8_t p = from_pct; p <= to_pct; p++) {
        display_bar(bar_y, p, C_ACCENT);
        delay(9);
    }
}

static bool _boot_step(uint8_t row, const char *label, bool (*fn)()) {
    static const char spinner[] = { '-', '\\', '|', '/' };
    char buf[22];
    for (int t = 0; t < 4; t++) {
        snprintf(buf, sizeof(buf), "%-14s %c", label, spinner[t]);
        display_row(row, buf, C_GREY, C_BG);
        delay(55);
    }
    bool ok = fn ? fn() : true;
    display_check(row, label, ok);
    Serial.printf("[boot] %-14s %s\n", label, ok ? "OK" : "FAIL");
    return ok;
}

static bool _w_motors()  { motors_init();      return true; }
static bool _w_servos()  { servos_init();       return true; }
static bool _w_us()      { ultrasonics_init();  return true; }
static bool _w_hall()    { hall_sensors_init(); return true; }
static bool _w_buttons() { buttons_init();      return true; }

// ---------------------------------------------------------------------------
// Boot sequence
// ---------------------------------------------------------------------------
void ui_boot(bool imu_ok) {
    _imu_ok = imu_ok;
    _splash_stars_reset();

    // ── Animated splash — direct-draw (no sprite push, avoids GRAM offset issues)
    {
        const char    *title    = "ROBOT CTRL";
        const uint8_t  tlen     = (uint8_t)strlen(title);
        const char    *sub      = "ESP32 Robotics v1";
        const char    *sub2     = "by Avery Izatt";
        const int16_t  logo_cx  = DISP_W / 2;
        const int16_t  logo_cy  = 20;
        const int16_t  title_y  = 43;
        const int16_t  sub_y    = 55;
        const int16_t  sub2_y   = 64;
        uint8_t        chars_shown = 0;
        bool           logo_drawn  = false;
        bool           subs_drawn  = false;

        // Initial background
        display_fill(C_BG);
        for (int16_t y = 0; y < DISP_H; y += 4)
            display_hline(0, y, DISP_W, colour_blend(C_BG, C_BLACK, 45));
        display_fill_rect(0, 0,          DISP_W, 3, C_ACCENT);
        display_fill_rect(0, DISP_H - 3, DISP_W, 3, C_ACCENT);
        display_hline(0, 3,          DISP_W, colour_blend(C_ACCENT, C_BG, 140));
        display_hline(0, DISP_H - 4, DISP_W, colour_blend(C_ACCENT, C_BG, 140));

        unsigned long frame_ms = millis();
        for (uint8_t frame = 0; frame < 120; frame++) {
            display_begin_frame();

            // Advance typewriter counter (one char per frame ≥ 30)
            if (frame >= 30 && chars_shown < tlen) chars_shown++;
            // Mark subs ready once title fully typed
            if (chars_shown == tlen && !subs_drawn) subs_drawn = true;
            // Mark logo ready at frame 15
            if (frame == 15) logo_drawn = true;

            // Tick stars (erases old positions, draws new)
            _stars_tick_direct();

            // Re-composite static elements on top so stars never trail over them
            if (logo_drawn) {
                _logo_draw_direct(logo_cx, logo_cy);
            }
            if (chars_shown > 0) {
                char tmp[12];
                memcpy(tmp, title, chars_shown);
                tmp[chars_shown] = '\0';
                display_fill_rect(0, title_y, DISP_W, 10, C_BG);
                int16_t tx = (DISP_W - (int16_t)chars_shown * 6) / 2;
                display_text(tx < 2 ? 2 : tx, title_y, tmp, C_ACCENT, C_BG, 1);
                if (chars_shown == tlen) {
                    int16_t ul_w = tlen * 6;
                    int16_t ul_x = (DISP_W - ul_w) / 2;
                    display_hline(ul_x,     title_y + 10, ul_w,     C_ACCENT);
                    display_hline(ul_x + 2, title_y + 11, ul_w - 4, colour_blend(C_ACCENT, C_BG, 160));
                }
            }
            if (subs_drawn) {
                display_text((DISP_W - (int16_t)strlen(sub)  * 6) / 2, sub_y,  sub,  C_DKGREY, C_BG, 1);
                display_text((DISP_W - (int16_t)strlen(sub2) * 6) / 2, sub2_y, sub2, colour_blend(C_ACCENT, C_BG, 100), C_BG, 1);
            }

            // Re-draw accent bars (always on top)
            display_fill_rect(0, 0,          DISP_W, 3, C_ACCENT);
            display_fill_rect(0, DISP_H - 3, DISP_W, 3, C_ACCENT);

            display_end_frame();

            // Pace to ~25 fps
            unsigned long now = millis();
            if (now - frame_ms < 40) delay(40 - (now - frame_ms));
            frame_ms = millis();
        }
    }

    // ── Self-test ────────────────────────────────────────────────────────────
    led_yellow();
    display_clear();
    display_header("SELF-TEST", HDR_H);

    uint8_t r = 2;
    _boot_step(r++, "Motors",     _w_motors);
    _boot_step(r++, "Servos",     _w_servos);
    _boot_step(r++, "Ultrasonic", _w_us);
    _boot_step(r++, "Hall",       _w_hall);
    _boot_step(r++, "Buttons",    _w_buttons);

    display_row(r, "IMU probe...  ", C_GREY, C_BG);
    delay(300);
    display_check(r++, "IMU", _imu_ok);
    Serial.printf("[boot] IMU %s\n", _imu_ok ? "OK" : "NOT FOUND");
    if (!_imu_ok) {
        display_row(r, "Addr 0x68/0x69?", C_YELLOW, C_BG);
        delay(700);
    }

    // ── Progress bar ─────────────────────────────────────────────────────────
    led_green();
    _boot_bar_anim(DISP_H - 8, 0, 100);
    delay(250);

    // ── Ready banner ─────────────────────────────────────────────────────────
    display_fill(C_BG);
    display_fill_rect(0, 0,          DISP_W, 3, C_ACCENT);
    display_fill_rect(0, DISP_H - 3, DISP_W, 3, C_ACCENT);
    {
        const char *ready = "READY";
        int16_t rx = (DISP_W - (int16_t)strlen(ready) * 12) / 2;
        int16_t ry = (DISP_H / 2) - 8;
        // Stars continue behind the ready card
        for (uint8_t f = 0; f < 15; f++) {
            display_begin_frame();
            _stars_tick_direct();
            display_fill_rect(0, 0,          DISP_W, 3, C_ACCENT);
            display_fill_rect(0, DISP_H - 3, DISP_W, 3, C_ACCENT);
            display_end_frame();
            delay(30);
        }
        display_fill_rect(0, ry - 5, DISP_W, 26, C_PANEL);
        display_hline(0, ry - 5,  DISP_W, C_ACCENT);
        display_hline(0, ry + 20, DISP_W, C_ACCENT);
        display_text(rx, ry, ready, C_ACCENT, C_PANEL, 2);
        delay(700);
    }

    display_clear_bg();
    _needs_redraw  = true;
    _menu_idx      = 0;
    _menu_scroll   = 0;
    _state         = UiState::MENU;
    _last_activity = millis();
    led_green();
    Serial.println("[boot] done — entering menu");
}

// ---------------------------------------------------------------------------
// Menu — card-style with animated selector bar
// ---------------------------------------------------------------------------

// Menu card geometry
static constexpr int16_t MENU_ITEM_H  = 11;    // px per menu item
static constexpr int16_t MENU_START_Y = HDR_BOT + 2;
static constexpr uint8_t MENU_PAGE    = 5;     // visible items at once

static void _draw_menu();
static void _draw_menu_item(uint8_t idx, bool selected);
static void _draw_scrollbar();

static void _menu_select(uint8_t new_idx) {
    if (new_idx == _menu_idx) return;
    uint8_t old_idx    = _menu_idx;
    uint8_t old_scroll = _menu_scroll;
    _menu_idx = new_idx;
    _mark_active();
    Serial.printf("[menu] %d/%d  %s\n", _menu_idx + 1, MENU_LEN,
                  MENU_ITEMS[_menu_idx].label);
    if (_menu_idx < _menu_scroll)
        _menu_scroll = _menu_idx;
    if (_menu_idx >= _menu_scroll + MENU_PAGE)
        _menu_scroll = _menu_idx - MENU_PAGE + 1;
    if (_menu_scroll != old_scroll) {
        _draw_menu();
    } else {
        _draw_menu_item(old_idx,   false);
        _draw_menu_item(_menu_idx, true);
        _draw_scrollbar();
        display_footer(MENU_ITEMS[_menu_idx].label, "TOP:open");
    }
}

static bool _menu_handle_pot_position() {
    uint8_t next = pot_position(MENU_LEN);
    if (next == _menu_idx) return false;
    _menu_select(next);
    return true;
}

// Unique accent colour per menu entry for quick visual identification
static constexpr uint16_t MENU_ACCENTS[] = {
    0x07FF,   // Sensors    — cyan
    0x847F,   // IMU Data   — violet
    C_MAGENTA,// Tilt Demo  — magenta
    C_GREEN,  // Motors     — green
    C_YELLOW, // Ultrasonic — yellow
    C_ORANGE, // Hall Demo  — orange
    C_ACCENT2,// Servo Demo — amber
    C_LTGREY, // System     — silver
    C_ACCENT, // Games      — steel blue
    C_CYAN,   // Robot Modes
    C_GREEN,  // WiFi Info
    C_YELLOW, // WiFi Scan
    C_PURPLE, // BLE Radar
};

static void _draw_menu_item(uint8_t idx, bool selected) {
    int16_t vis = (int16_t)idx - (int16_t)_menu_scroll;
    if (vis < 0 || vis >= MENU_PAGE) return;
    int16_t y = MENU_START_Y + vis * MENU_ITEM_H;
    const MenuItem &item = MENU_ITEMS[idx];
    constexpr int16_t w = DISP_W - 4;
    uint16_t acc = MENU_ACCENTS[idx];

    if (selected) {
        // Filled card background
        display_fill_rect(0, y, w, MENU_ITEM_H - 1, C_PANEL);
        // 3-px coloured left bar
        display_fill_rect(0, y, 3, MENU_ITEM_H - 1, acc);
        // Icon in a small contrasting box
        display_fill_rect(4, y, 10, MENU_ITEM_H - 1, C_BG);
        display_text(5, y + 1, item.icon, acc, C_BG, 1);
        // Label
        display_text(16, y + 1, item.label, C_WHITE, C_PANEL, 1);
        // Arrow indicator on right
        display_text(w - 7, y + 1, ">", acc, C_PANEL, 1);
    } else {
        display_fill_rect(0, y, w, MENU_ITEM_H - 1, C_BG);
        // 1-px dim left edge
        display_fill_rect(0, y, 1, MENU_ITEM_H - 1, C_DKGREY);
        display_text(5, y + 1, item.icon, C_DKGREY, C_BG, 1);
        display_text(16, y + 1, item.label, C_GREY, C_BG, 1);
    }
    // Subtle separator between items
    display_hline(1, y + MENU_ITEM_H - 1, w - 1, C_BG);
}

// Thin scrollbar on the right edge — tracks pot / selection position
static void _draw_scrollbar() {
    if (MENU_LEN <= MENU_PAGE) return;
    constexpr int16_t SB_X = DISP_W - 3;
    constexpr int16_t SB_W = 2;
    int16_t total_h = MENU_PAGE * MENU_ITEM_H;
    int16_t thumb_h = (int16_t)((uint32_t)total_h * MENU_PAGE / MENU_LEN);
    if (thumb_h < 4) thumb_h = 4;
    int16_t travel  = total_h - thumb_h;
    int16_t thumb_y = MENU_START_Y;
    if (MENU_LEN > 1)
        thumb_y += (int16_t)((uint32_t)travel * _menu_idx / (MENU_LEN - 1));
    // Track
    display_fill_rect(SB_X, MENU_START_Y, SB_W, total_h, C_DKGREY);
    // Thumb
    display_fill_rect(SB_X, thumb_y, SB_W, thumb_h, C_ACCENT);
}

static void _draw_menu() {
    display_clear_bg();
    display_header("ROBOT CTRL", HDR_H);

    for (uint8_t i = _menu_scroll;
         i < _menu_scroll + MENU_PAGE && i < MENU_LEN; i++) {
        _draw_menu_item(i, i == _menu_idx);
    }

    _draw_scrollbar();

    display_footer(MENU_ITEMS[_menu_idx].label, "TOP:open");
    Serial.printf("[menu] %d/%d  %s\n", _menu_idx + 1, MENU_LEN,
                  MENU_ITEMS[_menu_idx].label);
}

static void _handle_menu() {
    if (_needs_redraw) {
        _menu_idx = pot_position(MENU_LEN);
        if (_menu_idx < _menu_scroll)
            _menu_scroll = _menu_idx;
        if (_menu_idx >= _menu_scroll + MENU_PAGE)
            _menu_scroll = _menu_idx - MENU_PAGE + 1;
        _draw_menu();
        _needs_redraw = false;
    }

    static bool enter_was_down = false;
    static bool back_was_down  = false;
    bool enter_down = nav_enter_is_pressed();
    bool back_down  = nav_back_is_pressed();

    // Top opens the selected item. Pot moves the highlight.
    if (enter_down && !enter_was_down) {
        enter_was_down = true;
        _mark_active();
        Serial.printf("[menu] open: %s\n", MENU_ITEMS[_menu_idx].label);
        _state        = MENU_ITEMS[_menu_idx].target;
        _needs_redraw = true;
        display_clear_bg();
        return;
    }
    if (back_down && !back_was_down) {
        back_was_down = true;
        _mark_active();
        Serial.println("[menu] back at root");
        return;
    }
    enter_was_down = enter_down;
    back_was_down  = back_down;

    // Absolute pot mapping: low end = first item, high end = last item.
    if (_menu_handle_pot_position()) {
        return;
    }
}

// ---------------------------------------------------------------------------
// Back helper — bottom button returns to menu from any sub-screen.
// ---------------------------------------------------------------------------
static bool _check_back() {
    static bool back_was_down = false;
    static bool enter_was_down = false;
    bool back_down = nav_back_is_pressed();
    bool enter_down = nav_enter_is_pressed();

    if (back_down && !back_was_down) {
        back_was_down = true;
        _mark_active();
        _state        = UiState::MENU;
        _needs_redraw = true;
        display_clear_bg();
        Serial.println("[ui] back → menu");
        return true;
    }
    if (enter_down && !enter_was_down) _mark_active();
    back_was_down = back_down;
    enter_was_down = enter_down;
    return false;
}

// ---------------------------------------------------------------------------
// Screen: Sensors — distance bars with colour heat map
// ---------------------------------------------------------------------------
static void _screen_sensors() {
    static unsigned long _last = 0;
    static bool          _hdr  = false;

    if (!_hdr) {
        display_clear_bg();
        display_header("SENSORS", HDR_H);
        display_footer("", "hold: back");
        _hdr = true;
    }

    if (millis() - _last < 250) {
        if (_check_back()) _hdr = false;
        return;
    }
    _last = millis();

    float d[4];
    ultrasonics_read_all_cm(d);
    float pps1 = hall_get_pulses_per_second(1);
    float pps2 = hall_get_pulses_per_second(2);
    long  cnt1 = hall_get_count(1);
    long  cnt2 = hall_get_count(2);
    uint16_t pv = pot_raw();

    // ── Layout ────────────────────────────────────────────────────────────
    // Content area: y=HDR_BOT..FOOT_Y  (14..71, 57 px)
    // Two columns split at x=80.  Row height 8px, row gap 11px.
    constexpr int16_t COL2 = 80;
    constexpr int16_t Y0   = HDR_BOT + 3;   // 17  US row 1
    constexpr int16_t Y1   = Y0 + 11;       // 28  US row 2
    constexpr int16_t YDIV = Y1 + 10;       // 38  separator
    constexpr int16_t Y2   = YDIV + 3;      // 41  hall speed
    constexpr int16_t Y3   = Y2 + 11;       // 52  hall count
    constexpr int16_t Y4   = Y3 + 11;       // 63  pot

    display_begin_frame();

    // Helper macro: format a distance
    char va[14], vb[14];

    // ── Ultrasonics (2×2 grid) ─────────────────────────────────────────────
    // Row 0: US1 left, US2 right
    if (d[0] >= 0.0f) snprintf(va, sizeof(va), "%5.0fcm", d[0]);
    else              snprintf(va, sizeof(va), "   --- ");
    if (d[1] >= 0.0f) snprintf(vb, sizeof(vb), "%5.0fcm", d[1]);
    else              snprintf(vb, sizeof(vb), "   --- ");
    display_fill_rect(0, Y0, DISP_W, ROW_PX, C_BG);
    display_text(2,          Y0, "US1", C_ACCENT2, C_BG, 1);
    display_text(22,         Y0, va,    C_WHITE,   C_BG, 1);
    display_text(COL2 + 2,   Y0, "US2", C_ACCENT2, C_BG, 1);
    display_text(COL2 + 22,  Y0, vb,    C_WHITE,   C_BG, 1);

    // Row 1: US3 left, US4 right
    if (d[2] >= 0.0f) snprintf(va, sizeof(va), "%5.0fcm", d[2]);
    else              snprintf(va, sizeof(va), "   --- ");
    if (d[3] >= 0.0f) snprintf(vb, sizeof(vb), "%5.0fcm", d[3]);
    else              snprintf(vb, sizeof(vb), "   --- ");
    display_fill_rect(0, Y1, DISP_W, ROW_PX, C_BG);
    display_text(2,          Y1, "US3", C_ACCENT2, C_BG, 1);
    display_text(22,         Y1, va,    C_WHITE,   C_BG, 1);
    display_text(COL2 + 2,   Y1, "US4", C_ACCENT2, C_BG, 1);
    display_text(COL2 + 22,  Y1, vb,    C_WHITE,   C_BG, 1);

    // ── Separator ────────────────────────────────────────────────────────────
    display_hline(4, YDIV, DISP_W - 8, C_DKGREY);

    // ── Hall sensors — speed ─────────────────────────────────────────────────
    uint16_t h1c = (pps1 > 0.5f) ? C_CYAN    : C_DKGREY;
    uint16_t h2c = (pps2 > 0.5f) ? C_ACCENT2 : C_DKGREY;
    snprintf(va, sizeof(va), "%5.0f/s", pps1);
    snprintf(vb, sizeof(vb), "%5.0f/s", pps2);
    display_fill_rect(0, Y2, DISP_W, ROW_PX, C_BG);
    display_text(2,         Y2, "H1", C_DKGREY, C_BG, 1);
    display_text(16,        Y2, va,   h1c,      C_BG, 1);
    display_text(COL2 + 2,  Y2, "H2", C_DKGREY, C_BG, 1);
    display_text(COL2 + 16, Y2, vb,   h2c,      C_BG, 1);

    // ── Hall sensors — cumulative count ──────────────────────────────────────
    display_fill_rect(0, Y3, DISP_W, ROW_PX, C_BG);
    snprintf(va, sizeof(va), "#%-6ld", cnt1);
    snprintf(vb, sizeof(vb), "#%-6ld", cnt2);
    display_text(2,         Y3, va, h1c, C_BG, 1);
    display_text(COL2 + 2,  Y3, vb, h2c, C_BG, 1);

    // ── Pot value + mini bar ─────────────────────────────────────────────────
    display_fill_rect(0, Y4, DISP_W, ROW_PX, C_BG);
    snprintf(va, sizeof(va), "POT %4u", pv);
    display_text(2, Y4, va, C_GREY, C_BG, 1);
    constexpr int16_t PBX = 56;
    constexpr int16_t PBW = DISP_W - PBX - 4;
    int16_t pf = (int16_t)((uint32_t)pv * PBW / 4095u);
    display_fill_rect(PBX,     Y4,     PBW, ROW_PX, C_PANEL);
    if (pf > 0) display_fill_rect(PBX + 1, Y4 + 1, pf, ROW_PX - 2, C_ACCENT);
    display_rect(PBX - 1, Y4 - 1, PBW + 2, ROW_PX + 2, C_DKGREY);

    display_end_frame();
    if (_check_back()) _hdr = false;
}

// ---------------------------------------------------------------------------
// Screen: IMU — live telemetry with mini accel vector indicator
// ---------------------------------------------------------------------------
static void _screen_imu() {
    static unsigned long _last = 0;
    static bool          _hdr  = false;

    if (!_hdr) {
        display_clear_bg();
        display_header("IMU DATA", HDR_H);
        display_footer("DEMO", "HOLD");
        if (!_imu_ok) {
            display_text_centred(0, DISP_H/2 - 8, DISP_W, "Not detected",   C_RED,    C_BG, 1);
            display_text_centred(0, DISP_H/2 + 2, DISP_W, "0x68 / 0x69?",  C_YELLOW, C_BG, 1);
        }
        _hdr = true;
    }

    // Cycle button → jump into Tilt Demo
    if (button_just_pressed(BTN_CYCLE)) {
        _mark_active();
        _state        = UiState::SCREEN_IMU_DEMO;
        _needs_redraw = true;
        display_clear_bg();
        _hdr = false;
        return;
    }
    if (nav_back_just_pressed()) {
        _mark_active();
        _state        = UiState::MENU;
        _needs_redraw = true;
        display_clear_bg();
        _hdr = false;
        Serial.println("[ui] back to menu");
        return;
    }

    if (!_imu_ok || millis() - _last < 100) return;
    _last = millis();

    ImuData d;
    display_begin_frame();
    if (!imu_read(d)) {
        display_text_centred(0, DISP_H/2 - 4, DISP_W, "Read error", C_RED, C_BG, 1);
        display_end_frame();
        return;
    }

    // Two-column layout
    constexpr int16_t COL_W = DISP_W / 2;
    constexpr int16_t ROW_SP = 9;
    int16_t cx = HDR_BOT + 2;
    char buf[12];

    // Accel column (left)
    display_text(3, cx, "ACCEL", C_DKGREY, C_BG, 1); cx += ROW_SP;
    snprintf(buf, sizeof(buf), "X %+.2f", d.accel_x);
    display_text(3, cx, buf, C_CYAN,   C_BG, 1); cx += ROW_SP;
    snprintf(buf, sizeof(buf), "Y %+.2f", d.accel_y);
    display_text(3, cx, buf, C_GREEN,  C_BG, 1); cx += ROW_SP;
    snprintf(buf, sizeof(buf), "Z %+.2f", d.accel_z);
    display_text(3, cx, buf, C_YELLOW, C_BG, 1); cx += ROW_SP;

    // Gyro column — start at same y
    int16_t gx = COL_W + 3;
    int16_t gy = HDR_BOT + 2;
    display_text(gx, gy, "GYRO", C_DKGREY, C_BG, 1); gy += ROW_SP;
    snprintf(buf, sizeof(buf), "X%+6.1f", d.gyro_x);
    display_text(gx, gy, buf, C_CYAN,   C_BG, 1); gy += ROW_SP;
    snprintf(buf, sizeof(buf), "Y%+6.1f", d.gyro_y);
    display_text(gx, gy, buf, C_GREEN,  C_BG, 1); gy += ROW_SP;
    snprintf(buf, sizeof(buf), "Z%+6.1f", d.gyro_z);
    display_text(gx, gy, buf, C_YELLOW, C_BG, 1); gy += ROW_SP;

    // Separator
    display_vline(COL_W - 1, HDR_BOT + 1, FOOT_Y - HDR_BOT - 2, C_DKGREY);

    // Temp + mini tilt bar at bottom
    int16_t by = cx;
    if (by < FOOT_Y - 10) {
        display_hline(0, by - 2, DISP_W, C_DKGREY);
        if (d.temp_c != 0.0f) {
            snprintf(buf, sizeof(buf), "Temp %.1fC", d.temp_c);
            display_text(3, by, buf, C_ORANGE, C_BG, 1);
        }
        // Mini tilt indicator (accel_x → bar offset)
        float tilt = d.accel_x / 9.81f;  // -1..+1
        if (tilt < -1.0f) tilt = -1.0f;
        if (tilt >  1.0f) tilt =  1.0f;
        constexpr int16_t TI_X = 80, TI_W = 60, TI_Y_OFF = 3;
        int16_t tip  = (int16_t)((tilt + 1.0f) * 0.5f * TI_W);
        display_fill_rect(TI_X, by + TI_Y_OFF, TI_W, 4, C_PANEL);
        display_rect     (TI_X, by + TI_Y_OFF, TI_W, 4, C_DKGREY);
        display_fill_rect(TI_X + tip - 2, by + TI_Y_OFF - 1, 5, 6,
                          (fabsf(tilt) > 0.5f) ? C_ORANGE : C_ACCENT);
    }

    Serial.printf("[imu] aX=%.2f aY=%.2f aZ=%.2f | gX=%.1f gY=%.1f gZ=%.1f\n",
                  d.accel_x, d.accel_y, d.accel_z,
                  d.gyro_x,  d.gyro_y,  d.gyro_z);
    display_end_frame();
}

// ---------------------------------------------------------------------------
// Screen: IMU Tilt Demo — liquid level simulation
//
// The arena is half-filled with blue liquid.  When the board tilts the
// surface tilts accordingly (using accel_x / accel_y).  A small air-bubble
// trapped inside the liquid always floats toward the top of the liquid,
// so it moves opposite to gravity — just like a spirit level.
//
// Rendering strategy (sprite-less):
//   • Every frame we repaint the entire arena — background, liquid body,
//     surface wave, bubble, and border.  The arena is small (≈156×54 px)
//     so a column-by-column fill is fast enough at 33 fps with TFT_eSPI DMA.
// ---------------------------------------------------------------------------

static constexpr int16_t ARENA_X1 = 2;
static constexpr int16_t ARENA_X2 = DISP_W - 3;
static constexpr int16_t ARENA_Y1 = HDR_BOT + 2;
static constexpr int16_t ARENA_Y2 = FOOT_Y - 2;
static constexpr int16_t ARENA_W  = ARENA_X2 - ARENA_X1;
static constexpr int16_t ARENA_H  = ARENA_Y2 - ARENA_Y1;

// Liquid colours
static constexpr uint16_t C_LIQUID_DEEP   = 0x0319;  // deep navy  #041832
static constexpr uint16_t C_LIQUID_MID    = 0x04BA;  // mid blue   #0924D0 → 0x044E better
static constexpr uint16_t C_LIQUID_SURF   = 0x07FF;  // cyan surf line
static constexpr uint16_t C_BUBBLE        = 0xC7FF;  // light blue-white bubble
static constexpr uint16_t C_BUBBLE_HILIT  = 0xEFFF;  // near-white highlight

// EMA smoothing weights
static constexpr float TILT_ALPHA   = 0.20f;  // surface tilt  (higher = more responsive)
static constexpr float BUBBLE_ALPHA = 0.25f;  // bubble pos    (slightly faster than surface)

struct LiquidState {
    float tilt;             // smoothed left/right tilt from accel_y  (-1..+1)
    float tilt_prev;        // previous frame tilt — for slosh velocity
    float slosh_vel;        // extra wave amplitude from rapid tilt changes
    float wave_phase;
    // Bubble position in arena coords
    float bub_x, bub_y;
    bool  init;
};

static LiquidState _liq = {};

static void _liquid_init() {
    _liq.tilt      = 0;
    _liq.tilt_prev = 0;
    _liq.slosh_vel = 0;
    _liq.wave_phase = 0;
    _liq.bub_x = ARENA_X1 + ARENA_W * 0.5f;
    _liq.bub_y = ARENA_Y1 + ARENA_H * 0.35f;
    _liq.init  = true;
}

// Returns the liquid surface Y at a given screen X.
// tilt: -1.0 = full left, +1.0 = full right  (from accel_y)
// slosh: extra wave amplitude from rapid tilt changes
static inline int16_t _surface_y(int16_t sx, float tilt, float wave_phase,
                                  float slosh = 0.0f,
                                  float fill_level = 0.52f) {
    float fx       = (float)(sx - ARENA_X1) / (float)ARENA_W;  // 0..1
    float center_y = ARENA_Y1 + ARENA_H * (1.0f - fill_level);
    // Tilt: positive tilt (right side down) → right side has more liquid
    // left (fx=0) surface goes DOWN (+offset), right (fx=1) goes UP (-offset)
    float tilt_off = tilt * (ARENA_H * 0.46f) * (0.5f - fx) * 2.0f;
    // Base ripple + slosh contribution (slosh adds amplitude when tilt changes fast)
    float wave_amp = 1.8f + fabsf(slosh) * 12.0f;
    float ripple   = sinf(fx * 7.85f + wave_phase)          * wave_amp
                   + sinf(fx * 3.14f - wave_phase * 0.7f)   * (wave_amp * 0.55f)
                   + sinf(fx * 12.0f + wave_phase * 1.4f)   * (fabsf(slosh) * 4.0f);
    int16_t sy = (int16_t)(center_y + tilt_off + ripple);
    if (sy < ARENA_Y1 + 2) sy = ARENA_Y1 + 2;
    if (sy > ARENA_Y2 - 2) sy = ARENA_Y2 - 2;
    return sy;
}

static void _screen_imu_demo() {
    static unsigned long _last_frame = 0;
    static bool          _hdr        = false;

    if (!_hdr) {
        display_clear_bg();
        display_header("TILT DEMO", HDR_H);
        display_footer("IMU", "HOLD");
        _liquid_init();
        _hdr = true;
    }

    if (button_just_pressed(BTN_CYCLE)) {
        _mark_active();
        _state = UiState::SCREEN_IMU; _needs_redraw = true;
        display_clear_bg(); _hdr = false; return;
    }
    if (_check_back()) { _hdr = false; return; }

    constexpr unsigned long FRAME_MS = 30;
    if (millis() - _last_frame < FRAME_MS) return;
    _last_frame = millis();

    // ── IMU read  (accel_y = left/right tilt: left=-1g, right=+1g) ───────
    float ay = 0.0f;
    if (_imu_ok) {
        ImuData d;
        if (imu_read(d)) {
            ay = d.accel_y / 9.81f;
            if (ay >  1.0f) ay =  1.0f;
            if (ay < -1.0f) ay = -1.0f;
        }
    }

    // ── Smooth tilt + compute slosh velocity ─────────────────────────────
    _liq.tilt_prev = _liq.tilt;
    _liq.tilt      = _liq.tilt * (1.0f - TILT_ALPHA) + ay * TILT_ALPHA;
    float tilt_delta = _liq.tilt - _liq.tilt_prev;
    // Slosh decays each frame, is boosted by tilt velocity
    _liq.slosh_vel = _liq.slosh_vel * 0.82f + tilt_delta * 4.5f;
    _liq.wave_phase += 0.10f + fabsf(_liq.slosh_vel) * 0.06f;

    // ── Bubble target: spirit-level — floats to the high (uphill) side ───
    // When tilt > 0 (right side down), high side is LEFT → bubble drifts left
    float bub_target_x = ARENA_X1 + ARENA_W * (0.5f - _liq.tilt * 0.45f);
    // Clamp bubble inside arena
    if (bub_target_x < ARENA_X1 + 6)  bub_target_x = ARENA_X1 + 6.0f;
    if (bub_target_x > ARENA_X2 - 6)  bub_target_x = ARENA_X2 - 6.0f;
    // Find surface Y at bubble's horizontal position
    int16_t surf_at_bub = _surface_y((int16_t)bub_target_x,
                                      _liq.tilt, _liq.wave_phase, _liq.slosh_vel);
    float bub_target_y  = (float)surf_at_bub + 5.0f;  // just below surface

    _liq.bub_x = _liq.bub_x * (1.0f - BUBBLE_ALPHA) + bub_target_x * BUBBLE_ALPHA;
    _liq.bub_y = _liq.bub_y * (1.0f - BUBBLE_ALPHA) + bub_target_y * BUBBLE_ALPHA;

    // ── Render: column-by-column ──────────────────────────────────────────
    for (int16_t sx = ARENA_X1; sx < ARENA_X2; sx++) {
        int16_t sy = _surface_y(sx, _liq.tilt, _liq.wave_phase, _liq.slosh_vel);

        // Air above surface
        display_vline(sx, ARENA_Y1, sy - ARENA_Y1, C_BG);

        // Surface pixel — cyan highlight
        display_pixel(sx, sy, C_LIQUID_SURF);

        // Liquid body — gradient: lighter near surface, deeper toward bottom
        int16_t liq_h = ARENA_Y2 - sy - 1;
        if (liq_h > 0) {
            // Top quarter of liquid body: mid blue
            int16_t mid_h = liq_h / 3;
            if (mid_h > 0)
                display_vline(sx, sy + 1, mid_h, C_LIQUID_MID);
            // Remainder: deep navy
            if (liq_h - mid_h > 0)
                display_vline(sx, sy + 1 + mid_h, liq_h - mid_h, C_LIQUID_DEEP);
        }
    }

    // ── Bubble (drawn on top of liquid) ───────────────────────────────────
    int16_t bxi = (int16_t)_liq.bub_x;
    int16_t byi = (int16_t)_liq.bub_y;
    constexpr int16_t BUB_R = 4;
    int16_t surf_at_bxi = _surface_y(bxi, _liq.tilt, _liq.wave_phase, _liq.slosh_vel);
    if (byi > surf_at_bxi && byi < ARENA_Y2 - BUB_R) {
        display_circle(bxi, byi, BUB_R, C_BUBBLE);
        display_circle(bxi, byi, BUB_R - 1,
                       colour_blend(C_LIQUID_MID, C_BUBBLE, 130));
        display_pixel(bxi - 1, byi - 2, C_BUBBLE_HILIT);
        display_pixel(bxi,     byi - 3, C_BUBBLE_HILIT);
    }

    // ── Arena border ──────────────────────────────────────────────────────
    display_rect(ARENA_X1 - 1, ARENA_Y1 - 1, ARENA_W + 2, ARENA_H + 2, C_DKGREY);

    // ── Tilt readout — shows accel_y value and a level-gauge bar ─────────
    // Left label, right label, needle in centre
    constexpr int16_t GY = FOOT_Y + 1;
    constexpr int16_t GW = DISP_W - 24;
    constexpr int16_t GX = 12;
    display_fill_rect(0, GY, DISP_W, ROW_PX, C_BG);
    display_text(2,         GY, "L", C_DKGREY, C_BG, 1);
    display_text(DISP_W - 8, GY, "R", C_DKGREY, C_BG, 1);
    // Track
    display_fill_rect(GX, GY + 2, GW, 4, C_PANEL);
    display_rect     (GX, GY + 2, GW, 4, C_DKGREY);
    // Centre mark
    display_vline(GX + GW / 2, GY + 1, 6, C_DKGREY);
    // Needle — maps -1..+1 to left..right of track
    int16_t needle_x = GX + (int16_t)(((_liq.tilt + 1.0f) * 0.5f) * (float)(GW - 4));
    uint16_t nc = (fabsf(_liq.tilt) > 0.6f) ? C_ORANGE : C_ACCENT;
    display_fill_rect(needle_x, GY + 1, 4, 6, nc);
    // Numeric readout top-left corner of arena (small, non-intrusive)
    char tbuf[12];
    snprintf(tbuf, sizeof(tbuf), "Y:%+.2f", _liq.tilt);
    display_text(ARENA_X1 + 1, ARENA_Y1 + 2, tbuf, C_LIQUID_SURF, C_LIQUID_DEEP, 1);
}

// ---------------------------------------------------------------------------
// Screen: Motors — pot sets speed, BTN_CYCLE drives forward
// ---------------------------------------------------------------------------
static void _screen_motors() {
    static bool          _hdr      = false;
    static unsigned long _last_draw = 0;
    static bool          _was      = false;

    if (!_hdr) {
        display_clear_bg();
        display_header("MOTORS", HDR_H);
        display_footer("DRIVE", "back");
        _hdr = true;
    }

    bool driving = button_is_pressed(BTN_CYCLE);
    uint8_t spd  = (uint8_t)(pot_raw() >> 4);  // 0-4095 → 0-255
    uint8_t pct  = (uint8_t)((uint16_t)spd * 100 / 255);

    if (driving) {
        motor_set(MotorId::MOTOR1, spd);
        motor_set(MotorId::MOTOR2, spd);
    } else {
        if (_was) {
            motor_coast(MotorId::MOTOR1);
            motor_coast(MotorId::MOTOR2);
            Serial.println("[motors] coast");
        }
    }
    _was = driving;

    // Redraw at ~15fps
    if (millis() - _last_draw >= 66) {
        _last_draw = millis();
        display_begin_frame();
        constexpr int16_t BAR_Y = HDR_BOT + 26;   // 40 — leaves room for hint+label
        constexpr int16_t BAR_W = DISP_W - 20;
        constexpr int16_t BAR_H = 12;

        // Erase full content area each frame
        display_fill_rect(0, HDR_BOT, DISP_W, FOOT_Y - HDR_BOT, C_BG);

        // Hint line (top of content, y=16)
        display_text_centred(0, HDR_BOT + 2, DISP_W,
                             "POT=speed  NEXT=drive", C_GREY, C_BG, 1);

        // Speed label + percent (y=26, below hint)
        char spdbuf[12]; snprintf(spdbuf, sizeof(spdbuf), "SPD: %3d%%", pct);
        display_text_centred(0, HDR_BOT + 12, DISP_W, spdbuf,
                             driving ? C_GREEN : C_GREY, C_BG, 1);

        // Speed bar (y=40)
        uint16_t bar_col = driving
            ? (pct > 70 ? C_ORANGE : C_GREEN)
            : colour_blend(C_DKGREY, C_GREEN, pct);
        int16_t fill = (int16_t)((uint16_t)pct * BAR_W / 100);
        display_fill_rect(10, BAR_Y, BAR_W, BAR_H, C_PANEL);
        display_rect     (10, BAR_Y, BAR_W, BAR_H, C_DKGREY);
        if (fill > 0)
            display_fill_rect(11, BAR_Y + 1, fill, BAR_H - 2, bar_col);

        // Status banner (y=56)
        constexpr int16_t SY = BAR_Y + BAR_H + 4;
        if (driving) {
            display_fill_rect(1, SY, DISP_W - 2, 10, C_GREEN);
            display_text_centred(0, SY + 1, DISP_W, ">>>  DRIVING  >>>",
                                 C_BLACK, C_GREEN, 1);
        } else {
            display_fill_rect(1, SY, DISP_W - 2, 10, C_BG);
            display_text_centred(0, SY + 1, DISP_W, "--  READY  --",
                                 C_GREY, C_BG, 1);
        }
        display_end_frame();
    }

    if (_check_back()) {
        motor_coast(MotorId::MOTOR1);
        motor_coast(MotorId::MOTOR2);
        _hdr = false; _was = false;
    }
}

// ---------------------------------------------------------------------------
// Screen: System Info
// ---------------------------------------------------------------------------
static void _screen_system() {
    static bool          _hdr  = false;
    static unsigned long _last = 0;

    if (!_hdr) {
        display_clear_bg();
        display_header("SYSTEM INFO", HDR_H);

        // 7 compact rows of 8px each: y=14..70, fits perfectly
        int16_t y = HDR_BOT + 1;
        char buf[22];

        display_kv(y, "Board",  "ESP32-S3",       C_CYAN);   y += ROW_PX;
        display_kv(y, "FW",     "v1.0",            C_WHITE);  y += ROW_PX;
        snprintf(buf, sizeof(buf), "%u MHz", ESP.getCpuFreqMHz());
        display_kv(y, "CPU",    buf,               C_GREEN);  y += ROW_PX;
        snprintf(buf, sizeof(buf), "%u KB", ESP.getFlashChipSize() / 1024);
        display_kv(y, "Flash",  buf,               C_YELLOW); y += ROW_PX;
        display_kv(y, "IMU",    _imu_ok ? "OK" : "none",
                   _imu_ok ? C_GREEN : C_RED);                y += ROW_PX;
        // Divider before author section
        display_hline(8, y, DISP_W - 16, C_DKGREY);          y += 1;
        display_kv(y, "Author", "averyizatt",     C_ACCENT);  y += ROW_PX;
        display_kv(y, "Web",    "averyizatt.com", C_ACCENT2); y += ROW_PX;

        display_footer("...", "HOLD");
        _hdr = true;
    }

    // Dynamic footer: live uptime + free heap
    if (millis() - _last >= 1000) {
        _last = millis();
        char up[24];
        snprintf(up, sizeof(up), "up %lus | %uKB",
                 millis() / 1000UL, ESP.getFreeHeap() / 1024);
        display_footer(up, "HOLD");
        Serial.printf("[sys] %s\n", up);
    }

    if (_check_back()) _hdr = false;
}

// ---------------------------------------------------------------------------
// Demo: Ultrasonic — animated distance bars for all 4 sensors
//   BTN_CYCLE → cycle which sensor is "focused" (enlarged readout)
//   BTN_SELECT → back
// ---------------------------------------------------------------------------
static void _demo_ultrasonic() {
    static unsigned long _last  = 0;
    static bool          _hdr   = false;
    static uint8_t       _focus = 0;   // 0–3: which sensor is highlighted

    if (!_hdr) {
        display_clear_bg();
        display_header("ULTRASONIC", HDR_H);
        display_footer("FOCUS", "HOLD");
        _hdr = true;
    }

    if (button_just_pressed(BTN_CYCLE)) {
        _mark_active();
        _focus = (_focus + 1) % 4;
        _last = 0;  // force immediate redraw
    }

    if (millis() - _last < 120) {
        if (_check_back()) _hdr = false;
        return;
    }
    _last = millis();

    display_begin_frame();
    float d[4]; ultrasonics_read_all_cm(d);
    static const char *labels[4] = { "US1", "US2", "US3", "US4" };
    constexpr int16_t  BAR_X  = 32;
    constexpr int16_t  BAR_W  = DISP_W - BAR_X - 4;
    constexpr float    US_MAX = 300.0f;

    for (uint8_t i = 0; i < 4; i++) {
        int16_t row_y  = HDR_BOT + 2 + (int16_t)i * 13;
        bool    focus  = (i == _focus);
        uint16_t lbl_c = focus ? C_ACCENT2 : C_GREY;

        // Label
        display_text(2, row_y, labels[i], lbl_c, C_BG, 1);

        // Distance text
        char val[10];
        if (d[i] >= 0.0f) snprintf(val, sizeof(val), "%4.0f", d[i]);
        else              snprintf(val, sizeof(val), "----");
        display_fill_rect(BAR_X - 26, row_y, 26, 8, C_BG);
        display_text(BAR_X - 26, row_y, val, focus ? C_WHITE : C_LTGREY, C_BG, 1);

        // Bar
        float frac = (d[i] >= 0.0f) ? (d[i] / US_MAX) : 0.0f;
        if (frac > 1.0f) frac = 1.0f;
        uint16_t bar_c = (frac < 0.25f) ? C_RED :
                         (frac < 0.55f) ? C_YELLOW : C_GREEN;
        int16_t fill = (int16_t)(frac * BAR_W);
        display_fill_rect(BAR_X, row_y, BAR_W, 8, C_PANEL);
        display_rect     (BAR_X, row_y, BAR_W, 8, focus ? C_ACCENT : C_DKGREY);
        if (fill > 0)
            display_fill_rect(BAR_X + 1, row_y + 1, fill, 6, bar_c);
    }

    // Focused sensor big readout at bottom
    int16_t big_y = HDR_BOT + 4 * 13 + 4;
    char big[20];
    if (d[_focus] >= 0.0f)
        snprintf(big, sizeof(big), "%.1f cm", d[_focus]);
    else
        snprintf(big, sizeof(big), "out of range");
    display_fill_rect(0, big_y, DISP_W, 10, C_BG);
    display_text_centred(0, big_y, DISP_W, big,
                         d[_focus] >= 0.0f ? C_ACCENT2 : C_DKGREY, C_BG, 1);

    display_end_frame();
    if (_check_back()) { _hdr = false; _focus = 0; }
}

// ---------------------------------------------------------------------------
// Demo: Hall Effect — live pulse count + speed for both sensors
//   BTN_CYCLE → reset both counters
//   BTN_SELECT → back
// ---------------------------------------------------------------------------
static void _demo_hall() {
    static unsigned long _last = 0;
    static bool          _hdr  = false;

    if (!_hdr) {
        display_clear_bg();
        display_header("HALL SENSORS", HDR_H);
        display_footer("RESET", "HOLD");
        _hdr = true;
    }

    if (button_just_pressed(BTN_CYCLE)) {
        _mark_active();
        hall_reset_count(1);
        hall_reset_count(2);
        _last = 0;
    }

    if (millis() - _last < 150) {
        if (_check_back()) _hdr = false;
        return;
    }
    _last = millis();

    display_begin_frame();
    float  pps1 = hall_get_pulses_per_second(1);
    float  pps2 = hall_get_pulses_per_second(2);
    long   cnt1 = hall_get_count(1);
    long   cnt2 = hall_get_count(2);

    constexpr int16_t COL = DISP_W / 2;
    constexpr int16_t CY  = HDR_BOT + 4;
    char buf[16];

    // --- Sensor 1 (left column) ---
    display_text_centred(0,    CY,      COL, "SENSOR 1",  C_DKGREY,  C_BG, 1);
    snprintf(buf, sizeof(buf), "%.0f /s", pps1);
    display_fill_rect(0, CY + 10, COL, 10, C_BG);
    uint16_t c1 = (pps1 > 20.0f) ? C_GREEN : (pps1 > 5.0f) ? C_YELLOW : C_LTGREY;
    display_text_centred(0, CY + 10, COL, buf, c1, C_BG, 1);

    // Speed bar sensor 1
    constexpr float  MAX_PPS = 100.0f;
    constexpr int16_t BW = COL - 6;
    float f1 = (pps1 / MAX_PPS > 1.0f) ? 1.0f : pps1 / MAX_PPS;
    display_fill_rect(3, CY + 22, BW, 7, C_PANEL);
    display_rect     (3, CY + 22, BW, 7, C_DKGREY);
    if (f1 > 0) display_fill_rect(4, CY + 23, (int16_t)(f1 * (BW-2)), 5, c1);

    // Count
    snprintf(buf, sizeof(buf), "cnt %ld", cnt1);
    display_fill_rect(0, CY + 32, COL, 8, C_BG);
    display_text_centred(0, CY + 32, COL, buf, C_GREY, C_BG, 1);

    // Divider
    display_vline(COL, HDR_BOT + 2, FOOT_Y - HDR_BOT - 4, C_DKGREY);

    // --- Sensor 2 (right column) ---
    display_text_centred(COL, CY,      COL, "SENSOR 2",  C_DKGREY,  C_BG, 1);
    snprintf(buf, sizeof(buf), "%.0f /s", pps2);
    display_fill_rect(COL, CY + 10, COL, 10, C_BG);
    uint16_t c2 = (pps2 > 20.0f) ? C_GREEN : (pps2 > 5.0f) ? C_YELLOW : C_LTGREY;
    display_text_centred(COL, CY + 10, COL, buf, c2, C_BG, 1);

    float f2 = (pps2 / MAX_PPS > 1.0f) ? 1.0f : pps2 / MAX_PPS;
    display_fill_rect(COL + 3, CY + 22, BW, 7, C_PANEL);
    display_rect     (COL + 3, CY + 22, BW, 7, C_DKGREY);
    if (f2 > 0) display_fill_rect(COL + 4, CY + 23, (int16_t)(f2 * (BW-2)), 5, c2);

    snprintf(buf, sizeof(buf), "cnt %ld", cnt2);
    display_fill_rect(COL, CY + 32, COL, 8, C_BG);
    display_text_centred(COL, CY + 32, COL, buf, C_GREY, C_BG, 1);

    // Hint
    int16_t hint_y = CY + 44;
    if (hint_y < FOOT_Y - 9) {
        display_fill_rect(0, hint_y, DISP_W, 8, C_BG);
        display_text_centred(0, hint_y, DISP_W,
                             (cnt1 == 0 && cnt2 == 0) ? "spin a wheel!" : "NEXT=reset",
                             C_DKGREY, C_BG, 1);
    }

    display_end_frame();
    if (_check_back()) _hdr = false;
}

// ---------------------------------------------------------------------------
// Demo: Servo — single manual mode
//   On entry        servo centers at 90°
//   POT             commands servo angle 0–180° after the dial moves
//   BOTTOM button   back to menu
// ---------------------------------------------------------------------------
static void _demo_servo() {
    static bool          _init      = false;
    static bool          _pot_armed = false;
    static uint16_t      _entry_pot = 0;
    static int16_t       _angle     = 90;
    static int16_t       _drawn_ang = -999;
    static unsigned long _last_draw = 0;

    bool servo_write_needed = false;

    if (!_init) {
        _init      = true;
        _pot_armed = false;
        _entry_pot = pot_raw();
        _angle     = 90;
        _drawn_ang = -999;
        _last_draw = 0;
        display_clear_bg();
        display_header("SERVO DEMO");
        display_footer("POT angle", "BOT back");
        servo_set_angle(ServoId::SERVO1, _angle);
        Serial.println("[servo] open");
    }

    // Use both the debounced state and a raw pin fallback so this screen cannot
    // miss the physical back button.
    bool back_pressed = button_is_pressed(BTN_BACK) ||
                        nav_back_just_pressed() ||
                        (digitalRead(BUTTON2_PIN) == LOW);
    if (back_pressed) {
        _init = false;
        servo_detach(ServoId::SERVO1);
        _mark_active();
        _state        = UiState::MENU;
        _needs_redraw = true;
        display_clear_bg();
        Serial.println("[servo] back -> menu");
        return;
    }

    uint16_t raw = pot_raw();
    if (!_pot_armed && abs((int16_t)raw - (int16_t)_entry_pot) > 24) {
        _pot_armed = true;
        Serial.println("[servo] pot armed");
    }
    if (_pot_armed) {
        int16_t na = (int16_t)pot_position(181);
        if (na != _angle) {
            _angle = na;
            servo_write_needed = true;
            _mark_active();
        }
    }

    bool changed = (_angle != _drawn_ang);
    if (!changed && millis() - _last_draw < 33) {
        if (servo_write_needed) servo_set_angle(ServoId::SERVO1, _angle);
        return;
    }
    _drawn_ang  = _angle;
    _last_draw  = millis();

    // Erase entire content area
    display_fill_rect(0, HDR_BOT, DISP_W, FOOT_Y - HDR_BOT, C_BG);

    // ── Top strip: angle readout + mode badge ─────────────────────────────
    constexpr int16_t TY = HDR_BOT + 2;  // y = 16

    // Large angle number (size 2 = 12×16 px per char)
    char abuf[5];
    snprintf(abuf, sizeof(abuf), "%3d", _angle);
    display_text(2, TY, abuf, C_WHITE, C_BG, 2);
    display_text(40, TY + 5, "deg", C_GREY, C_BG, 1);

    // Mode badge
    display_fill_rect(DISP_W - 47, TY - 1, 46, 10, C_PANEL);
    display_rect     (DISP_W - 47, TY - 1, 46, 10, C_ACCENT);
    display_text     (DISP_W - 45, TY,     "MANUAL", C_ACCENT, C_PANEL, 1);

    // ── Info strip ────────────────────────────────────────────────────────
    constexpr int16_t IY = TY + 19;   // y = 35
    constexpr int16_t BX = 22, BW = DISP_W - 26;

    display_text(2, IY, _pot_armed ? "POT" : "MOVE", C_GREY, C_BG, 1);
    uint8_t apct = (uint8_t)((uint32_t)_angle * 100u / 180u);
    display_fill_rect(BX,     IY + 1,     BW,     5, C_PANEL);
    display_rect     (BX - 1, IY,         BW + 2, 7, C_DKGREY);
    if (apct > 0)
        display_fill_rect(BX, IY + 1,
                          (int16_t)((uint32_t)apct * BW / 100u), 5, C_GREEN);

    // ── Protractor ────────────────────────────────────────────────────────
    constexpr int16_t CX    = DISP_W / 2;   // 80
    constexpr int16_t CY    = 67;            // pivot y
    constexpr int16_t R     = 26;            // arc radius
    constexpr int16_t ARM_L = 22;            // arm length
    constexpr float   PI_F  = 3.14159f;

    TFT_eSPI *tft = display_get_tft();

    // Base line (diameter across 0°–180°)
    tft->drawLine(CX - R - 2, CY, CX + R + 2, CY, C_DKGREY);

    // Semicircle arc (every 3°)
    for (int16_t a = 0; a <= 180; a += 3) {
        float   ar = a * PI_F / 180.0f;
        int16_t ax = CX - (int16_t)(R * cosf(ar));
        int16_t ay = CY - (int16_t)(R * sinf(ar));
        display_pixel(ax, ay, C_LTGREY);
    }

    // Tick marks at 0°, 45°, 90°, 135°, 180°
    static constexpr float TICK_RAD[5] = {
        0.0f,
        PI_F * 45.0f  / 180.0f,
        PI_F * 90.0f  / 180.0f,
        PI_F * 135.0f / 180.0f,
        PI_F
    };
    for (uint8_t t = 0; t < 5; t++) {
        float   c = cosf(TICK_RAD[t]), s = sinf(TICK_RAD[t]);
        int16_t ox = CX - (int16_t)( R      * c);
        int16_t oy = CY - (int16_t)( R      * s);
        int16_t ix = CX - (int16_t)((R - 5) * c);
        int16_t iy = CY - (int16_t)((R - 5) * s);
        tft->drawLine(ix, iy, ox, oy, C_GREY);
    }

    // Servo arm
    float    ar  = _angle * PI_F / 180.0f;
    float    car = cosf(ar), sar = sinf(ar);
    int16_t  tx  = CX - (int16_t)(ARM_L * car);
    int16_t  ty  = CY - (int16_t)(ARM_L * sar);
    int16_t  ppx = (int16_t)(sar * 1.5f);
    int16_t  ppy = (int16_t)(car * 1.5f);
    uint16_t arm_col = C_GREEN;

    tft->drawLine(CX + ppx, CY - ppy, tx + ppx, ty - ppy, arm_col);
    tft->drawLine(CX,       CY,       tx,        ty,       arm_col);
    tft->drawLine(CX - ppx, CY + ppy, tx - ppx,  ty + ppy, arm_col);

    // Pivot hub
    display_fill_circle(CX, CY, 4, C_ACCENT);
    display_fill_circle(CX, CY, 2, C_WHITE);
    // Tip dot
    display_fill_circle(tx, ty, 3, arm_col);

    if (servo_write_needed) servo_set_angle(ServoId::SERVO1, _angle);
}

// ---------------------------------------------------------------------------
// Screen: WiFi Info — AP credentials, IP, connected clients, web URL
// ---------------------------------------------------------------------------
static void _screen_wifi() {
    static bool          _hdr  = false;
    static unsigned long _last = 0;

    if (!_hdr) {
        display_clear_bg();
        display_header("WIFI AP", HDR_H);

        int16_t y = HDR_BOT + 2;
        display_kv(y, "SSID",  WIFI_AP_SSID);      y += ROW_PX + 1;
        display_kv(y, "Pass",  WIFI_AP_PASSWORD);   y += ROW_PX + 1;
        display_kv(y, "IP",    "192.168.4.1");       y += ROW_PX + 1;
        display_kv(y, "Ch",    String(WIFI_AP_CHANNEL).c_str()); y += ROW_PX + 1;

        display_footer("SCAN", "HOLD");
        _hdr  = true;
        _last = 0;
    }

    if (button_just_pressed(BTN_CYCLE)) {
        _mark_active();
        _state        = UiState::SCREEN_WIFI_SCAN;
        _needs_redraw = true;
        display_clear_bg();
        _hdr = false;
        return;
    }

    if (millis() - _last >= 1000) {
        _last = millis();
        int16_t y = HDR_BOT + 2 + 4 * (ROW_PX + 1);
        char buf[12];
        snprintf(buf, sizeof(buf), "%d", wifi_ap_client_count());
        display_kv(y, "Clients", buf); y += ROW_PX + 1;
        snprintf(buf, sizeof(buf), "%us", (unsigned)(millis() / 1000UL));
        display_kv(y, "Uptime",  buf);
    }

    if (_check_back()) _hdr = false;
}

// ---------------------------------------------------------------------------
// Screen: WiFi Scanner — scan nearby networks, show SSID + RSSI on TFT
//   BTN_CYCLE → (re)scan
//   BTN_SELECT → back
// ---------------------------------------------------------------------------
static constexpr int WSCAN_MAX = 16;
static WifiNetInfo  _wscan_results[WSCAN_MAX];
static int          _wscan_count  = 0;
static uint8_t      _wscan_scroll = 0;
static constexpr uint8_t WSCAN_PAGE = 4;

static void _wscan_draw() {
    display_clear_bg();
    display_header("WIFI SCAN", HDR_H);

    if (_wscan_count == 0) {
        display_text_centred(0, DISP_H / 2 - 8, DISP_W,
                             "Press NEXT to scan", C_DKGREY, C_BG, 1);
        display_footer("SCAN", "HOLD");
        return;
    }

    // Sort by RSSI descending (simple insertion sort, ≤16 items)
    for (int i = 1; i < _wscan_count; i++) {
        WifiNetInfo tmp = _wscan_results[i];
        int j = i - 1;
        while (j >= 0 && _wscan_results[j].rssi < tmp.rssi) {
            _wscan_results[j + 1] = _wscan_results[j]; j--;
        }
        _wscan_results[j + 1] = tmp;
    }

    constexpr int16_t ITEM_H = 14;
    constexpr int16_t BAR_W  = 30;
    constexpr int16_t BAR_X  = DISP_W - BAR_W - 2;

    for (int i = 0; i < WSCAN_PAGE; i++) {
        int idx = _wscan_scroll + i;
        if (idx >= _wscan_count) break;
        int16_t y   = HDR_BOT + 2 + (int16_t)i * ITEM_H;
        const WifiNetInfo &n = _wscan_results[idx];

        // RSSI → fill fraction (−100 dBm = 0, −30 dBm = 1)
        float frac = ((float)n.rssi + 100.0f) / 70.0f;
        if (frac < 0.0f) frac = 0.0f;
        if (frac > 1.0f) frac = 1.0f;
        uint16_t bar_c = (frac < 0.35f) ? C_RED :
                         (frac < 0.65f) ? C_YELLOW : C_GREEN;
        int16_t fill = (int16_t)(frac * BAR_W);

        // SSID — truncate to fit
        char ssid[14]; strncpy(ssid, n.ssid[0] ? n.ssid : "(hidden)", 13); ssid[13] = '\0';
        display_fill_rect(0, y, DISP_W, ITEM_H - 1, C_BG);
        display_text(2, y + 1, ssid, C_WHITE, C_BG, 1);

        // Lock icon placeholder
        if (n.enc) display_text(2, y + 8, "*", C_YELLOW, C_BG, 1);

        // Channel
        char ch[4]; snprintf(ch, sizeof(ch), "ch%d", n.ch);
        display_text(BAR_X - 20, y + 3, ch, C_DKGREY, C_BG, 1);

        // RSSI bar
        display_fill_rect(BAR_X, y + 3, BAR_W, 8, C_PANEL);
        display_rect     (BAR_X, y + 3, BAR_W, 8, C_DKGREY);
        if (fill > 0)
            display_fill_rect(BAR_X + 1, y + 4, fill, 6, bar_c);
    }

    // Scrollbar
    if (_wscan_count > WSCAN_PAGE) {
        constexpr int16_t SB_X = DISP_W - 2;
        int16_t total_h  = WSCAN_PAGE * ITEM_H;
        int16_t thumb_h  = (int16_t)((uint32_t)total_h * WSCAN_PAGE / _wscan_count);
        if (thumb_h < 4) thumb_h = 4;
        int16_t thumb_y  = HDR_BOT + 2 +
            (int16_t)((uint32_t)(total_h - thumb_h) * _wscan_scroll / (_wscan_count - WSCAN_PAGE));
        display_fill_rect(SB_X, HDR_BOT + 2, 2, total_h, C_DKGREY);
        display_fill_rect(SB_X, thumb_y,      2, thumb_h, C_ACCENT);
    }

    char cnt[12]; snprintf(cnt, sizeof(cnt), "%d nets", _wscan_count);
    display_footer(cnt, "HOLD");
}

static void _screen_wifi_scan() {
    static bool _hdr        = false;
    static bool _scanning   = false;

    if (!_hdr) {
        _wscan_count  = 0;
        _wscan_scroll = 0;
        _wscan_draw();
        _hdr = true;
    }

    if (button_just_pressed(BTN_CYCLE)) {
        _mark_active();
        if (!_scanning) {
            _scanning = true;
            display_clear_bg();
            display_header("WIFI SCAN", HDR_H);
            display_text_centred(0, DISP_H / 2 - 4, DISP_W, "Scanning...", C_ACCENT2, C_BG, 1);
            display_footer("...", "HOLD");
            led_yellow();
            _wscan_count  = wifi_scan_networks(_wscan_results, WSCAN_MAX);
            _wscan_scroll = 0;
            _scanning     = false;
            led_green();
            _wscan_draw();
        }
        return;
    }

    if (nav_back_just_pressed()) {
        _mark_active();
        _state        = UiState::MENU;
        _needs_redraw = true;
        display_clear_bg();
        _hdr = false;
        Serial.println("[ui] wifi scan -> menu");
        return;
    }

    // Pot scrolling
    if (_wscan_count > WSCAN_PAGE && pot_moved()) {
        _mark_active();
        uint8_t new_scroll = (uint8_t)((uint32_t)pot_position(100)
                             * (_wscan_count - WSCAN_PAGE) / 99);
        if (new_scroll != _wscan_scroll) {
            _wscan_scroll = new_scroll;
            _wscan_draw();
        }
    }
}

// ---------------------------------------------------------------------------
// Screen: BLE Radar — scan for BLE devices, show name/addr/RSSI on TFT
//   BTN_CYCLE → (re)scan
//   BTN_SELECT → back
// ---------------------------------------------------------------------------
static constexpr int BLE_MAX = 16;
static BleDevInfo   _ble_results[BLE_MAX];
static int          _ble_count  = 0;
static uint8_t      _ble_scroll = 0;
static constexpr uint8_t BLE_PAGE = 4;

static void _ble_draw() {
    display_clear_bg();
    display_header("BLE RADAR", HDR_H);

    if (_ble_count == 0) {
        display_text_centred(0, DISP_H / 2 - 8, DISP_W,
                             "Press NEXT to scan", C_DKGREY, C_BG, 1);
        display_footer("SCAN", "HOLD");
        return;
    }

    // Sort by RSSI descending
    for (int i = 1; i < _ble_count; i++) {
        BleDevInfo tmp = _ble_results[i];
        int j = i - 1;
        while (j >= 0 && _ble_results[j].rssi < tmp.rssi) {
            _ble_results[j + 1] = _ble_results[j]; j--;
        }
        _ble_results[j + 1] = tmp;
    }

    constexpr int16_t ITEM_H = 14;
    constexpr int16_t BAR_W  = 24;
    constexpr int16_t BAR_X  = DISP_W - BAR_W - 2;

    for (int i = 0; i < BLE_PAGE; i++) {
        int idx = _ble_scroll + i;
        if (idx >= _ble_count) break;
        int16_t y = HDR_BOT + 2 + (int16_t)i * ITEM_H;
        const BleDevInfo &dev = _ble_results[idx];

        // Name or address
        char label[14];
        if (dev.name[0]) {
            strncpy(label, dev.name, 13);
        } else {
            // Show last 8 chars of addr (xx:xx:xx)
            int alen = strlen(dev.addr);
            const char *tail = (alen > 8) ? dev.addr + alen - 8 : dev.addr;
            strncpy(label, tail, 13);
        }
        label[13] = '\0';

        display_fill_rect(0, y, DISP_W, ITEM_H - 1, C_BG);
        display_text(2, y + 1, label, dev.name[0] ? C_WHITE : C_LTGREY, C_BG, 1);

        // RSSI text
        char rssi_s[8]; snprintf(rssi_s, sizeof(rssi_s), "%d", dev.rssi);
        display_text(BAR_X - 22, y + 3, rssi_s, C_DKGREY, C_BG, 1);

        // RSSI bar
        float frac = ((float)dev.rssi + 100.0f) / 70.0f;
        if (frac < 0.0f) frac = 0.0f;
        if (frac > 1.0f) frac = 1.0f;
        uint16_t bar_c = (frac < 0.35f) ? C_RED :
                         (frac < 0.65f) ? C_YELLOW : C_PURPLE;
        int16_t fill = (int16_t)(frac * BAR_W);
        display_fill_rect(BAR_X, y + 3, BAR_W, 8, C_PANEL);
        display_rect     (BAR_X, y + 3, BAR_W, 8, C_DKGREY);
        if (fill > 0)
            display_fill_rect(BAR_X + 1, y + 4, fill, 6, bar_c);
    }

    // Scrollbar
    if (_ble_count > BLE_PAGE) {
        constexpr int16_t SB_X = DISP_W - 2;
        int16_t total_h = BLE_PAGE * ITEM_H;
        int16_t thumb_h = (int16_t)((uint32_t)total_h * BLE_PAGE / _ble_count);
        if (thumb_h < 4) thumb_h = 4;
        int16_t thumb_y = HDR_BOT + 2 +
            (int16_t)((uint32_t)(total_h - thumb_h) * _ble_scroll / (_ble_count - BLE_PAGE));
        display_fill_rect(SB_X, HDR_BOT + 2, 2, total_h, C_DKGREY);
        display_fill_rect(SB_X, thumb_y,      2, thumb_h, C_PURPLE);
    }

    char cnt[12]; snprintf(cnt, sizeof(cnt), "%d devs", _ble_count);
    display_footer(cnt, "HOLD");
}

static void _screen_ble_scan() {
    static bool _hdr      = false;
    static bool _scanning = false;

    if (!_hdr) {
        _ble_count  = 0;
        _ble_scroll = 0;
        _ble_draw();
        _hdr = true;
    }

    if (button_just_pressed(BTN_CYCLE)) {
        _mark_active();
        if (!_scanning) {
            _scanning = true;
            display_clear_bg();
            display_header("BLE RADAR", HDR_H);
            display_text_centred(0, DISP_H / 2 - 8, DISP_W, "Scanning BLE...",   C_PURPLE, C_BG, 1);
            display_text_centred(0, DISP_H / 2 + 2, DISP_W, "(~4 seconds)",       C_DKGREY, C_BG, 1);
            display_footer("...", "...");
            led_yellow();
            _ble_count  = ble_scan_devices(_ble_results, BLE_MAX);
            _ble_scroll = 0;
            _scanning   = false;
            led_green();
            _ble_draw();
        }
        return;
    }

    if (nav_back_just_pressed()) {
        _mark_active();
        _state        = UiState::MENU;
        _needs_redraw = true;
        display_clear_bg();
        _hdr = false;
        Serial.println("[ui] ble scan -> menu");
        return;
    }

    // Pot scrolling
    if (_ble_count > BLE_PAGE && pot_moved()) {
        _mark_active();
        uint8_t new_scroll = (uint8_t)((uint32_t)pot_position(100)
                             * (_ble_count - BLE_PAGE) / 99);
        if (new_scroll != _ble_scroll) {
            _ble_scroll = new_scroll;
            _ble_draw();
        }
    }
}

// ---------------------------------------------------------------------------
// Idle animation — starfield with pulsing title
// ---------------------------------------------------------------------------
// Text occupies the centre band: keep stars out to prevent over-draw flicker.
static constexpr int16_t _IDLE_TEXT_Y0 = DISP_H / 2 - 8;       // 32
static constexpr int16_t _IDLE_TEXT_Y1 = _IDLE_TEXT_Y0 + 18;   // 50

// Pick a y row outside the text band
static int8_t _idle_star_y() {
    int8_t y;
    do { y = (int8_t)random(0, DISP_H); } while (y >= _IDLE_TEXT_Y0 && y < _IDLE_TEXT_Y1);
    return y;
}

static void _draw_idle() {
    static unsigned long _last = 0;
    static uint8_t       _t    = 0;

    if (millis() - _last < 30) return;
    _last = millis();
    _t++;

    if (!_ss_init) {
        _splash_stars_reset();
        // Move all initial star y values out of the text band
        for (auto &s : _ss) s.y = _idle_star_y();
    }

    TFT_eSPI *tft = display_get_tft();

    display_begin_frame();
    for (uint8_t i = 0; i < 32; i++) {
        // Erase the full trail — all pixels between old and new position.
        // A star with spd=3 moves 3px left, so 3 pixels must be blanked.
        for (uint8_t dx = 0; dx < _ss[i].spd; dx++) {
            int16_t ex = _ss[i].x - (int16_t)dx;
            if (ex >= 0) tft->drawPixel(ex, _ss[i].y, C_BG);
        }

        _ss[i].x -= _ss[i].spd;
        if (_ss[i].x < 0) {
            _ss[i].x   = DISP_W - 1;
            _ss[i].y   = _idle_star_y();   // guaranteed outside text band
            _ss[i].bri = (uint8_t)random(25, 140);
            _ss[i].spd = (uint8_t)random(1, 4);
        }

        // Only draw if the new position is outside the text band
        if (_ss[i].y < _IDLE_TEXT_Y0 || _ss[i].y >= _IDLE_TEXT_Y1) {
            uint8_t b = _ss[i].bri + (_ss[i].spd - 1) * 25;
            tft->drawPixel(_ss[i].x, _ss[i].y, colour_blend(C_BG, C_GREY, b));
        }
    }

    // Pulsing centred text (drawn after stars, owns the centre band entirely)
    float    s   = 0.5f + 0.5f * sinf(_t * 0.04f);
    float    s2  = 0.5f + 0.5f * sinf(_t * 0.04f + 1.5f);
    uint16_t c1  = colour_blend(C_BG, C_ACCENT,  (uint8_t)(s  * 160));
    uint16_t c2  = colour_blend(C_BG, C_DKGREY,  (uint8_t)(s2 * 200));
    display_fill_rect(0, _IDLE_TEXT_Y0, DISP_W, 18, C_BG);
    display_text_centred(0, _IDLE_TEXT_Y0,      DISP_W, "ROBOT CTRL",     c1, C_BG, 1);
    display_text_centred(0, _IDLE_TEXT_Y0 + 10, DISP_W, "averyizatt.com", c2, C_BG, 1);
    display_end_frame();
}

// ---------------------------------------------------------------------------
// ui_update — call every loop()
// ---------------------------------------------------------------------------
void ui_update() {
    if (_state != UiState::IDLE &&
        _state != UiState::SCREEN_GAMES &&
        _state != UiState::SCREEN_ROBOT_MODES &&
        (millis() - _last_activity) >= IDLE_TIMEOUT_MS) {
        _state = UiState::IDLE;
        display_clear_bg();   // fill with C_BG so star erases match background
        _ss_init = false;     // force star positions to be re-randomised for idle
        led_cyan();
        Serial.println("[ui] idle");
    }

    if (_state == UiState::IDLE) {
        if (pot_moved() ||
            nav_enter_is_pressed() ||
            nav_back_is_pressed()) {
            _mark_active();
        } else {
            _draw_idle();
            return;
        }
    }

    switch (_state) {
        case UiState::MENU:            _handle_menu();      break;
        case UiState::SCREEN_SENSORS:  _screen_sensors();   break;
        case UiState::SCREEN_IMU:      _screen_imu();       break;
        case UiState::SCREEN_IMU_DEMO: _screen_imu_demo();  break;
        case UiState::SCREEN_MOTORS:   _screen_motors();    break;
        case UiState::SCREEN_SYSTEM:   _screen_system();    break;
        case UiState::DEMO_ULTRASONIC: _demo_ultrasonic();  break;
        case UiState::DEMO_HALL:       _demo_hall();        break;
        case UiState::DEMO_SERVO:      _demo_servo();       break;
        case UiState::SCREEN_WIFI:      _screen_wifi();      break;
        case UiState::SCREEN_WIFI_SCAN: _screen_wifi_scan(); break;
        case UiState::SCREEN_BLE_SCAN:  _screen_ble_scan();  break;
        case UiState::SCREEN_GAMES:
            if (games_menu()) {
                // games_menu() returned true = player chose back to main menu
                _state        = UiState::MENU;
                _needs_redraw = true;
                display_clear_bg();
            }
            break;
        case UiState::SCREEN_ROBOT_MODES:
            if (robot_modes_menu()) {
                _state        = UiState::MENU;
                _needs_redraw = true;
                display_clear_bg();
            }
            break;
        default: break;
    }
}
