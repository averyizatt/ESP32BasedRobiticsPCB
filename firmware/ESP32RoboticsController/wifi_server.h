#pragma once

// =============================================================================
// WiFi Access Point + Web Dashboard
// =============================================================================
// Creates a soft AP named "RoboController" and hosts a live dashboard at
// http://192.168.4.1/ that shows sensor readings, hall counts, IMU data, etc.
//
// AP credentials:
//   SSID     : RoboController
//   Password : robot1234
//   IP       : 192.168.4.1
//
// Endpoints:
//   GET /          — full HTML dashboard (auto-refreshes via JS)
//   GET /api/data  — JSON snapshot of all sensor data
// =============================================================================

#define WIFI_AP_SSID      "RoboController"
#define WIFI_AP_PASSWORD  "robot1234"
#define WIFI_AP_CHANNEL   6

// Start the soft AP and HTTP server.
// Call once from setup() after all peripheral inits are done.
void wifi_server_init();

// Handle pending HTTP client requests. Call every loop() iteration.
void wifi_server_update();
