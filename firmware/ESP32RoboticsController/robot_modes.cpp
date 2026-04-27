#include "robot_modes.h"
#include "autonomous_modes.h"
#include "display.h"
#include "buttons.h"
#include "led.h"
#include "pot.h"
#include "motors.h"
#include "ultrasonics.h"
#include "imu.h"
#include <XboxSeriesXControllerESP32_asukiaaa.hpp>

// =============================================================================
// Robot Modes â€” wires the autonomous_modes engine into the UI menu.
//
// Sensor mapping (4Ã— HC-SR04):
//   US1 = front   US2 = left   US3 = right   US4 = rear (unused by engine)
//
// Motor mapping:
//   MOTOR1 = left track    MOTOR2 = right track
// =============================================================================

// ---------------------------------------------------------------------------
// Layout constants (mirror ui.cpp conventions)
// ---------------------------------------------------------------------------
static constexpr int16_t RM_HDR_H     = 13;
static constexpr int16_t RM_HDR_BOT   = RM_HDR_H + 1;
static constexpr int16_t RM_FOOT_Y    = DISP_H - ROW_PX - 1;
static constexpr int16_t RM_CONTENT_H = RM_FOOT_Y - RM_HDR_BOT;

// ---------------------------------------------------------------------------
// Mode descriptor (use a different name to avoid clash with RobotMode enum)
// ---------------------------------------------------------------------------
struct RModeEntry {
    const char *label;   // short name shown in the list   (â‰¤16 chars)
    const char *desc;    // one-line description shown below the list
    void (*run)();       // called every loop() tick while mode is active
};

// ---------------------------------------------------------------------------
// Autonomous engine — singleton
// ---------------------------------------------------------------------------
static RobotModes _am;
static bool       _am_ready = false;

// ---------------------------------------------------------------------------
// Xbox BLE controller — singleton
// ---------------------------------------------------------------------------
static XboxSeriesXControllerESP32_asukiaaa::Core _xbox;
static bool _xbox_ready = false;

static void _xbox_ensure_ready() {
    if (!_xbox_ready) {
        _xbox.begin();
        _xbox_ready = true;
    }
}

static void _motor_cb(int left, int right) {
    motor_set(MotorId::MOTOR1, left);
    motor_set(MotorId::MOTOR2, right);
}

// Build a SensorInput from board peripherals (rate-limited by callers).
static SensorInput _read_sensors() {
    SensorInput s = {};

    // Ultrasonics: US1=front, US2=left, US3=right
    float us[4] = { 0, 0, 0, 0 };
    ultrasonics_read_all_cm(us);
    s.distFront = (us[0] > 0.0f) ? us[0] : 400.0f;
    s.distLeft  = (us[1] > 0.0f) ? us[1] : 400.0f;
    s.distRight = (us[2] > 0.0f) ? us[2] : 400.0f;
    s.wallAngle = 0.0f;

    // IMU â€” convert m/sÂ² â†’ g
    ImuData imu = {};
    if (imu_read(imu)) {
        s.accelX = imu.accel_x / 9.81f;
        s.accelY = imu.accel_y / 9.81f;
        s.accelZ = imu.accel_z / 9.81f;
        s.gyroX  = imu.gyro_x;
        s.gyroY  = imu.gyro_y;
        s.gyroZ  = imu.gyro_z;
    }

    s.rcConnected = false;
    return s;
}

static void _am_ensure_ready() {
    if (!_am_ready) {
        _am.setMotorCallback(_motor_cb);
        _am_ready = true;
    }
}

// Small live HUD drawn at top of content area (called every update cycle).
static void _draw_hud(const SensorInput &s) {
    int ml = 0, mr = 0;
    _am.getMotorSpeeds(ml, mr);

    char line1[22], line2[22];
    snprintf(line1, sizeof(line1), "L:%-4d R:%-4d", ml, mr);
    snprintf(line2, sizeof(line2), "F:%.0f L:%.0f R:%.0f",
             s.distFront, s.distLeft, s.distRight);

    display_fill_rect(0, RM_HDR_BOT,              DISP_W, ROW_PX + 1, C_BG);
    display_fill_rect(0, RM_HDR_BOT + ROW_PX + 2, DISP_W, ROW_PX + 1, C_BG);
    display_text(2, RM_HDR_BOT,              line1, C_CYAN,   C_BG, 1);
    display_text(2, RM_HDR_BOT + ROW_PX + 2, line2, C_YELLOW, C_BG, 1);
}

// Core tick shared by all modes â€” called every loop(), rate-limited internally.
static void _tick(RobotMode mode) {
    _am_ensure_ready();

    static unsigned long _last = 0;
    constexpr unsigned long RATE_MS = 50;   // ~20 Hz (each ultrasonic read ~10 ms)
    if (millis() - _last < RATE_MS) return;
    _last = millis();

    _am.setMode(mode);
    SensorInput s = _read_sensors();
    _am.update(s);
    _draw_hud(s);
}

// ---------------------------------------------------------------------------
// Per-mode tick wrappers
// ---------------------------------------------------------------------------
static void _run_rc() {
    _am_ensure_ready();
    _xbox_ensure_ready();

    // Let the library manage BLE scanning / reconnection every tick
    _xbox.onLoop();

    // Control update at ~50 Hz
    static unsigned long _last_rc = 0;
    constexpr unsigned long RC_RATE_MS = 20;
    if (millis() - _last_rc < RC_RATE_MS) return;
    _last_rc = millis();

    SensorInput s = {};
    if (_xbox.isConnected()) {
        // joyLVert: 0 = full up (forward), 0xffff = full down (backward)
        // joyRHori: 0 = full left,          0xffff = full right
        int throttle = (32767 - (int)_xbox.xboxNotif.joyLVert) / 64;  // -512..512
        int steering = ((int)_xbox.xboxNotif.joyRHori - 32767)  / 64; // -512..512
        s.rcThrottle  = throttle;
        s.rcSteering  = steering;
        s.rcConnected = true;
    } else {
        s.rcConnected = false;
    }

    _am.setMode(MODE_RC_CONTROL);
    _am.update(s);

    // HUD
    int ml = 0, mr = 0;
    _am.getMotorSpeeds(ml, mr);
    char line1[22], line2[22];
    snprintf(line1, sizeof(line1), "L:%-4d R:%-4d", ml, mr);
    if (_xbox.isConnected()) {
        snprintf(line2, sizeof(line2), "T:%-4d S:%-4d",
                 (int)s.rcThrottle, (int)s.rcSteering);
    } else {
        snprintf(line2, sizeof(line2), "Scanning BLE...");
    }
    display_fill_rect(0, RM_HDR_BOT,               DISP_W, ROW_PX + 1, C_BG);
    display_fill_rect(0, RM_HDR_BOT + ROW_PX + 2,  DISP_W, ROW_PX + 1, C_BG);
    display_text(2, RM_HDR_BOT,               line1, C_CYAN, C_BG, 1);
    display_text(2, RM_HDR_BOT + ROW_PX + 2,  line2,
                 _xbox.isConnected() ? C_YELLOW : C_ORANGE, C_BG, 1);
}
static void _run_uart()   { _tick(MODE_UART_CONTROL); }
static void _run_sok()    { _tick(MODE_SOK_CONTROL);  }
static void _run_auto()   { _tick(MODE_AUTONOMOUS);   }
static void _run_simple() { _tick(MODE_SIMPLE_AUTO);  }
static void _run_wall()   { _tick(MODE_WALL_FOLLOW);  }
static void _run_premap() { _tick(MODE_PREMAP_NAV);   }

// ---------------------------------------------------------------------------
// Mode list â€” all 7 modes from autonomous_modes.h
// ---------------------------------------------------------------------------
static const RModeEntry ROBOT_MODE_LIST[] = {
    { "RC Control",   "Tank-drive joystick",      _run_rc     },
    { "UART Control", "UART-commanded forward",   _run_uart   },
    { "Socket Ctrl",  "Socket motor pass-through",_run_sok    },
    { "Autonomous",   "Adv state-machine (SLAM)",  _run_auto   },
    { "Simple Auto",  "PID fwd + recovery",       _run_simple },
    { "Wall Follow",  "Left-hand PID follower",   _run_wall   },
    { "Premap Nav",   "Map-seeded autonomous",    _run_premap },
};
static constexpr uint8_t ROBOT_MODE_COUNT =
    (uint8_t)(sizeof(ROBOT_MODE_LIST) / sizeof(ROBOT_MODE_LIST[0]));

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static bool    _needs_draw  = true;
static uint8_t _idx         = 0;
static bool    _running     = false;
static uint16_t _pot_ref    = 0;

// ---------------------------------------------------------------------------
// Menu drawing
// ---------------------------------------------------------------------------
static void _draw_menu() {
    display_clear_bg();
    display_header("Robot Modes");

    const uint8_t visible = (uint8_t)(RM_CONTENT_H / (ROW_PX + 1));
    uint8_t start = (_idx >= visible) ? _idx - visible + 1 : 0;

    for (uint8_t i = start;
         i < ROBOT_MODE_COUNT && i < start + visible; i++) {
        int16_t  y   = RM_HDR_BOT + (i - start) * (ROW_PX + 1);
        bool     sel = (i == _idx);
        uint16_t bg  = sel ? C_PANEL : C_BG;
        uint16_t fg  = sel ? C_WHITE : C_GREY;

        display_fill_rect(0, y, DISP_W, ROW_PX, bg);
        char buf[22];
        snprintf(buf, sizeof(buf), sel ? ">%-16s" : " %-16s",
                 ROBOT_MODE_LIST[i].label);
        display_text(2, y, buf, fg, bg, 1);
    }

    // Description shown in footer to avoid overlapping list items
    display_footer(ROBOT_MODE_LIST[_idx].desc, "BOT back");
}

// ---------------------------------------------------------------------------
// Public entry â€” called every loop() tick from ui.cpp
// Returns true to signal "exit back to main menu".
// ---------------------------------------------------------------------------
bool robot_modes_menu() {
    // â”€â”€ Draw on entry / after stopping a mode â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    if (_needs_draw) {
        _running    = false;
        _needs_draw = false;
        _pot_ref    = pot_raw();
        // Stop motors whenever we return to the list
        motor_set(MotorId::MOTOR1, 0);
        motor_set(MotorId::MOTOR2, 0);
        _draw_menu();
    }

    // -- Active mode running ------------------------------------------------
    if (_running) {
        ROBOT_MODE_LIST[_idx].run();

        // Bottom button stops the active mode and returns to the mode list.
        static bool stop_was_down = false;
        bool stop_down = button_is_pressed(BTN_BACK);
        if (stop_down && !stop_was_down) {
            stop_was_down = true;
            _running       = false;
            _needs_draw    = true;
            led_green();
            Serial.printf("[robot_modes] stopped: %s\n",
                          ROBOT_MODE_LIST[_idx].label);
        }
        stop_was_down = stop_down;
        return false;
    }

    // -- Menu navigation ----------------------------------------------------
    static bool enter_was_down = false;
    static bool back_was_down  = false;
    bool enter_down = button_is_pressed(BTN_ENTER);
    bool back_down  = button_is_pressed(BTN_BACK);

    // Top starts the selected mode. Pot moves the highlight.
    if (enter_down && !enter_was_down) {
        enter_was_down = true;
        _running = true;
        _am_ensure_ready();
        _am.resetSimpleAutoState();
        _am.resetWallFollowState();
        display_clear_bg();
        display_header(ROBOT_MODE_LIST[_idx].label);
        display_footer("BOT stop", "");
        led_blue();
        Serial.printf("[robot_modes] starting: %s\n",
                      ROBOT_MODE_LIST[_idx].label);
        return false;
    }

    // Bottom button exits back to the main menu.
    if (back_down && !back_was_down) {
        back_was_down = true;
        _needs_draw   = true;
        led_green();
        Serial.println("[robot_modes] back to main menu");
        return true;
    }
    enter_was_down = enter_down;
    back_was_down  = back_down;

    // Pot scrolls relative to its position when the menu is drawn. It runs
    // after buttons so analog movement cannot interfere with button edges.
    constexpr int16_t POT_STEP = 420;
    int16_t pot_delta = (int16_t)pot_raw() - (int16_t)_pot_ref;
    if (abs(pot_delta) >= POT_STEP) {
        int8_t steps = (int8_t)(pot_delta / POT_STEP);
        _pot_ref += (int16_t)steps * POT_STEP;

        int16_t next = (int16_t)_idx + steps;
        while (next < 0) next += ROBOT_MODE_COUNT;
        next %= ROBOT_MODE_COUNT;
        if ((uint8_t)next != _idx) {
            _idx = (uint8_t)next;
            _needs_draw = true;
            Serial.printf("[robot_modes] %u/%u %s\n", _idx + 1, ROBOT_MODE_COUNT,
                          ROBOT_MODE_LIST[_idx].label);
        }
    }

    return false;
}
