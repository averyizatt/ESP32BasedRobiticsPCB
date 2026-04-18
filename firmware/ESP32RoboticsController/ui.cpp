#include "ui.h"
#include "display.h"
#include "buttons.h"
#include "games.h"
#include "motors.h"
#include "servos.h"
#include "ultrasonics.h"
#include "hall_sensors.h"
#include "imu.h"
#include "led.h"
#include "pot.h"
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
//    BTN_CYCLE  → advance / next
//    BTN_SELECT → confirm / back
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
    { "Games",     "G", UiState::SCREEN_GAMES     },
};
static constexpr uint8_t MENU_LEN = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);
static uint8_t _menu_idx = 0;

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
        display_row(row, buf, C_GREY, C_BLACK);
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

    // ── Scanline wipe in ────────────────────────────────────────────────────
    display_clear();
    for (int16_t y = 0; y < DISP_H; y += 2) {
        display_hline(0, y, DISP_W, C_ACCENT);
        delay(6);
    }
    for (int16_t y = 0; y < DISP_H; y += 2) {
        display_hline(0, y, DISP_W, C_BLACK);
        delay(4);
    }

    // ── Typewriter splash ───────────────────────────────────────────────────
    const char *title = "ROBOT CTRL";
    const uint8_t tlen = (uint8_t)strlen(title);
    int16_t ty = (DISP_H / 2) - 12;

    for (uint8_t i = 0; i <= tlen; i++) {
        char tmp[12]; memcpy(tmp, title, i); tmp[i] = '\0';
        display_fill_rect(0, ty, DISP_W, 16, C_BLACK);
        int16_t cx = (DISP_W - (int16_t)i * 12) / 2;
        display_text(cx, ty, tmp, C_ACCENT, C_BLACK, 2);
        delay(60);
    }
    // Accent underline beneath title
    int16_t ul_x = (DISP_W - tlen * 12) / 2;
    display_hline(ul_x, ty + 17, tlen * 12, C_ACCENT);
    display_hline(ul_x, ty + 18, tlen * 12, C_DKGREY);

    const char *sub  = "ESP32 Robotics Controller v1";
    const char *sub2 = "by Avery Izatt";
    int16_t sx  = (DISP_W - (int16_t)strlen(sub)  * 6) / 2;
    int16_t sx2 = (DISP_W - (int16_t)strlen(sub2) * 6) / 2;
    display_text(sx,  ty + 20, sub,  C_GREY,   C_BLACK, 1);
    display_text(sx2, ty + 29, sub2, C_ACCENT2, C_BLACK, 1);
    delay(900);

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

    display_row(r, "IMU probe...  ", C_GREY, C_BLACK);
    delay(300);
    display_check(r++, "IMU", _imu_ok);
    Serial.printf("[boot] IMU %s\n", _imu_ok ? "OK" : "NOT FOUND");
    if (!_imu_ok) {
        display_row(r, "Addr 0x68/0x69?", C_YELLOW, C_BLACK);
        delay(700);
    }

    // ── Progress bar ─────────────────────────────────────────────────────────
    led_green();
    _boot_bar_anim(DISP_H - 8, 0, 100);
    delay(250);

    // ── Ready banner ─────────────────────────────────────────────────────────
    display_clear();
    const char *ready = "READY";
    int16_t rx = (DISP_W - (int16_t)strlen(ready) * 12) / 2;
    int16_t ry = (DISP_H / 2) - 8;
    display_fill_rect(0, ry - 4, DISP_W, 24, C_PANEL);
    display_hline(0, ry - 4, DISP_W, C_ACCENT);
    display_hline(0, ry + 20, DISP_W, C_ACCENT);
    display_text(rx, ry, ready, C_GREEN, C_PANEL, 2);
    delay(750);

    display_clear_bg();
    _needs_redraw  = true;
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
static uint8_t _menu_scroll = 0;

static void _draw_menu_item(uint8_t idx, bool selected) {
    int16_t vis = (int16_t)idx - (int16_t)_menu_scroll;
    if (vis < 0 || vis >= MENU_PAGE) return;
    int16_t y = MENU_START_Y + vis * MENU_ITEM_H;
    const MenuItem &item = MENU_ITEMS[idx];
    int16_t w = DISP_W - 4;  // leave space for scrollbar

    if (selected) {
        display_fill_rect(0, y, w, MENU_ITEM_H, C_PANEL);
        display_fill_rect(0, y, 2, MENU_ITEM_H, C_ACCENT);    // left accent bar
        display_text(6, y + 2, item.label, C_WHITE, C_PANEL, 1);
    } else {
        display_fill_rect(0, y, w, MENU_ITEM_H, C_BG);
        display_text(6, y + 2, item.label, C_GREY,  C_BG, 1);
    }
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
    display_header("MENU", HDR_H);

    for (uint8_t i = _menu_scroll;
         i < _menu_scroll + MENU_PAGE && i < MENU_LEN; i++) {
        _draw_menu_item(i, i == _menu_idx);
    }

    _draw_scrollbar();

    char cnt[6]; snprintf(cnt, sizeof(cnt), "%d/%d", _menu_idx + 1, MENU_LEN);
    display_footer(cnt, "SELECT");
    Serial.printf("[menu] %d/%d  %s\n", _menu_idx + 1, MENU_LEN,
                  MENU_ITEMS[_menu_idx].label);
}

static void _handle_menu() {
    // Potentiometer selects the menu item directly
    uint8_t new_idx = pot_position(MENU_LEN);

    if (new_idx != _menu_idx) {
        uint8_t old_idx    = _menu_idx;
        uint8_t old_scroll = _menu_scroll;
        _menu_idx = new_idx;
        _mark_active();

        // Keep selection visible
        if (_menu_idx < _menu_scroll)
            _menu_scroll = _menu_idx;
        if (_menu_idx >= _menu_scroll + MENU_PAGE)
            _menu_scroll = _menu_idx - MENU_PAGE + 1;

        if (_menu_scroll != old_scroll) {
            _draw_menu();                          // full redraw on page change
        } else {
            _draw_menu_item(old_idx,   false);
            _draw_menu_item(_menu_idx, true);
            _draw_scrollbar();
            char cnt[6]; snprintf(cnt, sizeof(cnt), "%d/%d", _menu_idx + 1, MENU_LEN);
            display_footer(cnt, "SELECT");
        }
        return;
    }

    if (button_just_pressed(BTN_SELECT)) {
        _mark_active();
        Serial.printf("[menu] select: %s\n", MENU_ITEMS[_menu_idx].label);
        _state        = MENU_ITEMS[_menu_idx].target;
        _needs_redraw = true;
        display_clear_bg();
        return;
    }
    if (_needs_redraw) {
        _menu_idx = pot_position(MENU_LEN);
        if (_menu_idx < _menu_scroll)
            _menu_scroll = _menu_idx;
        if (_menu_idx >= _menu_scroll + MENU_PAGE)
            _menu_scroll = _menu_idx - MENU_PAGE + 1;
        _draw_menu();
        _needs_redraw = false;
    }
}

// ---------------------------------------------------------------------------
// Back helper — BTN_SELECT returns to menu from any sub-screen
// ---------------------------------------------------------------------------
static bool _check_back() {
    if (button_just_pressed(BTN_SELECT)) {
        _mark_active();
        _state        = UiState::MENU;
        _needs_redraw = true;
        display_clear_bg();
        Serial.println("[ui] back → menu");
        return true;
    }
    if (button_just_pressed(BTN_CYCLE)) _mark_active();
    return false;
}

// ---------------------------------------------------------------------------
// Screen: Sensors — distance bars with colour heat map
// ---------------------------------------------------------------------------
static void _screen_sensors() {
    static unsigned long _last    = 0;
    static bool          _hdr     = false;

    if (!_hdr) {
        display_clear_bg();
        display_header("SENSORS", HDR_H);
        display_footer("NEXT", "BACK");
        _hdr = true;
    }

    if (millis() - _last < 250) return;
    _last = millis();

    // -- Ultrasonic bars ------------------------------------------------------
    float d[4]; ultrasonics_read_all_cm(d);
    constexpr int16_t BAR_X   = 60;
    constexpr int16_t BAR_W   = DISP_W - BAR_X - 4;
    constexpr int16_t US_MAX  = 200;  // cm full-scale
    static const char *labels[4] = { "US1", "US2", "US3", "US4" };

    for (int i = 0; i < 4; i++) {
        int16_t row_y = HDR_BOT + 2 + i * 12;
        char val[10];
        // Key label
        display_text(3, row_y, labels[i], C_GREY, C_BG, 1);
        // Value text
        if (d[i] >= 0.0f) {
            snprintf(val, sizeof(val), "%5.0fcm", d[i]);
        } else {
            snprintf(val, sizeof(val), "  ---  ");
        }
        display_text(BAR_X - 42, row_y, val, C_WHITE, C_BG, 1);
        // Colour bar: close=red, mid=yellow, far=green
        float frac = (d[i] >= 0.0f) ? (d[i] / US_MAX) : 0.0f;
        if (frac > 1.0f) frac = 1.0f;
        uint16_t bar_c;
        if      (frac < 0.33f) bar_c = C_RED;
        else if (frac < 0.66f) bar_c = C_YELLOW;
        else                   bar_c = C_GREEN;
        int16_t fill = (int16_t)(frac * BAR_W);
        display_fill_rect(BAR_X, row_y,        BAR_W, 8, C_PANEL);
        display_rect     (BAR_X, row_y,        BAR_W, 8, C_DKGREY);
        if (fill > 0)
            display_fill_rect(BAR_X + 1, row_y + 1, fill, 6, bar_c);
    }

    // -- Hall sensors ---------------------------------------------------------
    int16_t hy = HDR_BOT + 2 + 4 * 12 + 2;
    float pps1 = hall_get_pulses_per_second(1);
    float pps2 = hall_get_pulses_per_second(2);
    char h1[14], h2[14];
    snprintf(h1, sizeof(h1), "Hall1: %.0f/s", pps1);
    snprintf(h2, sizeof(h2), "Hall2: %.0f/s", pps2);
    display_fill_rect(0, hy, DISP_W, ROW_PX * 2 + 1, C_BG);
    display_text(3, hy,           h1, C_CYAN,   C_BG, 1);
    display_text(3, hy + ROW_PX,  h2, C_ACCENT2, C_BG, 1);

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
        display_footer("DEMO", "BACK");
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
    if (button_just_pressed(BTN_SELECT)) {
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
    if (!imu_read(d)) {
        display_text_centred(0, DISP_H/2 - 4, DISP_W, "Read error", C_RED, C_BG, 1);
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
}

// ---------------------------------------------------------------------------
// Screen: IMU Tilt Demo — liquid simulation
//
// Physics model:
//   A "blob" with velocity responds to accelerometer tilt.
//   We draw the blob as a filled circle, erase old position, draw new one.
//   Additionally a "liquid surface" is simulated as a sine wave whose
//   horizontal tilt tracks accel_y.
// ---------------------------------------------------------------------------

static constexpr int16_t ARENA_X1 = 2;
static constexpr int16_t ARENA_X2 = DISP_W - 3;
static constexpr int16_t ARENA_Y1 = HDR_BOT + 2;
static constexpr int16_t ARENA_Y2 = FOOT_Y - 2;
static constexpr int16_t ARENA_W  = ARENA_X2 - ARENA_X1;
static constexpr int16_t ARENA_H  = ARENA_Y2 - ARENA_Y1;

struct LiquidState {
    float blob_x, blob_y;
    float vel_x,  vel_y;
    // Liquid surface: tilt angle in radians (target + smoothed)
    float surface_tilt;
    float surface_tilt_target;
    // Wave phase
    float wave_phase;
    bool  init;
};

static LiquidState _liq = {};

static void _liquid_init() {
    _liq.blob_x = ARENA_X1 + ARENA_W / 2.0f;
    _liq.blob_y = ARENA_Y1 + ARENA_H / 2.0f;
    _liq.vel_x  = 0.0f;
    _liq.vel_y  = 0.0f;
    _liq.surface_tilt  = 0.0f;
    _liq.surface_tilt_target = 0.0f;
    _liq.wave_phase = 0.0f;
    _liq.init = true;
}

// Draw / erase the liquid surface as a tilted sine wave.
// pass erase=true to overdraw with bg colour before moving.
static void _draw_surface(float tilt, float phase, bool erase) {
    constexpr int16_t SRF_HALF = ARENA_H / 3;   // surface sits ≈ 1/3 from bottom
    int16_t base_y = ARENA_Y1 + (ARENA_H * 2) / 3;

    for (int16_t sx = 0; sx < ARENA_W; sx++) {
        float fx    = (float)sx / (float)ARENA_W;
        // Tilt contribution: left edge up when tilting right
        float tilt_off = tilt * SRF_HALF * (fx - 0.5f) * 2.0f;
        // Sine ripple
        float ripple   = sinf(fx * 6.2832f + phase) * 2.0f;
        int16_t sy = (int16_t)(base_y + tilt_off + ripple);
        if (sy < ARENA_Y1) sy = ARENA_Y1;
        if (sy > ARENA_Y2) sy = ARENA_Y2;

        uint16_t fill_c = erase ? C_BG : C_NAVY;
        uint16_t surf_c = erase ? C_BG : C_ACCENT;

        // Fill below surface with dark water
        display_vline(ARENA_X1 + sx, sy + 1, ARENA_Y2 - sy, fill_c);
        // Surface line
        display_pixel(ARENA_X1 + sx, sy, surf_c);
    }
}

static void _screen_imu_demo() {
    static unsigned long _last_frame = 0;
    static bool          _hdr        = false;
    static int16_t       _old_bx, _old_by;
    static constexpr int16_t BLOB_R = 5;
    static constexpr float   GRAVITY = 0.35f;
    static constexpr float   DAMPING = 0.88f;

    if (!_hdr) {
        display_clear_bg();
        display_header("TILT DEMO", HDR_H);
        display_footer("IMU", "BACK");
        // Draw arena border
        display_rect(ARENA_X1 - 1, ARENA_Y1 - 1,
                     ARENA_W + 2, ARENA_H + 2, C_DKGREY);
        _liquid_init();
        _old_bx = (int16_t)_liq.blob_x;
        _old_by = (int16_t)_liq.blob_y;
        _hdr = true;
    }

    // Cycle → jump back to IMU Data screen
    if (button_just_pressed(BTN_CYCLE)) {
        _mark_active();
        _state        = UiState::SCREEN_IMU;
        _needs_redraw = true;
        display_clear_bg();
        _hdr = false;
        return;
    }
    if (button_just_pressed(BTN_SELECT)) {
        _mark_active();
        _state        = UiState::MENU;
        _needs_redraw = true;
        display_clear_bg();
        _hdr = false;
        Serial.println("[ui] back to menu");
        return;
    }

    constexpr unsigned long FRAME_MS = 30;
    if (millis() - _last_frame < FRAME_MS) return;
    _last_frame = millis();

    // Read IMU (fall back to zero motion if unavailable)
    float ax = 0.0f, ay = 0.0f;
    if (_imu_ok) {
        ImuData d;
        if (imu_read(d)) {
            ax = d.accel_x / 9.81f;   // normalise to g: -1..+1 roughly
            ay = d.accel_y / 9.81f;
        }
    }

    // ── Physics ────────────────────────────────────────────────────────────
    _liq.vel_x  = (_liq.vel_x  + ax * GRAVITY) * DAMPING;
    _liq.vel_y  = (_liq.vel_y  + ay * GRAVITY) * DAMPING;
    _liq.blob_x += _liq.vel_x;
    _liq.blob_y += _liq.vel_y;

    // Wall bounce
    if (_liq.blob_x < ARENA_X1 + BLOB_R) {
        _liq.blob_x = ARENA_X1 + BLOB_R; _liq.vel_x = -_liq.vel_x * 0.6f;
    }
    if (_liq.blob_x > ARENA_X2 - BLOB_R) {
        _liq.blob_x = ARENA_X2 - BLOB_R; _liq.vel_x = -_liq.vel_x * 0.6f;
    }
    if (_liq.blob_y < ARENA_Y1 + BLOB_R) {
        _liq.blob_y = ARENA_Y1 + BLOB_R; _liq.vel_y = -_liq.vel_y * 0.6f;
    }
    if (_liq.blob_y > ARENA_Y2 - BLOB_R) {
        _liq.blob_y = ARENA_Y2 - BLOB_R; _liq.vel_y = -_liq.vel_y * 0.6f;
    }

    // Surface tilt tracking
    _liq.surface_tilt_target = -ay * 1.2f;
    _liq.surface_tilt = _liq.surface_tilt * 0.85f + _liq.surface_tilt_target * 0.15f;
    _liq.wave_phase  += 0.12f;

    // ── Render ─────────────────────────────────────────────────────────────
    // 1. Erase old surface
    _draw_surface(_liq.surface_tilt - 0.01f, _liq.wave_phase - 0.12f, true);

    // 2. Erase old blob
    int16_t bxi = (int16_t)_liq.blob_x;
    int16_t byi = (int16_t)_liq.blob_y;
    display_fill_circle(_old_bx, _old_by, BLOB_R + 1, C_BG);

    // 3. Draw new surface
    _draw_surface(_liq.surface_tilt, _liq.wave_phase, false);

    // 4. Draw blob (on top of surface)
    float speed = sqrtf(_liq.vel_x * _liq.vel_x + _liq.vel_y * _liq.vel_y);
    uint16_t blob_c = (speed > 1.5f) ? C_ORANGE :
                      (speed > 0.5f) ? C_ACCENT  : C_CYAN;
    display_fill_circle(bxi, byi, BLOB_R, blob_c);
    // Highlight dot
    display_fill_circle(bxi - 1, byi - 1, 1, colour_blend(blob_c, C_WHITE, 160));

    // 5. Re-draw arena border edges that got clipped
    display_rect(ARENA_X1 - 1, ARENA_Y1 - 1,
                 ARENA_W + 2, ARENA_H + 2, C_DKGREY);

    _old_bx = bxi;
    _old_by = byi;
}

// ---------------------------------------------------------------------------
// Screen: Motors
// ---------------------------------------------------------------------------
static void _screen_motors() {
    static bool _hdr = false;

    if (!_hdr) {
        display_clear_bg();
        display_header("MOTORS", HDR_H);

        int16_t cy = HDR_BOT + 6;
        display_text_centred(0, cy,      DISP_W, "Hold NEXT = forward", C_GREY, C_BG, 1);
        display_text_centred(0, cy + 10, DISP_W, "Release  = coast",    C_DKGREY, C_BG, 1);
        display_footer("FWD", "BACK");
        _hdr = true;
    }

    bool driving      = button_is_pressed(BTN_CYCLE);
    static bool _was  = false;

    if (driving != _was) {
        _was = driving;
        int16_t sy = HDR_BOT + 32;
        if (driving) {
            motor_set(MotorId::MOTOR1,  180);
            motor_set(MotorId::MOTOR2,  180);
            display_fill_rect(1, sy, DISP_W - 2, 14, C_GREEN);
            display_text_centred(0, sy + 3, DISP_W, ">>>  FWD  >>>", C_BLACK, C_GREEN, 1);
            Serial.println("[motors] FWD");
        } else {
            motor_coast(MotorId::MOTOR1);
            motor_coast(MotorId::MOTOR2);
            display_fill_rect(1, sy, DISP_W - 2, 14, C_PANEL);
            display_text_centred(0, sy + 3, DISP_W, "-- COAST --",  C_DKGREY, C_PANEL, 1);
            Serial.println("[motors] coast");
        }
    }

    if (_check_back()) {
        motor_coast(MotorId::MOTOR1);
        motor_coast(MotorId::MOTOR2);
        _hdr  = false;
        _was  = false;
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
        display_header("SYSTEM", HDR_H);

        int16_t y = HDR_BOT + 2;
        char buf[20];

        // Static rows
        display_kv(y, "Board",  "ESP32-S3");   y += ROW_PX + 1;
        display_kv(y, "FW",     "v1.0");        y += ROW_PX + 1;
        snprintf(buf, sizeof(buf), "%uMHz", ESP.getCpuFreqMHz());
        display_kv(y, "CPU",    buf);           y += ROW_PX + 1;
        snprintf(buf, sizeof(buf), "%uKB", ESP.getFlashChipSize() / 1024);
        display_kv(y, "Flash",  buf);           y += ROW_PX + 1;
        display_kv(y, "IMU",    _imu_ok ? "yes" : "no",
                   _imu_ok ? C_GREEN : C_RED);  y += ROW_PX + 1;
        display_kv(y, "Disp",  "160x80 ST7735"); y += ROW_PX + 1;

        display_footer("---", "BACK");
        _hdr = true;
    }

    if (millis() - _last >= 1000) {
        _last = millis();
        int16_t y = HDR_BOT + 2 + 6 * (ROW_PX + 1);
        char up[12], hp[12];
        snprintf(up, sizeof(up), "%lus",  millis() / 1000UL);
        snprintf(hp, sizeof(hp), "%uKB",  ESP.getFreeHeap() / 1024);
        display_kv(y,             "Uptime", up); y += ROW_PX + 1;
        display_kv(y,             "Heap",   hp);
        Serial.printf("[sys] uptime=%s  heap=%s\n", up, hp);
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
        display_footer("FOCUS", "BACK");
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
        display_footer("RESET", "BACK");
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

    if (_check_back()) _hdr = false;
}

// ---------------------------------------------------------------------------
// Demo: Servo — sweeps servo1 back and forth, shows live angle
//   BTN_CYCLE → nudge +15 deg (manual override)
//   BTN_SELECT → back  (servo left at last position)
// ---------------------------------------------------------------------------
static void _demo_servo() {
    static unsigned long _last_sweep = 0;
    static unsigned long _last_draw  = 0;
    static bool          _hdr        = false;
    static int16_t       _angle      = 90;
    static int8_t        _dir        = 1;  // +1 sweeping up, -1 sweeping down
    static bool          _manual     = false;

    if (!_hdr) {
        display_clear_bg();
        display_header("SERVO DEMO", HDR_H);
        display_footer("NUDGE", "BACK");
        _angle = 90;
        _dir   = 1;
        servo_set_angle(ServoId::SERVO1, _angle);
        _hdr = true;
    }

    if (button_just_pressed(BTN_CYCLE)) {
        _mark_active();
        _angle += 15;
        if (_angle > 180) _angle = 0;
        servo_set_angle(ServoId::SERVO1, _angle);
        _manual      = true;
        _last_sweep  = millis();  // pause auto-sweep for 1s
    }

    // Auto-sweep (resumes 1 s after manual nudge)
    if (!_manual || (millis() - _last_sweep > 1000)) {
        _manual = false;
        if (millis() - _last_sweep >= 25) {
            _last_sweep = millis();
            _angle += _dir * 2;
            if (_angle >= 180) { _angle = 180; _dir = -1; }
            if (_angle <=   0) { _angle =   0; _dir =  1; }
            servo_set_angle(ServoId::SERVO1, _angle);
        }
    }

    // Redraw at 30 fps
    if (millis() - _last_draw < 33) {
        if (_check_back()) { _hdr = false; }
        return;
    }
    _last_draw = millis();

    // ── Semicircle arc indicator ─────────────────────────────────────────
    constexpr int16_t CX = DISP_W / 2;
    constexpr int16_t CY_ARC = HDR_BOT + 34;
    constexpr int16_t R  = 26;

    // Erase arc area
    display_fill_rect(CX - R - 2, HDR_BOT + 6, (R + 2) * 2 + 4, R + 6, C_BG);

    // Draw arc ticks (0, 45, 90, 135, 180 degrees)
    static const int16_t tick_degs[] = { 0, 45, 90, 135, 180 };
    for (int t = 0; t < 5; t++) {
        float rad = ((float)tick_degs[t] - 90.0f) * 3.14159f / 180.0f;
        int16_t tx = CX   + (int16_t)((R + 3) * cosf(rad));
        int16_t ty = CY_ARC + (int16_t)((R + 3) * sinf(rad));
        display_pixel(tx, ty, C_DKGREY);
    }

    // Arc outline (top half of circle as dotted line)
    for (int16_t a = 0; a <= 180; a += 3) {
        float rad = ((float)a - 90.0f) * 3.14159f / 180.0f;
        int16_t ax = CX    + (int16_t)(R * cosf(rad));
        int16_t ay = CY_ARC + (int16_t)(R * sinf(rad));
        display_pixel(ax, ay, C_PANEL);
    }

    // Servo arm needle
    float needle_rad = ((float)_angle - 90.0f) * 3.14159f / 180.0f;
    int16_t nx = CX    + (int16_t)((R - 2) * cosf(needle_rad));
    int16_t ny = CY_ARC + (int16_t)((R - 2) * sinf(needle_rad));
    // Draw line from centre to tip
    int16_t steps = R - 2;
    for (int16_t s = 0; s <= steps; s++) {
        int16_t lx = CX    + (int16_t)(s * cosf(needle_rad));
        int16_t ly = CY_ARC + (int16_t)(s * sinf(needle_rad));
        display_pixel(lx, ly, s > steps / 2 ? C_ACCENT2 : C_ACCENT);
    }
    display_fill_circle(CX, CY_ARC, 3, C_LTGREY);
    display_fill_circle(CX, CY_ARC, 2, C_WHITE);

    // Angle readout
    char buf[12];
    snprintf(buf, sizeof(buf), "%3d deg", _angle);
    display_fill_rect(0, CY_ARC + R + 4, DISP_W, 10, C_BG);
    display_text_centred(0, CY_ARC + R + 4, DISP_W, buf, C_ACCENT2, C_BG, 1);

    // Direction arrow
    const char *dir_s = (_dir > 0) ? "sweep ->" : "<- sweep";
    display_fill_rect(0, CY_ARC + R + 14, DISP_W, 8, C_BG);
    display_text_centred(0, CY_ARC + R + 14, DISP_W, dir_s, C_DKGREY, C_BG, 1);

    if (_check_back()) { _hdr = false; }
}

// ---------------------------------------------------------------------------
// Idle animation — minimal professional standby
// ---------------------------------------------------------------------------
static void _draw_idle() {
    static unsigned long _last = 0;
    static uint8_t       _t    = 0;

    if (millis() - _last < 60) return;
    _last = millis();
    _t++;

    // Slowly pulse "STANDBY" between dim grey and dark grey
    float s = 0.5f + 0.5f * sinf(_t * 0.04f);
    uint16_t c = colour_blend(C_DKGREY, C_GREY, (uint8_t)(s * 255.0f));
    display_fill_rect(0, DISP_H / 2 - 4, DISP_W, 8, C_BG);
    display_text_centred(0, DISP_H / 2 - 4, DISP_W, "STANDBY", c, C_BG, 1);
}

// ---------------------------------------------------------------------------
// ui_update — call every loop()
// ---------------------------------------------------------------------------
void ui_update() {
    if (_state != UiState::IDLE &&
        (millis() - _last_activity) >= IDLE_TIMEOUT_MS) {
        _state = UiState::IDLE;
        display_clear();
        led_cyan();
        Serial.println("[ui] idle");
    }

    if (_state == UiState::IDLE) {
        if (pot_moved() ||
            button_just_pressed(BTN_CYCLE) ||
            button_just_pressed(BTN_SELECT)) {
            _mark_active();
        } else {
            _draw_idle();
        }
        return;
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
        case UiState::SCREEN_GAMES:
            if (games_menu()) {
                // games_menu() returned true = player chose back to main menu
                _state        = UiState::MENU;
                _needs_redraw = true;
                display_clear_bg();
            }
            break;
        default: break;
    }
}
