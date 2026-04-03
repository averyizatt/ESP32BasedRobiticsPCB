#pragma once

#include <Arduino.h>
#include <SPI.h>
#include "config.h"

// =============================================================================
// SPI Display Interface
// =============================================================================
// Wraps a generic SPI display driver (e.g., ST7735, ST7789, ILI9341).
// Replace the driver includes and constructor below with the one matching
// your specific display module.
//
// Required library: Adafruit GFX + your display-specific Adafruit driver,
// OR TFT_eSPI configured for your display panel.
// =============================================================================

// Initialise SPI bus and display. Call once in setup().
void display_init();

// Clear the display to black.
void display_clear();

// Print a line of text at the given row (0-indexed, ~10 px per row).
void display_print_line(uint8_t row, const String &text);

// Draw a simple key=value status line (used for telemetry).
void display_status(uint8_t row, const char *label, float value);

// Flush / refresh the display if using a buffered driver.
void display_update();
