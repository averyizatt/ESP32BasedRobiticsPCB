#pragma once

#include <Arduino.h>

// =============================================================================
// UI — Boot sequence, main menu, sub-screens, idle animation
// =============================================================================
// Navigation:
//   BTN_CYCLE  (BTN1) — advance cursor / cycle through items
//   BTN_SELECT (BTN2) — confirm selection / return to menu from sub-screens
// =============================================================================

// Run the animated boot sequence and peripheral self-test.
// Must be called after display_init(). Blocks until boot is complete.
// imu_ok: pass in the IMU init result so the UI can show probe status.
void ui_boot(bool imu_ok);

// Call once per loop() after buttons_update().
void ui_update();
