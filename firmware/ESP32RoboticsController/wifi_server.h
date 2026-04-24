#pragma once

// =============================================================================
// WiFi Access Point + Async Web Dashboard
// =============================================================================
// Creates a soft AP and hosts a live dashboard at http://192.168.4.1/
//
// Transport : AsyncWebSocket — ESP32 pushes JSON to all connected browsers
//             at WIFI_WS_PUSH_MS interval. No polling from the client.
//
// Extra endpoints:
//   GET /api/scan  — trigger WiFi network scan, returns JSON array
//   GET /api/ble   — trigger BLE device scan, returns JSON array
//
// AP credentials:
//   SSID     : RoboController
//   Password : robot1234
//   IP       : 192.168.4.1
// =============================================================================

#define WIFI_AP_SSID        "RoboController"
#define WIFI_AP_PASSWORD    "robot1234"
#define WIFI_AP_CHANNEL     6
#define WIFI_WS_PUSH_MS     200   // WebSocket push interval (ms)
#define WIFI_BLE_SCAN_SEC   4     // BLE scan duration per request (s)

// Start the soft AP, WebSocket server, and BLE scanner.
// Call once from setup() after all peripheral inits are done.
void wifi_server_init();

// Call every loop() — pushes WebSocket data and cleans up clients.
void wifi_server_update();

// ---------------------------------------------------------------------------
// Local device-side scan helpers (blocking, safe to call from ui.cpp)
// ---------------------------------------------------------------------------

// Number of stations currently connected to the soft AP.
int wifi_ap_client_count();

struct WifiNetInfo {
    char ssid[33];
    int  rssi;
    int  ch;
    bool enc;   // true = encrypted
};
// Scans for nearby WiFi networks. Fills `out[0..maxn-1]`, returns count.
// Blocks for ~1–2 s. maxn recommended ≤ 20.
int wifi_scan_networks(WifiNetInfo* out, int maxn);

struct BleDevInfo {
    char addr[18];
    char name[32];
    int  rssi;
};
// Scans for BLE advertisements for `secs` seconds. Fills `out[0..maxn-1]`,
// returns count. Blocking. maxn recommended ≤ 20.
int ble_scan_devices(BleDevInfo* out, int maxn, int secs = WIFI_BLE_SCAN_SEC);

// Pump the async server and push WebSocket frames. Call every loop().
void wifi_server_update();
