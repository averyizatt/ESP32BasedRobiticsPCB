#pragma once

#include <Arduino.h>

// =============================================================================
// Robot Modes module — autonomous behaviour routines for the ESP32 robotics
// controller.  Each mode receives CPU time every loop() tick while active.
// =============================================================================
//
// Adding a new mode
// -----------------
//  1. Write a void setup function and a void loop/tick function in
//     robot_modes.cpp (or a separate .cpp you #include from there).
//  2. Add an entry to ROBOT_MODE_LIST in robot_modes.cpp:
//       { "My Mode", "short description", _my_mode_run }
//     The run function is called every loop() tick while the mode is active.
//     Set run = nullptr for a placeholder that isn't implemented yet.
//  3. Recompile — no changes to ui.cpp required.
//
// Navigation (mirrors Games module)
// ----------------------------------
//   BTN_TOP short press  → next mode
//   BTN_TOP long press   → start highlighted mode
//   BTN_BOTTOM           → stop active mode / back to main menu
//   Pot                  → select mode after the dial moves
//
// Call robot_modes_menu() from the UI update loop when in SCREEN_ROBOT_MODES.
// Returns true when the user exits back to the main menu.
// =============================================================================

// Dispatch robot-modes sub-menu and active-mode loop.
// Returns true when the user navigates back to the main menu.
bool robot_modes_menu();
