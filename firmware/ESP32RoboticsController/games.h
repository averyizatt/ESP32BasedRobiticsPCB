#pragma once

#include <Arduino.h>

// =============================================================================
// Games module — arcade mini-games for the 160×80 TFT + 2-button controller
// =============================================================================
// Navigation inside games:
//   BTN_CYCLE  (BTN1) — primary action (steer / shoot / move)
//   BTN_SELECT (BTN2) — secondary / back
//
// Call games_menu() from the UI update loop when in SCREEN_GAMES state.
// Returns true when the player exits back to the main menu.
// =============================================================================

// Show games sub-menu and dispatch to individual games.
// Returns true when the user navigates back to the main menu.
bool games_menu();
