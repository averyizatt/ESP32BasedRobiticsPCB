#include "games.h"
#include "display.h"
#include "buttons.h"
#include "led.h"
#include <math.h>

// =============================================================================
// Games — Snake, Pong, Asteroids
// =============================================================================
// Display: 160×80 px  |  BTN_CYCLE = BTN1 (NEXT)  |  BTN_SELECT = BTN2 (BACK)
//
// Layout constants (shared with ui.cpp):
//   Header:  y 0–13     Content: y 14–70     Footer: y 71–79
// =============================================================================

static constexpr int16_t G_HDR_H   = 13;
static constexpr int16_t G_CONT_Y  = G_HDR_H + 1;   // first content pixel
static constexpr int16_t G_FOOT_Y  = DISP_H - 9;    // footer top
static constexpr int16_t G_CONT_H  = G_FOOT_Y - G_CONT_Y;  // 56 px usable

// =============================================================================
// Internal state machine
// =============================================================================
enum class GameState : uint8_t {
    GAMES_MENU,
    GAME_SNAKE,
    GAME_PONG,
    GAME_ASTEROIDS,
};
static GameState _gstate     = GameState::GAMES_MENU;
static bool      _g_needs_draw = true;

// ─────────────────────────────────────────────────────────────────────────────
// Small helpers
// ─────────────────────────────────────────────────────────────────────────────
static inline void _g_back_to_menu() {
    _gstate       = GameState::GAMES_MENU;
    _g_needs_draw = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Shared PRNG (LCG — no stdlib rand needed)
// ─────────────────────────────────────────────────────────────────────────────
static uint32_t _rng = 0xDEADBEEF;
static inline uint32_t _rand() {
    _rng ^= _rng << 13;
    _rng ^= _rng >> 17;
    _rng ^= _rng << 5;
    return _rng;
}
static inline int16_t _rand_range(int16_t lo, int16_t hi) {
    return lo + (int16_t)(_rand() % (uint32_t)(hi - lo + 1));
}

// =============================================================================
// GAME: SNAKE
// =============================================================================
// Controls:
//   BTN_CYCLE  (short press) — turn right
//   BTN_SELECT (short press) — turn left  / back to menu when dead
//
// Arena: full content area  (0..DISP_W-1) × (G_CONT_Y..G_FOOT_Y-1)
// Grid cell: 4×4 px
// =============================================================================

static constexpr int16_t SN_CELL   = 4;
static constexpr int16_t SN_X0     = 0;
static constexpr int16_t SN_Y0     = G_CONT_Y;
static constexpr int16_t SN_COLS   = DISP_W  / SN_CELL;   // 40
static constexpr int16_t SN_ROWS   = G_CONT_H / SN_CELL;  // 14
static constexpr uint8_t SN_MAX    = 80;   // max snake length

struct SnakeCell { int8_t x, y; };

static SnakeCell _sn_body[SN_MAX];
static uint8_t   _sn_head;        // ring-buffer head index
static uint8_t   _sn_len;         // current length
static int8_t    _sn_dx, _sn_dy;  // current direction
static SnakeCell _sn_food;
static uint16_t  _sn_score;
static bool      _sn_dead;
static bool      _sn_hdr;
static unsigned long _sn_last;

static inline SnakeCell _sn_at(uint8_t i) {
    return _sn_body[(_sn_head + SN_MAX - i) % SN_MAX];
}

static void _sn_place_food() {
    // Pick a random cell not occupied by the snake
    bool ok = false;
    while (!ok) {
        _sn_food.x = (int8_t)_rand_range(0, SN_COLS - 1);
        _sn_food.y = (int8_t)_rand_range(0, SN_ROWS - 1);
        ok = true;
        for (uint8_t i = 0; i < _sn_len; i++) {
            SnakeCell c = _sn_at(i);
            if (c.x == _sn_food.x && c.y == _sn_food.y) { ok = false; break; }
        }
    }
}

static void _sn_draw_cell(int8_t gx, int8_t gy, uint16_t col) {
    int16_t px = SN_X0 + gx * SN_CELL;
    int16_t py = SN_Y0 + gy * SN_CELL;
    display_fill_rect(px, py, SN_CELL - 1, SN_CELL - 1, col);
}

static void _snake_init() {
    _sn_len   = 4;
    _sn_head  = 0;
    _sn_dx    = 1; _sn_dy = 0;
    _sn_score = 0;
    _sn_dead  = false;
    _sn_hdr   = false;
    _sn_last  = 0;

    // Start horizontally centred
    int8_t sx = (int8_t)(SN_COLS / 2);
    int8_t sy = (int8_t)(SN_ROWS / 2);
    for (uint8_t i = 0; i < _sn_len; i++) {
        _sn_body[i].x = sx - (int8_t)(_sn_len - 1 - i);
        _sn_body[i].y = sy;
    }
    _sn_head = _sn_len - 1;
    _rng ^= (uint32_t)millis();
    _sn_place_food();
}

static void _game_snake() {
    if (!_sn_hdr) {
        display_clear_bg();
        display_header("SNAKE", G_HDR_H);
        display_footer("TURN R", "TURN L");
        // Draw entire snake
        for (uint8_t i = 0; i < _sn_len; i++) {
            SnakeCell c = _sn_at(i);
            _sn_draw_cell(c.x, c.y, (i == 0) ? C_ACCENT : C_GREEN);
        }
        _sn_draw_cell(_sn_food.x, _sn_food.y, C_RED);
        _sn_hdr = true;
    }

    // Dead — wait for button to restart or SELECT to exit
    if (_sn_dead) {
        if (button_just_pressed(BTN_CYCLE)) {
            // Restart
            _snake_init();
            display_clear_bg();
            display_header("SNAKE", G_HDR_H);
            display_footer("TURN R", "TURN L");
            for (uint8_t i = 0; i < _sn_len; i++) {
                SnakeCell c = _sn_at(i);
                _sn_draw_cell(c.x, c.y, (i == 0) ? C_ACCENT : C_GREEN);
            }
            _sn_draw_cell(_sn_food.x, _sn_food.y, C_RED);
            _sn_dead = false;
        }
        if (button_just_pressed(BTN_SELECT)) {
            _sn_hdr = false;
            _g_back_to_menu();
        }
        return;
    }

    // Input: turn right / turn left
    if (button_just_pressed(BTN_CYCLE)) {
        // Rotate direction clockwise (right turn)
        int8_t ndx =  _sn_dy;
        int8_t ndy = -_sn_dx;
        // Prevent reversing
        if (ndx != -_sn_dx || ndy != -_sn_dy) {
            _sn_dx = ndx; _sn_dy = ndy;
        }
    }
    if (button_just_pressed(BTN_SELECT)) {
        // Left turn  — rotate anti-clockwise
        int8_t ndx = -_sn_dy;
        int8_t ndy =  _sn_dx;
        if (ndx != -_sn_dx || ndy != -_sn_dy) {
            _sn_dx = ndx; _sn_dy = ndy;
        }
        // Note: SELECT also used for left turn; back is via long-press (handled by ui)
    }

    // Tick rate increases with score
    uint32_t tick_ms = (uint32_t)(180 - _sn_score * 3);
    if (tick_ms < 60) tick_ms = 60;
    if (millis() - _sn_last < tick_ms) return;
    _sn_last = millis();

    // Compute new head
    SnakeCell head = _sn_at(0);
    int8_t nx = head.x + _sn_dx;
    int8_t ny = head.y + _sn_dy;

    // Wall wrap
    if (nx < 0)           nx = SN_COLS - 1;
    if (nx >= SN_COLS)    nx = 0;
    if (ny < 0)           ny = SN_ROWS - 1;
    if (ny >= SN_ROWS)    ny = 0;

    // Self-collision (skip tail tip as it will move)
    for (uint8_t i = 0; i < _sn_len - 1; i++) {
        SnakeCell c = _sn_at(i);
        if (c.x == nx && c.y == ny) {
            // Dead
            _sn_dead = true;
            char msg[20];
            snprintf(msg, sizeof(msg), "SCORE: %u", _sn_score);
            display_fill_rect(20, G_CONT_Y + 14, DISP_W - 40, 28, C_PANEL);
            display_rect     (20, G_CONT_Y + 14, DISP_W - 40, 28, C_RED);
            display_text_centred(0, G_CONT_Y + 17, DISP_W, "GAME OVER", C_RED,    C_PANEL, 1);
            display_text_centred(0, G_CONT_Y + 27, DISP_W, msg,         C_YELLOW, C_PANEL, 1);
            display_text_centred(0, G_CONT_Y + 37, DISP_W, "R=RETRY L=MENU", C_GREY, C_PANEL, 1);
            led_red();
            return;
        }
    }

    bool ate = (nx == _sn_food.x && ny == _sn_food.y);

    // Erase tail (before advancing if not eating)
    if (!ate) {
        SnakeCell tail = _sn_at(_sn_len - 1);
        _sn_draw_cell(tail.x, tail.y, C_BG);
    }

    // Advance ring buffer
    _sn_head = (_sn_head + 1) % SN_MAX;
    _sn_body[_sn_head].x = nx;
    _sn_body[_sn_head].y = ny;
    if (ate) {
        if (_sn_len < SN_MAX) _sn_len++;
        _sn_score++;
        _sn_place_food();
        _sn_draw_cell(_sn_food.x, _sn_food.y, C_RED);
        // Flash score
        char sc[8]; snprintf(sc, sizeof(sc), "%u", _sn_score);
        display_fill_rect(DISP_W - 24, 0, 24, G_HDR_H, C_PANEL);
        display_text(DISP_W - 6 * (int16_t)strlen(sc) - 2, 3, sc, C_YELLOW, C_PANEL, 1);
        led_green();
    }

    // Draw new head (slightly brighter than body)
    _sn_draw_cell(nx,     ny,     C_ACCENT);
    // Redraw second cell as body colour
    if (_sn_len > 1) {
        SnakeCell c2 = _sn_at(1);
        _sn_draw_cell(c2.x, c2.y, C_GREEN);
    }
}

// =============================================================================
// GAME: PONG  (one player vs wall — angle-based bounce)
// =============================================================================
// BTN_CYCLE held = paddle up | released = paddle down (or reverse)
// BTN_SELECT = back to games menu
// The ball bounces off the left wall and the player's paddle on the right.
// Miss = lose a life. 3 lives.
// =============================================================================

static constexpr int16_t PN_PX      = DISP_W - 5;   // paddle x
static constexpr int16_t PN_PAD_H   = 14;            // paddle height
static constexpr int16_t PN_PAD_MIN = G_CONT_Y + 1;
static constexpr int16_t PN_PAD_MAX = G_FOOT_Y - PN_PAD_H - 1;

struct PongState {
    float bx, by, vx, vy;
    int16_t pad_y;
    uint8_t lives;
    uint16_t score;
    bool dead;
    bool hdr;
    unsigned long last_frame;
};
static PongState _pong;

static void _pong_init() {
    _pong.bx    = (float)(DISP_W / 2);
    _pong.by    = (float)(G_CONT_Y + G_CONT_H / 2);
    _pong.vx    = -2.0f;
    _pong.vy    = 1.5f;
    _pong.pad_y = G_CONT_Y + G_CONT_H / 2 - PN_PAD_H / 2;
    _pong.lives = 3;
    _pong.score = 0;
    _pong.dead  = false;
    _pong.hdr   = false;
    _pong.last_frame = 0;
}

static void _pong_draw_paddle(int16_t y, uint16_t col) {
    display_fill_rect(PN_PX, y, 3, PN_PAD_H, col);
}

static void _pong_draw_lives(uint16_t col) {
    for (uint8_t i = 0; i < 3; i++) {
        uint16_t c = (i < _pong.lives) ? C_RED : C_PANEL;
        display_fill_circle(4 + i * 8, G_HDR_H - 5, 3, c);
    }
}

static void _game_pong() {
    if (!_pong.hdr) {
        display_clear_bg();
        display_header("PONG", G_HDR_H);
        display_footer("UP", "BACK");
        // Court lines
        display_vline(0, G_CONT_Y, G_CONT_H, C_DKGREY);         // left wall
        display_hline(0, G_CONT_Y,     DISP_W, C_DKGREY);        // top wall
        display_hline(0, G_FOOT_Y - 1, DISP_W, C_DKGREY);        // bottom wall
        _pong_draw_lives(C_RED);
        _pong_draw_paddle(_pong.pad_y, C_ACCENT);
        _pong.hdr = true;
    }

    // Dead screen
    if (_pong.dead) {
        if (button_just_pressed(BTN_CYCLE)) {
            _pong_init();
            display_clear_bg();
            display_header("PONG", G_HDR_H);
            display_footer("UP", "BACK");
            display_vline(0, G_CONT_Y, G_CONT_H, C_DKGREY);
            display_hline(0, G_CONT_Y,     DISP_W, C_DKGREY);
            display_hline(0, G_FOOT_Y - 1, DISP_W, C_DKGREY);
            _pong_draw_lives(C_RED);
            _pong_draw_paddle(_pong.pad_y, C_ACCENT);
            _pong.dead = false;
            _pong.hdr  = true;
        }
        if (button_just_pressed(BTN_SELECT)) {
            _pong.hdr = false;
            _g_back_to_menu();
        }
        return;
    }

    if (button_just_pressed(BTN_SELECT)) {
        _pong.hdr = false;
        _g_back_to_menu();
        return;
    }

    constexpr unsigned long PONG_MS = 33;   // ~30 fps
    if (millis() - _pong.last_frame < PONG_MS) return;
    _pong.last_frame = millis();

    // Paddle movement: held = up, not held = down (autoplay feel)
    int16_t old_pad = _pong.pad_y;
    if (button_is_pressed(BTN_CYCLE)) {
        _pong.pad_y -= 3;
    } else {
        _pong.pad_y += 2;
    }
    if (_pong.pad_y < PN_PAD_MIN) _pong.pad_y = PN_PAD_MIN;
    if (_pong.pad_y > PN_PAD_MAX) _pong.pad_y = PN_PAD_MAX;

    // Erase & redraw paddle
    if (old_pad != _pong.pad_y) {
        _pong_draw_paddle(old_pad, C_BG);
        // Re-draw walls that may have been overwritten
        display_vline(PN_PX - 1, G_CONT_Y, G_CONT_H, C_BG);
        _pong_draw_paddle(_pong.pad_y, C_ACCENT);
    }

    // Erase ball
    int16_t obx = (int16_t)_pong.bx;
    int16_t oby = (int16_t)_pong.by;
    display_fill_rect(obx, oby, 3, 3, C_BG);

    // Move ball
    _pong.bx += _pong.vx;
    _pong.by += _pong.vy;

    int16_t nbx = (int16_t)_pong.bx;
    int16_t nby = (int16_t)_pong.by;

    // Top/bottom wall bounce
    if (nby <= G_CONT_Y) {
        _pong.by = (float)G_CONT_Y + 1;
        _pong.vy = fabsf(_pong.vy);
    }
    if (nby >= G_FOOT_Y - 4) {
        _pong.by = (float)(G_FOOT_Y - 4);
        _pong.vy = -fabsf(_pong.vy);
    }

    // Left wall bounce
    if (nbx <= 1) {
        _pong.bx = 2.0f;
        _pong.vx = fabsf(_pong.vx);
    }

    // Paddle collision
    if (nbx + 2 >= PN_PX &&
        nby + 2 >= _pong.pad_y &&
        nby     <= _pong.pad_y + PN_PAD_H) {
        // Reflect with slight angle based on hit position
        float rel = ((float)(nby - _pong.pad_y) / (float)PN_PAD_H) - 0.5f; // -0.5..+0.5
        _pong.vx = -fabsf(_pong.vx) - 0.2f;   // speed up a tiny bit
        _pong.vy = rel * 4.0f;
        if (fabsf(_pong.vy) < 0.5f) _pong.vy = (_pong.vy >= 0) ? 0.5f : -0.5f;
        if (_pong.vx < -4.5f) _pong.vx = -4.5f;  // cap speed
        _pong.score++;
        // Show score
        char sc[8]; snprintf(sc, sizeof(sc), "%u", _pong.score);
        display_fill_rect(DISP_W - 24, 0, 24, G_HDR_H, C_PANEL);
        display_text(DISP_W - 6 * (int16_t)strlen(sc) - 2, 3, sc, C_YELLOW, C_PANEL, 1);
        led_green();
    }

    // Miss — ball passed paddle
    if (nbx >= DISP_W - 1) {
        led_red();
        _pong.lives--;
        _pong.bx = (float)(DISP_W / 2);
        _pong.by = (float)(G_CONT_Y + G_CONT_H / 2);
        _pong.vx = -2.0f;
        _pong.vy = (_rand() & 1) ? 1.5f : -1.5f;
        _pong_draw_lives(C_RED);

        if (_pong.lives == 0) {
            _pong.dead = true;
            char msg[20]; snprintf(msg, sizeof(msg), "SCORE: %u", _pong.score);
            display_fill_rect(20, G_CONT_Y + 10, DISP_W - 40, 30, C_PANEL);
            display_rect     (20, G_CONT_Y + 10, DISP_W - 40, 30, C_RED);
            display_text_centred(0, G_CONT_Y + 13, DISP_W, "GAME OVER", C_RED,    C_PANEL, 1);
            display_text_centred(0, G_CONT_Y + 23, DISP_W, msg,         C_YELLOW, C_PANEL, 1);
            display_text_centred(0, G_CONT_Y + 33, DISP_W, "R=RETRY",   C_GREY,   C_PANEL, 1);
            return;
        }
        return;
    }

    // Draw ball
    display_fill_rect((int16_t)_pong.bx, (int16_t)_pong.by, 3, 3, C_WHITE);
}

// =============================================================================
// GAME: ASTEROIDS (side-scroller dodge)
// =============================================================================
// Rocks scroll from right to left.
// Player ship is on the left; BTN_CYCLE = thrust up, released = fall.
// Hit = lose a life (3 lives). Survive as long as possible for score.
// BTN_SELECT = back to games menu.
// =============================================================================

static constexpr uint8_t  AS_MAX_ROCKS = 6;
static constexpr int16_t  AS_SHIP_X    = 14;
static constexpr float    AS_GRAVITY   = 0.25f;
static constexpr float    AS_THRUST    = -0.55f;
static constexpr float    AS_ROCK_SPD  = 2.5f;

struct AsteroidRock {
    float  x, y;
    int8_t size;   // half-size radius
    bool   active;
};

struct AsteroidState {
    float  ship_y, ship_vy;
    AsteroidRock rocks[AS_MAX_ROCKS];
    uint16_t score;
    uint8_t  lives;
    bool     dead;
    bool     hdr;
    unsigned long last_frame;
    unsigned long last_spawn;
    uint16_t spawn_interval_ms;  // decreases over time
    bool     invuln;             // brief invulnerability after hit
    unsigned long invuln_until;
};
static AsteroidState _ast;

static void _ast_spawn_rock() {
    for (uint8_t i = 0; i < AS_MAX_ROCKS; i++) {
        if (!_ast.rocks[i].active) {
            _ast.rocks[i].x    = (float)(DISP_W + 4);
            _ast.rocks[i].y    = (float)_rand_range(G_CONT_Y + 4, G_FOOT_Y - 8);
            _ast.rocks[i].size = (int8_t)_rand_range(3, 6);
            _ast.rocks[i].active = true;
            return;
        }
    }
}

static void _ast_init() {
    _ast.ship_y   = (float)(G_CONT_Y + G_CONT_H / 2);
    _ast.ship_vy  = 0.0f;
    _ast.score    = 0;
    _ast.lives    = 3;
    _ast.dead     = false;
    _ast.hdr      = false;
    _ast.last_frame      = 0;
    _ast.last_spawn      = 0;
    _ast.spawn_interval_ms = 1200;
    _ast.invuln   = false;
    _ast.invuln_until = 0;
    for (uint8_t i = 0; i < AS_MAX_ROCKS; i++) _ast.rocks[i].active = false;
}

static void _ast_draw_ship(float y, uint16_t col) {
    int16_t sy = (int16_t)y;
    // Simple triangle pointing right
    display_triangle(AS_SHIP_X + 6, sy,
                     AS_SHIP_X - 2, sy - 4,
                     AS_SHIP_X - 2, sy + 4, col);
    if (col != C_BG) {
        display_pixel(AS_SHIP_X + 5, sy, C_WHITE);  // cockpit highlight
    }
}

static void _ast_draw_rock(const AsteroidRock &r, uint16_t col) {
    int16_t rx = (int16_t)r.x;
    int16_t ry = (int16_t)r.y;
    int16_t rs = r.size;
    display_fill_circle(rx, ry, rs, col);
    if (col != C_BG) {
        display_circle(rx, ry, rs, C_LTGREY);  // outline
    }
}

static void _game_asteroids() {
    if (!_ast.hdr) {
        display_clear_bg();
        display_header("ASTEROIDS", G_HDR_H);
        display_footer("THRUST", "BACK");
        display_hline(0, G_CONT_Y,     DISP_W, C_DKGREY);
        display_hline(0, G_FOOT_Y - 1, DISP_W, C_DKGREY);
        // Draw lives
        for (uint8_t i = 0; i < 3; i++) {
            uint16_t c = (i < _ast.lives) ? C_CYAN : C_PANEL;
            display_fill_circle(4 + i * 8, G_HDR_H - 5, 3, c);
        }
        _ast_draw_ship(_ast.ship_y, C_CYAN);
        _ast.hdr = true;
    }

    // Dead screen
    if (_ast.dead) {
        if (button_just_pressed(BTN_CYCLE)) {
            _ast_init();
            display_clear_bg();
            display_header("ASTEROIDS", G_HDR_H);
            display_footer("THRUST", "BACK");
            display_hline(0, G_CONT_Y,     DISP_W, C_DKGREY);
            display_hline(0, G_FOOT_Y - 1, DISP_W, C_DKGREY);
            for (uint8_t i = 0; i < 3; i++)
                display_fill_circle(4 + i * 8, G_HDR_H - 5, 3, C_CYAN);
            _ast_draw_ship(_ast.ship_y, C_CYAN);
            _ast.dead = false;
            _ast.hdr  = true;
        }
        if (button_just_pressed(BTN_SELECT)) {
            _ast.hdr = false;
            _g_back_to_menu();
        }
        return;
    }

    if (button_just_pressed(BTN_SELECT)) {
        _ast.hdr = false;
        _g_back_to_menu();
        return;
    }

    constexpr unsigned long AST_MS = 33;
    if (millis() - _ast.last_frame < AST_MS) return;
    _ast.last_frame = millis();

    _ast.score++;

    // Spawn rocks
    if (millis() - _ast.last_spawn > _ast.spawn_interval_ms) {
        _ast_spawn_rock();
        _ast.last_spawn = millis();
        // Gradually speed up spawning
        if (_ast.spawn_interval_ms > 400) _ast.spawn_interval_ms -= 5;
    }

    // Erase ship
    _ast_draw_ship(_ast.ship_y, C_BG);

    // Physics
    if (button_is_pressed(BTN_CYCLE)) {
        _ast.ship_vy += AS_THRUST;
    }
    _ast.ship_vy += AS_GRAVITY;
    if (_ast.ship_vy >  4.0f) _ast.ship_vy =  4.0f;
    if (_ast.ship_vy < -4.0f) _ast.ship_vy = -4.0f;
    _ast.ship_y += _ast.ship_vy;

    // Clamp to arena
    if (_ast.ship_y < (float)(G_CONT_Y + 5)) {
        _ast.ship_y = (float)(G_CONT_Y + 5); _ast.ship_vy = 0.0f;
    }
    if (_ast.ship_y > (float)(G_FOOT_Y - 6)) {
        _ast.ship_y = (float)(G_FOOT_Y - 6); _ast.ship_vy = 0.0f;
    }

    // Move and draw rocks
    for (uint8_t i = 0; i < AS_MAX_ROCKS; i++) {
        if (!_ast.rocks[i].active) continue;

        _ast_draw_rock(_ast.rocks[i], C_BG);  // erase
        _ast.rocks[i].x -= AS_ROCK_SPD;

        if (_ast.rocks[i].x < -8) {
            _ast.rocks[i].active = false;
            continue;
        }

        // Collision with ship
        if (!_ast.invuln) {
            float dx = _ast.rocks[i].x - (float)AS_SHIP_X;
            float dy = _ast.rocks[i].y - _ast.ship_y;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < (float)(_ast.rocks[i].size + 4)) {
                _ast.rocks[i].active = false;
                _ast.lives--;
                led_red();
                _ast.invuln       = true;
                _ast.invuln_until = millis() + 1500;
                // Redraw lives
                for (uint8_t j = 0; j < 3; j++) {
                    uint16_t c = (j < _ast.lives) ? C_CYAN : C_PANEL;
                    display_fill_circle(4 + j * 8, G_HDR_H - 5, 3, c);
                }
                if (_ast.lives == 0) {
                    _ast.dead = true;
                    char msg[20]; snprintf(msg, sizeof(msg), "SCORE: %u", _ast.score);
                    display_fill_rect(20, G_CONT_Y + 10, DISP_W - 40, 30, C_PANEL);
                    display_rect     (20, G_CONT_Y + 10, DISP_W - 40, 30, C_RED);
                    display_text_centred(0, G_CONT_Y + 13, DISP_W, "GAME OVER", C_RED,    C_PANEL, 1);
                    display_text_centred(0, G_CONT_Y + 23, DISP_W, msg,         C_YELLOW, C_PANEL, 1);
                    display_text_centred(0, G_CONT_Y + 33, DISP_W, "R=RETRY",   C_GREY,   C_PANEL, 1);
                    return;
                }
                continue;
            }
        } else if (millis() > _ast.invuln_until) {
            _ast.invuln = false;
        }

        _ast_draw_rock(_ast.rocks[i], C_GREY);  // draw
    }

    // Draw ship (blink while invulnerable)
    bool draw_ship = true;
    if (_ast.invuln && ((millis() / 150) & 1)) draw_ship = false;
    if (draw_ship) _ast_draw_ship(_ast.ship_y, C_CYAN);

    // Score counter
    if (_ast.score % 20 == 0) {
        char sc[10]; snprintf(sc, sizeof(sc), "%u", _ast.score);
        display_fill_rect(DISP_W - 36, 0, 36, G_HDR_H, C_PANEL);
        display_text(DISP_W - 6 * (int16_t)strlen(sc) - 2, 3, sc, C_YELLOW, C_PANEL, 1);
    }

    led_green();
}

// =============================================================================
// Games sub-menu
// =============================================================================

struct GamesMenuItem {
    const char *label;
    const char *icon;
    GameState   target;
};

static const GamesMenuItem GAMES_ITEMS[] = {
    { "Snake",      "S", GameState::GAME_SNAKE      },
    { "Pong",       "P", GameState::GAME_PONG       },
    { "Asteroids",  "A", GameState::GAME_ASTEROIDS  },
};
static constexpr uint8_t GAMES_LEN = sizeof(GAMES_ITEMS) / sizeof(GAMES_ITEMS[0]);
static uint8_t _g_menu_idx = 0;

static void _draw_games_menu() {
    display_clear_bg();
    display_header("GAMES", G_HDR_H);

    constexpr int16_t ITEM_H  = 16;
    constexpr int16_t START_Y = G_CONT_Y + 4;

    for (uint8_t i = 0; i < GAMES_LEN; i++) {
        int16_t y = START_Y + i * ITEM_H;
        bool sel  = (i == _g_menu_idx);

        if (sel) {
            display_fill_rect(0, y, DISP_W, ITEM_H - 1, C_PANEL);
            display_fill_rect(0, y, 2, ITEM_H - 1, C_ACCENT);
            display_text(5,  y + 4, GAMES_ITEMS[i].icon,  C_ACCENT, C_PANEL, 1);
            display_text(14, y + 4, GAMES_ITEMS[i].label, C_WHITE,  C_PANEL, 1);
        } else {
            display_fill_rect(0, y, DISP_W, ITEM_H - 1, C_BG);
            display_fill_rect(0, y, 1, ITEM_H - 1, C_DKGREY);
            display_text(5,  y + 4, GAMES_ITEMS[i].icon,  C_DKGREY, C_BG, 1);
            display_text(14, y + 4, GAMES_ITEMS[i].label, C_GREY,   C_BG, 1);
        }
    }

    display_footer("NEXT", "ENTER");
    // BACK hint
    display_text(2, G_FOOT_Y + 1, "HOLD:BACK", C_DKGREY, C_BG, 1);
}

static bool _handle_games_menu() {
    if (_g_needs_draw) {
        _draw_games_menu();
        _g_needs_draw = false;
    }

    if (button_just_pressed(BTN_CYCLE)) {
        uint8_t old = _g_menu_idx;
        _g_menu_idx = (_g_menu_idx + 1) % GAMES_LEN;
        // Partial update: deselect old, select new
        constexpr int16_t ITEM_H  = 16;
        constexpr int16_t START_Y = G_CONT_Y + 4;
        auto redraw_item = [](uint8_t i, bool sel) {
            int16_t y = START_Y + i * ITEM_H;
            if (sel) {
                display_fill_rect(0, y, DISP_W, ITEM_H - 1, C_PANEL);
                display_fill_rect(0, y, 2, ITEM_H - 1, C_ACCENT);
                display_text(5,  y + 4, GAMES_ITEMS[i].icon,  C_ACCENT, C_PANEL, 1);
                display_text(14, y + 4, GAMES_ITEMS[i].label, C_WHITE,  C_PANEL, 1);
            } else {
                display_fill_rect(0, y, DISP_W, ITEM_H - 1, C_BG);
                display_fill_rect(0, y, 1, ITEM_H - 1, C_DKGREY);
                display_text(5,  y + 4, GAMES_ITEMS[i].icon,  C_DKGREY, C_BG, 1);
                display_text(14, y + 4, GAMES_ITEMS[i].label, C_GREY,   C_BG, 1);
            }
        };
        redraw_item(old,          false);
        redraw_item(_g_menu_idx,  true);
        return false;
    }

    if (button_just_pressed(BTN_SELECT)) {
        // Launch selected game
        _gstate       = GAMES_ITEMS[_g_menu_idx].target;
        _g_needs_draw = true;
        display_clear_bg();

        // Init the chosen game
        if (_gstate == GameState::GAME_SNAKE)     _snake_init();
        if (_gstate == GameState::GAME_PONG)      _pong_init();
        if (_gstate == GameState::GAME_ASTEROIDS) _ast_init();

        led_blue();
        return false;
    }

    // Long-hold CYCLE → exit games back to main menu
    if (button_held_ms(BTN_CYCLE) > 600UL) {
        _gstate       = GameState::GAMES_MENU;
        _g_needs_draw = true;
        return true;   // signal: go back to main UI menu
    }

    return false;
}

// =============================================================================
// Public entry point
// =============================================================================
bool games_menu() {
    switch (_gstate) {
        case GameState::GAMES_MENU:
            return _handle_games_menu();

        case GameState::GAME_SNAKE:
            _game_snake();
            // Long-hold CYCLE exits game → back to games menu
            if (button_held_ms(BTN_CYCLE) > 800UL && !_sn_dead) {
                _sn_hdr = false;
                _g_back_to_menu();
            }
            return false;

        case GameState::GAME_PONG:
            _game_pong();
            return false;

        case GameState::GAME_ASTEROIDS:
            _game_asteroids();
            return false;
    }
    return false;
}
