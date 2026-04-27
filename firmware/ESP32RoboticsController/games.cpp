#include "games.h"
#include "display.h"
#include "buttons.h"
#include "led.h"
#include "config.h"
#include "pot.h"
#include <math.h>

// =============================================================================
// Games (direct draw) — no sprite framebuffer
//
// This version intentionally avoids TFT_eSprite so rendering uses the same
// direct drawing path as the rest of the UI, which fixes panel-specific
// orientation/skew issues seen only in games on some ST7735 modules.
// =============================================================================

// -----------------------------------------------------------------------------
// Layout
// -----------------------------------------------------------------------------
static constexpr int16_t G_HDR_H  = 13;
static constexpr int16_t G_CONT_Y = G_HDR_H + 1;
static constexpr int16_t G_FOOT_Y = DISP_H - 9;
static constexpr int16_t G_CONT_H = G_FOOT_Y - G_CONT_Y;

// -----------------------------------------------------------------------------
// RNG
// -----------------------------------------------------------------------------
static uint32_t _rng = 0xBADC0DEu;
static inline uint32_t _rand_u32() {
    _rng ^= _rng << 13;
    _rng ^= _rng >> 17;
    _rng ^= _rng << 5;
    return _rng;
}
static inline int16_t _rand_range(int16_t lo, int16_t hi) {
    return lo + (int16_t)(_rand_u32() % (uint32_t)(hi - lo + 1));
}

static inline int16_t _clampi(int16_t v, int16_t lo, int16_t hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

// Use the smoothed driver value so games get stable readings.
static inline uint16_t _pot_raw() { return pot_raw(); }
static inline float    _pot_norm() { return (float)pot_raw() / 4095.0f; }

// -----------------------------------------------------------------------------
// Shared game state + stats
// -----------------------------------------------------------------------------
enum class GameScreen : uint8_t {
    MENU,
    SNAKE,
    PONG,
    ASTEROIDS,
};

static GameScreen _screen = GameScreen::MENU;
static bool _menu_needs_draw = true;

struct GameStats {
    uint16_t wins;
    uint16_t losses;
};

static GameStats _stats[3] = {};

static constexpr uint16_t WIN_SCORE_SNAKE = 20;
static constexpr uint16_t WIN_SCORE_PONG  = 10;
static constexpr uint16_t WIN_SCORE_AST   = 1200;

// -----------------------------------------------------------------------------
// UI helpers
// -----------------------------------------------------------------------------
static void _draw_frame(const char *title, const char *left_hint, const char *right_hint) {
    display_clear_bg();
    display_header(title, G_HDR_H);
    display_footer(left_hint, right_hint);
}

static void _draw_result_box(const char *title,
                             bool won,
                             uint16_t score,
                             GameStats st,
                             bool allow_menu,
                             const char *score_label = "SCORE") {
    int16_t x = 16;
    int16_t y = G_CONT_Y + 10;
    int16_t w = DISP_W - 32;
    int16_t h = 36;
    uint16_t border = won ? C_GREEN : C_RED;

    display_fill_rect(x, y, w, h, C_PANEL);
    display_rect(x, y, w, h, border);

    display_text_centred(x, y + 3, w, won ? "YOU WIN" : title, border, C_PANEL, 1);

    char score_buf[24];
    snprintf(score_buf, sizeof(score_buf), "%s:%u", score_label, score);
    display_text_centred(x, y + 13, w, score_buf, C_YELLOW, C_PANEL, 1);

    char wl[20];
    snprintf(wl, sizeof(wl), "W:%u L:%u", st.wins, st.losses);
    display_text_centred(x, y + 22, w, wl, C_CYAN, C_PANEL, 1);

    if (allow_menu) {
        display_text_centred(x, y + 29, w, "TOP=RETRY BOT=MENU", C_DKGREY, C_PANEL, 1);
    } else {
        display_text_centred(x, y + 29, w, "TOP=RETRY", C_DKGREY, C_PANEL, 1);
    }
}

// =============================================================================
// GAME 1: SNAKE
// =============================================================================

static constexpr int16_t SN_CELL = 4;
static constexpr int16_t SN_X0   = 0;
static constexpr int16_t SN_Y0   = G_CONT_Y;
static constexpr int16_t SN_COLS = DISP_W / SN_CELL;
static constexpr int16_t SN_ROWS = G_CONT_H / SN_CELL;
static constexpr uint8_t SN_MAX  = 80;

struct SnakeCell {
    int8_t x;
    int8_t y;
};

static SnakeCell _sn_body[SN_MAX];
static uint8_t _sn_head = 0;
static uint8_t _sn_len = 0;
static int8_t _sn_dx = 1;
static int8_t _sn_dy = 0;
static SnakeCell _sn_food = {0, 0};
static uint16_t _sn_score = 0;
static bool _sn_done = false;
static bool _sn_won = false;
static unsigned long _sn_last_tick = 0;

static inline SnakeCell _sn_at(uint8_t i) {
    return _sn_body[(_sn_head + SN_MAX - i) % SN_MAX];
}

static void _snake_place_food() {
    bool ok = false;
    while (!ok) {
        _sn_food.x = (int8_t)_rand_range(0, SN_COLS - 1);
        _sn_food.y = (int8_t)_rand_range(0, SN_ROWS - 1);
        ok = true;
        for (uint8_t i = 0; i < _sn_len; i++) {
            SnakeCell c = _sn_at(i);
            if (c.x == _sn_food.x && c.y == _sn_food.y) {
                ok = false;
                break;
            }
        }
    }
}

static void _snake_init() {
    _sn_len = 4;
    _sn_head = _sn_len - 1;
    _sn_dx = 1;
    _sn_dy = 0;
    _sn_score = 0;
    _sn_done = false;
    _sn_won = false;
    _sn_last_tick = 0;

    int8_t sx = (int8_t)(SN_COLS / 2);
    int8_t sy = (int8_t)(SN_ROWS / 2);
    for (uint8_t i = 0; i < _sn_len; i++) {
        _sn_body[i].x = sx - (int8_t)(_sn_len - 1 - i);
        _sn_body[i].y = sy;
    }

    _snake_place_food();
    _draw_frame("SNAKE", "POT/TOP:TURN", "BOT=BACK");
}

static void _snake_draw_board() {
    display_begin_frame();
    display_fill_rect(SN_X0, SN_Y0, SN_COLS * SN_CELL, SN_ROWS * SN_CELL, C_BG);

    // Border
    display_rect(SN_X0, SN_Y0, SN_COLS * SN_CELL, SN_ROWS * SN_CELL, C_DKGREY);

    // Snake
    for (uint8_t i = 0; i < _sn_len; i++) {
        SnakeCell c = _sn_at(i);
        int16_t px = SN_X0 + c.x * SN_CELL;
        int16_t py = SN_Y0 + c.y * SN_CELL;
        uint16_t col = (i == 0) ? C_ACCENT : C_GREEN;
        display_fill_rect(px + 1, py + 1, SN_CELL - 1, SN_CELL - 1, col);
    }

    // Food
    int16_t fx = SN_X0 + _sn_food.x * SN_CELL;
    int16_t fy = SN_Y0 + _sn_food.y * SN_CELL;
    display_fill_rect(fx + 1, fy + 1, SN_CELL - 1, SN_CELL - 1, C_ORANGE);

    // Header score
    char sc[16];
    snprintf(sc, sizeof(sc), "S:%u", _sn_score);
    display_text(DISP_W - (int16_t)strlen(sc) * 6 - 2, 3, sc, C_WHITE, C_PANEL, 1);
    display_end_frame();
}

static void _snake_tick() {
    SnakeCell h = _sn_at(0);
    int8_t nx = h.x + _sn_dx;
    int8_t ny = h.y + _sn_dy;

    // Wrap edges
    if (nx < 0) nx = SN_COLS - 1;
    if (nx >= SN_COLS) nx = 0;
    if (ny < 0) ny = SN_ROWS - 1;
    if (ny >= SN_ROWS) ny = 0;

    // Self hit
    for (uint8_t i = 0; i < _sn_len - 1; i++) {
        SnakeCell c = _sn_at(i);
        if (c.x == nx && c.y == ny) {
            _sn_done = true;
            _sn_won = false;
            _stats[0].losses++;
            led_red();
            return;
        }
    }

    bool ate = (nx == _sn_food.x && ny == _sn_food.y);

    _sn_head = (_sn_head + 1) % SN_MAX;
    _sn_body[_sn_head].x = nx;
    _sn_body[_sn_head].y = ny;

    if (ate) {
        if (_sn_len < SN_MAX) _sn_len++;
        _sn_score++;
        led_green();
        _snake_place_food();

        if (_sn_score >= WIN_SCORE_SNAKE) {
            _sn_done = true;
            _sn_won = true;
            _stats[0].wins++;
            return;
        }
    }
}

static void _snake_update() {
    if (_sn_done) {
        _snake_draw_board();
        _draw_result_box("GAME OVER", _sn_won, _sn_score, _stats[0], true);

        if (button_just_pressed(BTN_CYCLE)) {
            _snake_init();
        }
        if (nav_back_just_pressed()) {
            _screen = GameScreen::MENU;
            _menu_needs_draw = true;
            display_clear_bg();
        }
        return;
    }

    if (nav_back_just_pressed()) {
        _screen = GameScreen::MENU;
        _menu_needs_draw = true;
        display_clear_bg();
        return;
    }

    // Controls — pot 3-zone (left zone=turn left, right zone=turn right) OR top button
    {
        static uint8_t _pot_zone = 1;   // 0=left 1=centre 2=right
        uint16_t pv = _pot_raw();
        uint8_t  zone = (pv < 1200u) ? 0u : (pv > 2895u) ? 2u : 1u;
        bool turn_left  = (zone == 0 && _pot_zone != 0) || button_just_pressed(BTN_CYCLE);
        bool turn_right = (zone == 2 && _pot_zone != 2);
        _pot_zone = zone;

        if (turn_left) {
            int8_t ndx = _sn_dy,  ndy = -_sn_dx;
            if (!(ndx == -_sn_dx && ndy == -_sn_dy)) { _sn_dx = ndx; _sn_dy = ndy; }
        }
        if (turn_right) {
            int8_t ndx = -_sn_dy, ndy = _sn_dx;
            if (!(ndx == -_sn_dx && ndy == -_sn_dy)) { _sn_dx = ndx; _sn_dy = ndy; }
        }
    }

    uint32_t tick_ms = 160 - _sn_score * 2;
    if (tick_ms < 55) tick_ms = 55;

    if (millis() - _sn_last_tick >= tick_ms) {
        _sn_last_tick = millis();
        _snake_tick();
    }

    _snake_draw_board();
}

// =============================================================================
// GAME 2: PONG
// =============================================================================

static constexpr int16_t PN_PAD_H   = 14;
static constexpr int16_t PN_PAD_X    = DISP_W - 5;
static constexpr int16_t PN_PAD_MINY = G_CONT_Y + 1;
static constexpr int16_t PN_PAD_MAXY = G_FOOT_Y - PN_PAD_H - 1;

struct PongState {
    float bx;
    float by;
    float vx;
    float vy;
    int16_t pad_y;
    uint8_t lives;
    uint16_t score;
    bool done;
    bool won;
    unsigned long last_frame;
};

static PongState _pong = {};

static void _pong_init() {
    _pong.bx = (float)(DISP_W / 2);
    _pong.by = (float)(G_CONT_Y + G_CONT_H / 2);
    _pong.vx = -2.4f;
    _pong.vy = 1.5f;
    _pong.pad_y = G_CONT_Y + G_CONT_H / 2 - PN_PAD_H / 2;
    _pong.lives = 3;
    _pong.score = 0;
    _pong.done = false;
    _pong.won = false;
    _pong.last_frame = 0;
    _draw_frame("PONG", "POT=PADDLE", "BOT=BACK");
}

static void _pong_draw() {
    display_begin_frame();
    display_fill_rect(0, G_CONT_Y, DISP_W, G_CONT_H, C_BG);
    display_rect(0, G_CONT_Y, DISP_W, G_CONT_H, C_DKGREY);

    // Midline
    for (int16_t y = G_CONT_Y + 2; y < G_FOOT_Y - 2; y += 6) {
        display_vline(DISP_W / 2, y, 3, C_DKGREY);
    }

    // Paddle
    display_fill_rect(PN_PAD_X, _pong.pad_y, 3, PN_PAD_H, C_ACCENT);

    // Ball
    display_fill_rect((int16_t)_pong.bx, (int16_t)_pong.by, 3, 3, C_WHITE);

    // Lives (top-left in header)
    for (uint8_t i = 0; i < 3; i++) {
        uint16_t c = (i < _pong.lives) ? C_RED : C_DKGREY;
        display_fill_circle(4 + i * 8, 6, 2, c);
    }

    char sc[16];
    snprintf(sc, sizeof(sc), "S:%u", _pong.score);
    display_text(DISP_W - (int16_t)strlen(sc) * 6 - 2, 3, sc, C_WHITE, C_PANEL, 1);
    display_end_frame();
}

static void _pong_update() {
    if (_pong.done) {
        _pong_draw();
        _draw_result_box("GAME OVER", _pong.won, _pong.score, _stats[1], true);
        if (button_just_pressed(BTN_CYCLE)) _pong_init();
        if (nav_back_just_pressed()) {
            _screen = GameScreen::MENU;
            _menu_needs_draw = true;
            display_clear_bg();
        }
        return;
    }

    if (nav_back_just_pressed()) {
        _screen = GameScreen::MENU;
        _menu_needs_draw = true;
        display_clear_bg();
        return;
    }

    constexpr unsigned long FRAME_MS = 25;
    if (millis() - _pong.last_frame < FRAME_MS) {
        _pong_draw();
        return;
    }
    _pong.last_frame = millis();

    // Pot controls paddle
    int16_t target = PN_PAD_MINY + (int16_t)(((uint32_t)_pot_raw() * (uint32_t)(PN_PAD_MAXY - PN_PAD_MINY)) / 4095u);
    _pong.pad_y += (target - _pong.pad_y) / 3;
    _pong.pad_y = _clampi(_pong.pad_y, PN_PAD_MINY, PN_PAD_MAXY);

    _pong.bx += _pong.vx;
    _pong.by += _pong.vy;

    // Top/bottom bounce
    if (_pong.by <= G_CONT_Y) {
        _pong.by = (float)G_CONT_Y;
        _pong.vy = fabsf(_pong.vy);
    }
    if (_pong.by >= G_FOOT_Y - 3) {
        _pong.by = (float)(G_FOOT_Y - 3);
        _pong.vy = -fabsf(_pong.vy);
    }

    // Left wall bounce
    if (_pong.bx <= 1) {
        _pong.bx = 1.0f;
        _pong.vx = fabsf(_pong.vx);
    }

    // Paddle hit
    if ((int16_t)_pong.bx + 2 >= PN_PAD_X &&
        (int16_t)_pong.by + 2 >= _pong.pad_y &&
        (int16_t)_pong.by <= _pong.pad_y + PN_PAD_H) {
        float rel = ((float)((int16_t)_pong.by - _pong.pad_y) / (float)PN_PAD_H) - 0.5f;
        _pong.vx = -(fabsf(_pong.vx) + 0.12f);
        if (_pong.vx < -5.2f) _pong.vx = -5.2f;
        _pong.vy = rel * 4.8f;
        if (fabsf(_pong.vy) < 0.4f) _pong.vy = (_pong.vy >= 0) ? 0.4f : -0.4f;

        _pong.score++;
        led_green();

        if (_pong.score >= WIN_SCORE_PONG) {
            _pong.done = true;
            _pong.won = true;
            _stats[1].wins++;
        }
    }

    // Miss
    if ((int16_t)_pong.bx >= DISP_W - 1) {
        if (_pong.lives > 0) _pong.lives--;
        led_red();
        _pong.bx = (float)(DISP_W / 2);
        _pong.by = (float)(G_CONT_Y + G_CONT_H / 2);
        _pong.vx = -2.4f;
        _pong.vy = (_rand_u32() & 1u) ? 1.6f : -1.6f;

        if (_pong.lives == 0) {
            _pong.done = true;
            _pong.won = false;
            _stats[1].losses++;
        }
    }

    _pong_draw();
}

// =============================================================================
// GAME 3: ASTEROIDS (dodge)
// =============================================================================

static constexpr uint8_t AS_MAX_ROCKS = 8;
static constexpr int16_t AS_SHIP_X    = 16;

struct Rock {
    float x;
    float y;
    int8_t r;
    bool active;
};

struct AstState {
    float ship_y;
    Rock rocks[AS_MAX_ROCKS];
    uint16_t score;
    uint8_t lives;
    bool done;
    bool won;
    unsigned long last_frame;
    unsigned long last_spawn;
    uint16_t spawn_ms;
};

static AstState _ast = {};

static void _ast_spawn() {
    for (uint8_t i = 0; i < AS_MAX_ROCKS; i++) {
        if (!_ast.rocks[i].active) {
            _ast.rocks[i].active = true;
            _ast.rocks[i].x = (float)(DISP_W + 8);
            _ast.rocks[i].y = (float)_rand_range(G_CONT_Y + 6, G_FOOT_Y - 6);
            _ast.rocks[i].r = (int8_t)_rand_range(3, 6);
            return;
        }
    }
}

static void _ast_init() {
    _ast.ship_y = (float)(G_CONT_Y + G_CONT_H / 2);
    _ast.score = 0;
    _ast.lives = 3;
    _ast.done = false;
    _ast.won = false;
    _ast.last_frame = 0;
    _ast.last_spawn = 0;
    _ast.spawn_ms = 900;

    for (auto &r : _ast.rocks) r.active = false;

    _draw_frame("ASTEROIDS", "POT=FLY", "BOT=BACK");
}

static void _ast_draw() {
    display_begin_frame();
    display_fill_rect(0, G_CONT_Y, DISP_W, G_CONT_H, C_BG);
    display_rect(0, G_CONT_Y, DISP_W, G_CONT_H, C_DKGREY);

    // Stars
    for (int i = 0; i < 18; i++) {
        int16_t sx = (int16_t)(_rand_u32() % DISP_W);
        int16_t sy = (int16_t)(G_CONT_Y + (_rand_u32() % G_CONT_H));
        display_pixel(sx, sy, (_rand_u32() & 1u) ? C_DKGREY : C_PANEL);
    }

    // Rocks
    for (const auto &r : _ast.rocks) {
        if (!r.active) continue;
        display_fill_circle((int16_t)r.x, (int16_t)r.y, r.r, C_DKGREY);
        display_circle((int16_t)r.x, (int16_t)r.y, r.r, C_GREY);
    }

    // Ship
    int16_t sy = (int16_t)_ast.ship_y;
    display_fill_triangle(AS_SHIP_X + 6, sy,
                          AS_SHIP_X - 4, sy - 4,
                          AS_SHIP_X - 4, sy + 4, C_ACCENT);
    display_triangle(AS_SHIP_X + 6, sy,
                     AS_SHIP_X - 4, sy - 4,
                     AS_SHIP_X - 4, sy + 4, C_CYAN);

    // Header info
    for (uint8_t i = 0; i < 3; i++) {
        uint16_t c = (i < _ast.lives) ? C_CYAN : C_DKGREY;
        display_fill_circle(4 + i * 8, 6, 2, c);
    }

    char sc[18];
    snprintf(sc, sizeof(sc), "S:%u", _ast.score);
    display_text(DISP_W - (int16_t)strlen(sc) * 6 - 2, 3, sc, C_WHITE, C_PANEL, 1);
    display_end_frame();
}

static void _ast_update() {
    if (_ast.done) {
        _ast_draw();
        _draw_result_box("GAME OVER", _ast.won, _ast.score, _stats[2], true, "TIME");
        if (button_just_pressed(BTN_CYCLE)) _ast_init();
        if (nav_back_just_pressed()) {
            _screen = GameScreen::MENU;
            _menu_needs_draw = true;
            display_clear_bg();
        }
        return;
    }

    if (nav_back_just_pressed()) {
        _screen = GameScreen::MENU;
        _menu_needs_draw = true;
        display_clear_bg();
        return;
    }

    constexpr unsigned long FRAME_MS = 25;
    if (millis() - _ast.last_frame < FRAME_MS) {
        _ast_draw();
        return;
    }

    unsigned long now = millis();
    _ast.last_frame = now;
    _ast.score++;

    if (_ast.score >= WIN_SCORE_AST) {
        _ast.done = true;
        _ast.won = true;
        _stats[2].wins++;
        led_green();
        _ast_draw();
        return;
    }

    // Spawn interval tightens slowly
    if (now - _ast.last_spawn > _ast.spawn_ms) {
        _ast_spawn();
        _ast.last_spawn = now;
        if (_ast.spawn_ms > 360) _ast.spawn_ms -= 6;
    }

    // Pot maps ship Y
    int16_t target_y = G_CONT_Y + 6 + (int16_t)(((uint32_t)_pot_raw() * (uint32_t)(G_CONT_H - 12)) / 4095u);
    _ast.ship_y += (target_y - _ast.ship_y) * 0.35f;
    if (_ast.ship_y < (float)(G_CONT_Y + 6)) _ast.ship_y = (float)(G_CONT_Y + 6);
    if (_ast.ship_y > (float)(G_FOOT_Y - 6)) _ast.ship_y = (float)(G_FOOT_Y - 6);

    // Move rocks and check collision
    float speed = 2.0f + (float)_ast.score * 0.0012f;
    if (speed > 4.8f) speed = 4.8f;

    for (auto &r : _ast.rocks) {
        if (!r.active) continue;

        r.x -= speed;
        if (r.x < -10.0f) {
            r.active = false;
            continue;
        }

        float dx = r.x - (float)AS_SHIP_X;
        float dy = r.y - _ast.ship_y;
        float rr = (float)(r.r + 4);
        if (dx * dx + dy * dy < rr * rr) {
            r.active = false;
            if (_ast.lives > 0) _ast.lives--;
            led_red();

            if (_ast.lives == 0) {
                _ast.done = true;
                _ast.won = false;
                _stats[2].losses++;
                break;
            }
        }
    }

    _ast_draw();
}

// =============================================================================
// Games menu
// =============================================================================

struct MenuItem {
    const char *label;
    const char *icon;
    GameScreen target;
};

static const MenuItem MENU_ITEMS[] = {
    { "Snake",     "S", GameScreen::SNAKE },
    { "Pong",      "P", GameScreen::PONG },
    { "Asteroids", "A", GameScreen::ASTEROIDS },
};
static constexpr uint8_t MENU_LEN = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);
static uint8_t _menu_idx = 0;
static uint16_t _menu_pot_ref = 0;

static void _draw_menu_item(uint8_t i, bool sel);

static void _select_menu_item(uint8_t next) {
    if (next == _menu_idx) return;
    uint8_t old = _menu_idx;
    _menu_idx = next;
    Serial.printf("[games] %u/%u %s\n", _menu_idx + 1, MENU_LEN,
                  MENU_ITEMS[_menu_idx].label);
    _draw_menu_item(old, false);
    _draw_menu_item(_menu_idx, true);
}

static bool _handle_menu_pot_scroll() {
    constexpr int16_t POT_STEP = 520;
    int16_t delta = (int16_t)pot_raw() - (int16_t)_menu_pot_ref;
    if (abs(delta) < POT_STEP) return false;

    int8_t steps = (int8_t)(delta / POT_STEP);
    _menu_pot_ref += (int16_t)steps * POT_STEP;

    int16_t next = (int16_t)_menu_idx + steps;
    while (next < 0) next += MENU_LEN;
    next %= MENU_LEN;
    _select_menu_item((uint8_t)next);
    return true;
}

static void _draw_menu_item(uint8_t i, bool sel) {
    constexpr int16_t ITEM_H = 16;
    constexpr int16_t START_Y = G_CONT_Y + 4;

    int16_t y = START_Y + i * ITEM_H;
    uint16_t bg = sel ? C_PANEL : C_BG;
    uint16_t fg = sel ? C_WHITE : C_GREY;
    uint16_t ic = sel ? C_ACCENT : C_DKGREY;

    display_fill_rect(0, y, DISP_W, ITEM_H - 1, bg);
    display_fill_rect(0, y, sel ? 2 : 1, ITEM_H - 1, sel ? C_ACCENT : C_DKGREY);
    display_text(5, y + 4, MENU_ITEMS[i].icon, ic, bg, 1);
    display_text(14, y + 4, MENU_ITEMS[i].label, fg, bg, 1);

    char wl[16];
    snprintf(wl, sizeof(wl), "%uW %uL", _stats[i].wins, _stats[i].losses);
    display_text(DISP_W - (int16_t)strlen(wl) * 6 - 3, y + 4,
                 wl, sel ? C_ACCENT2 : C_DKGREY, bg, 1);
}

static void _draw_menu() {
    display_clear_bg();
    display_header("GAMES", G_HDR_H);

    for (uint8_t i = 0; i < MENU_LEN; i++) {
        _draw_menu_item(i, i == _menu_idx);
    }

    display_footer("TOP open", "BOT back");
}

static bool _handle_menu() {
    if (_menu_needs_draw) {
        _menu_idx = 0;
        _menu_pot_ref = pot_raw();
        _draw_menu();
        _menu_needs_draw = false;
    }

    static bool enter_was_down = false;
    static bool back_was_down  = false;
    bool enter_down = nav_enter_is_pressed();
    bool back_down  = nav_back_is_pressed();

    // Bottom button backs out to the main UI menu.
    if (back_down && !back_was_down) {
        back_was_down = true;
        _screen = GameScreen::MENU;
        _menu_needs_draw = true;
        display_clear_bg();
        return true;
    }

    // Top opens the selected game. Pot moves the highlight.
    if (enter_down && !enter_was_down) {
        enter_was_down = true;
        _screen = MENU_ITEMS[_menu_idx].target;
        if (_screen == GameScreen::SNAKE)     _snake_init();
        if (_screen == GameScreen::PONG)      _pong_init();
        if (_screen == GameScreen::ASTEROIDS) _ast_init();
        led_blue();
        return false;
    }
    enter_was_down = enter_down;
    back_was_down  = back_down;

    // Pot scrolls relative to its position when the games menu opens.
    if (_handle_menu_pot_scroll()) {
        return false;
    }

    return false;
}

// =============================================================================
// Public entry
// =============================================================================

bool games_menu() {
    if (_rng == 0xBADC0DEu) {
        _rng ^= (uint32_t)millis();
    }

    switch (_screen) {
        case GameScreen::MENU:
            return _handle_menu();

        case GameScreen::SNAKE:
            _snake_update();
            return false;

        case GameScreen::PONG:
            _pong_update();
            return false;

        case GameScreen::ASTEROIDS:
            _ast_update();
            return false;
    }

    return false;
}
