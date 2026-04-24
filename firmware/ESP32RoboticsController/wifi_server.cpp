#include "wifi_server.h"
#include "imu.h"
#include "ultrasonics.h"
#include "hall_sensors.h"
#include "buttons.h"
#include "config.h"
#include "motors.h"
#include "servos.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <NimBLEDevice.h>

// ---------------------------------------------------------------------------
// Server + WebSocket
// ---------------------------------------------------------------------------
static AsyncWebServer _server(80);
static AsyncWebSocket _ws("/ws");
static uint32_t       _lastPush = 0;

// ---------------------------------------------------------------------------
// BLE scan results (filled by scan callback, read by HTTP handler)
// ---------------------------------------------------------------------------
struct BleEntry { String addr; String name; int rssi; };
static std::vector<BleEntry> _bleResults;
static volatile bool          _bleBusy = false;

class _BleCallback : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* dev) override {
        BleEntry e;
        e.addr = dev->getAddress().toString().c_str();
        e.name = dev->haveName() ? dev->getName().c_str() : "";
        e.rssi = dev->getRSSI();
        _bleResults.push_back(e);
    }
};
static _BleCallback _bleCb;

// ---------------------------------------------------------------------------
// BLE GATT server — globals
// ---------------------------------------------------------------------------
static NimBLEServer*         _gattServer  = nullptr;
static NimBLECharacteristic* _sensorChar  = nullptr;
static NimBLECharacteristic* _motorChar   = nullptr;
static NimBLECharacteristic* _servoChar   = nullptr;
static uint32_t              _lastBlePush = 0;

class _MotorBleCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) override {
        std::string v = c->getValue();
        int l = 0, r = 0;
        if (sscanf(v.c_str(), "l:%d,r:%d", &l, &r) == 2) {
            l = constrain(l, -255, 255);
            r = constrain(r, -255, 255);
            motor_set(MotorId::MOTOR1, l);
            motor_set(MotorId::MOTOR2, r);
            Serial.printf("[ble] motor l=%d r=%d\n", l, r);
        }
    }
};
static _MotorBleCallback _motorBleCb;

// Write callback: servo command "<id>:<angle>"  e.g. "1:90"
class _ServoBleCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) override {
        std::string v = c->getValue();
        int id = 1, angle = 90;
        if (sscanf(v.c_str(), "%d:%d", &id, &angle) == 2) {
            angle = angle < 0 ? 0 : angle > 180 ? 180 : angle;
            servo_set_angle(id == 2 ? ServoId::SERVO2 : ServoId::SERVO1, angle);
            Serial.printf("[ble] servo id=%d angle=%d\n", id, angle);
        }
    }
};
static _ServoBleCallback _servoBleCb;

// ---------------------------------------------------------------------------
// Cached ultrasonic readings — updated at a reduced rate to avoid
// blocking the async web server task for up to 120 ms per push.
// ---------------------------------------------------------------------------
static float    _usCached[4]  = { -1.f, -1.f, -1.f, -1.f };
static uint32_t _usLastReadMs = 0;
static constexpr uint32_t US_CACHE_MS = 2000; // re-read every 2 s

static void _refreshUsCache() {
    if (millis() - _usLastReadMs >= US_CACHE_MS) {
        _usLastReadMs = millis();
        ultrasonics_read_all_cm(_usCached);
    }
}

// Compact JSON for BLE notify (fits in typical ATT MTU)
static void _buildBleJson(char* buf, size_t sz) {
    ImuData imu = {};
    bool imu_ok = imu_read(imu);
    snprintf(buf, sz,
        "{\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,"
        "\"gx\":%.1f,\"gy\":%.1f,\"gz\":%.1f,"
        "\"u\":[%.1f,%.1f,%.1f,%.1f]}",
        imu_ok ? imu.accel_x : 0.f, imu_ok ? imu.accel_y : 0.f, imu_ok ? imu.accel_z : 0.f,
        imu_ok ? imu.gyro_x  : 0.f, imu_ok ? imu.gyro_y  : 0.f, imu_ok ? imu.gyro_z  : 0.f,
        _usCached[0], _usCached[1], _usCached[2], _usCached[3]);
}

// ---------------------------------------------------------------------------
// Build sensor JSON into a fixed-size buffer
// ---------------------------------------------------------------------------
static void _buildJson(char* buf, size_t sz) {
    ImuData imu = {};
    bool imu_ok = imu_read(imu);
    // Use cached ultrasonic readings — do NOT call ultrasonics_read_all_cm()
    // here because it blocks up to 120 ms and would stall the async server.
    const float* us = _usCached;

    snprintf(buf, sz,
        "{"
        "\"t\":\"data\","
        "\"ssid\":\"%s\","
        "\"ip\":\"%s\","
        "\"uptime_ms\":%lu,"
        "\"free_heap\":%u,"
        "\"imu_ok\":%s,"
        "\"imu\":{"
          "\"accel_x\":%.4f,\"accel_y\":%.4f,\"accel_z\":%.4f,"
          "\"gyro_x\":%.4f,\"gyro_y\":%.4f,\"gyro_z\":%.4f,"
          "\"temp_c\":%.2f"
        "},"
        "\"us\":[%.2f,%.2f,%.2f,%.2f],"
        "\"hall\":["
          "{\"count\":%ld,\"pps\":%.2f},"
          "{\"count\":%ld,\"pps\":%.2f}"
        "],"
        "\"btn1\":%s,"
        "\"btn2\":%s"
        "}",
        WIFI_AP_SSID,
        WiFi.softAPIP().toString().c_str(),
        millis(),
        (unsigned)ESP.getFreeHeap(),
        imu_ok ? "true" : "false",
        imu.accel_x, imu.accel_y, imu.accel_z,
        imu.gyro_x,  imu.gyro_y,  imu.gyro_z,
        imu.temp_c,
        us[0], us[1], us[2], us[3],
        hall_get_count(1), hall_get_pulses_per_second(1),
        hall_get_count(2), hall_get_pulses_per_second(2),
        button_is_pressed(ButtonId::BTN1) ? "true" : "false",
        button_is_pressed(ButtonId::BTN2) ? "true" : "false"
    );
}

// ---------------------------------------------------------------------------
// HTML dashboard â€” WebSocket client, tabbed UI
// ---------------------------------------------------------------------------
static const char _HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RoboController</title>
<style>
:root{
  --bg:#0a0c10;--surface:#141820;--surface2:#1c2230;--border:#252d3d;
  --accent:#3b82f6;--accent2:#06b6d4;--green:#22c55e;--yellow:#eab308;
  --red:#ef4444;--purple:#a855f7;--pink:#ec4899;--orange:#f97316;
  --text:#e2e8f0;--muted:#64748b;--dim:#334155;
  --r:12px;--font:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;
}
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
body{font-family:var(--font);background:var(--bg);color:var(--text);min-height:100vh}

/* Header */
header{background:var(--surface);border-bottom:1px solid var(--border);padding:10px 20px;
  display:flex;align-items:center;justify-content:space-between;position:sticky;top:0;z-index:20}
.logo{display:flex;align-items:center;gap:10px}
.logo-icon{width:30px;height:30px;border-radius:8px;
  background:linear-gradient(135deg,var(--accent),var(--accent2));
  display:flex;align-items:center;justify-content:center;flex-shrink:0}
.logo-title{font-size:.95rem;font-weight:600;letter-spacing:-.02em}
.logo-sub{font-size:.65rem;color:var(--muted)}
.hdr-r{display:flex;align-items:center;gap:14px}
#uptime{font-size:.75rem;color:var(--muted);font-variant-numeric:tabular-nums}
.pill{display:flex;align-items:center;gap:5px;font-size:.72rem;color:var(--muted)}
.dot{width:7px;height:7px;border-radius:50%;background:var(--dim);flex-shrink:0;transition:background .3s}
.dot.live{background:var(--green);box-shadow:0 0 6px var(--green);animation:blink 2s ease-in-out infinite}
.dot.err{background:var(--red)}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.3}}

/* Tabs */
.tabs{background:var(--surface);border-bottom:1px solid var(--border);
  display:flex;gap:0;padding:0 20px;overflow-x:auto}
.tab{padding:9px 16px;font-size:.78rem;font-weight:500;cursor:pointer;
  color:var(--muted);border-bottom:2px solid transparent;white-space:nowrap;transition:all .15s}
.tab:hover{color:var(--text)}
.tab.active{color:var(--accent);border-bottom-color:var(--accent)}

/* Tab pages */
.page{display:none}.page.active{display:block}

/* Grid */
main{padding:16px 20px;max-width:1300px;margin:0 auto;
  display:grid;gap:14px;grid-template-columns:repeat(12,1fr)}
.c3{grid-column:span 3}.c4{grid-column:span 4}.c5{grid-column:span 5}
.c6{grid-column:span 6}.c7{grid-column:span 7}.c12{grid-column:span 12}
@media(max-width:960px){.c3,.c4,.c5,.c6,.c7{grid-column:span 6}}
@media(max-width:580px){.c3,.c4,.c5,.c6,.c7{grid-column:span 12};main{padding:10px 12px}}

/* Card */
.card{background:var(--surface);border:1px solid var(--border);
  border-radius:var(--r);padding:14px;transition:border-color .2s}
.card:hover{border-color:#3a4560}
.card-hdr{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px}
.card-title{font-size:.68rem;font-weight:600;text-transform:uppercase;letter-spacing:.08em;color:var(--muted)}
.card-ico{width:18px;height:18px;opacity:.4}

/* Rows */
.irow{display:flex;justify-content:space-between;align-items:center;
  padding:5px 0;border-bottom:1px solid var(--border);font-size:.82rem}
.irow:last-of-type{border-bottom:none}
.ilbl{color:var(--muted)}.ival{font-variant-numeric:tabular-nums;font-weight:500}
.ival.ok{color:var(--green)}.ival.warn{color:var(--yellow)}.ival.bad{color:var(--red)}

/* Bar */
.bar-wrap{margin-top:10px}
.bar-lbl{display:flex;justify-content:space-between;font-size:.7rem;color:var(--muted);margin-bottom:4px}
.bar-track{height:5px;background:var(--surface2);border-radius:3px;overflow:hidden}
.bar-fill{height:100%;border-radius:3px;transition:width .35s ease;
  background:linear-gradient(90deg,var(--accent),var(--accent2))}
.bar-fill.warn{background:linear-gradient(90deg,var(--yellow),var(--orange))}
.bar-fill.bad{background:var(--red)}

/* Big stat */
.bigstat{display:flex;flex-direction:column;align-items:center;justify-content:center;padding:10px 0;gap:2px}
.bigstat-val{font-size:2.4rem;font-weight:700;font-variant-numeric:tabular-nums;line-height:1}
.bigstat-unit{font-size:.75rem;color:var(--muted)}

/* Axis bars */
.axis{margin-bottom:9px}.axis:last-child{margin-bottom:0}
.axis-hdr{display:flex;justify-content:space-between;margin-bottom:3px}
.axis-lbl{font-size:.72rem;font-weight:600;color:var(--muted)}
.axis-val{font-size:.72rem;font-variant-numeric:tabular-nums;font-weight:600}
.axis-track{height:5px;background:var(--surface2);border-radius:3px;position:relative;overflow:hidden}
.axis-track::after{content:'';position:absolute;left:50%;top:0;width:1px;height:100%;
  background:var(--border);transform:translateX(-50%)}
.axis-fill{height:100%;position:absolute;top:0;border-radius:3px;transition:left .12s,width .12s}

/* Hall */
.hall-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.hall-tile{background:var(--surface2);border-radius:8px;padding:12px;text-align:center}
.hall-name{font-size:.62rem;text-transform:uppercase;letter-spacing:.07em;color:var(--muted);margin-bottom:3px}
.hall-cnt{font-size:1.6rem;font-weight:700;font-variant-numeric:tabular-nums;color:var(--accent2);line-height:1}
.hall-spd{font-size:.68rem;color:var(--muted);margin-top:2px}

/* Buttons */
.btn-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.btn-tile{border:2px solid var(--border);border-radius:10px;
  padding:14px 8px;text-align:center;transition:all .12s}
.btn-tile.on{background:rgba(59,130,246,.12);border-color:var(--accent);
  box-shadow:0 0 10px rgba(59,130,246,.2)}
.btn-name{font-size:.62rem;text-transform:uppercase;letter-spacing:.07em;color:var(--muted);margin-bottom:5px}
.btn-tile.on .btn-name{color:var(--accent)}
.btn-ico{font-size:1.3rem;line-height:1}

/* Radar */
.radar-wrap{display:flex;justify-content:center;padding:4px 0}

/* Scan button */
.scan-btn{
  display:inline-flex;align-items:center;gap:6px;padding:6px 14px;
  background:linear-gradient(135deg,var(--accent),var(--accent2));
  border:none;border-radius:7px;color:#fff;font-size:.78rem;font-weight:600;
  cursor:pointer;transition:opacity .15s}
.scan-btn:hover{opacity:.85}.scan-btn:disabled{opacity:.4;cursor:not-allowed}
.scan-btn.spin svg{animation:spin 1s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}

/* Network/BLE table */
.tbl{width:100%;border-collapse:collapse;font-size:.82rem}
.tbl th{font-size:.65rem;text-transform:uppercase;letter-spacing:.07em;color:var(--muted);
  text-align:left;padding:5px 8px;border-bottom:1px solid var(--border)}
.tbl td{padding:6px 8px;border-bottom:1px solid var(--border)}
.tbl tr:last-child td{border-bottom:none}
.tbl tr:hover td{background:var(--surface2)}
.rssi-bar{display:inline-block;height:7px;border-radius:4px;vertical-align:middle;margin-right:6px}
.sec-badge{display:inline-block;padding:1px 6px;border-radius:4px;font-size:.65rem;font-weight:600}
.sec-badge.open{background:rgba(239,68,68,.15);color:var(--red)}
.sec-badge.wpa{background:rgba(34,197,94,.12);color:var(--green)}
.empty-state{text-align:center;padding:32px;color:var(--muted);font-size:.82rem}

/* footer */
footer{text-align:center;padding:14px;font-size:.68rem;color:var(--dim)}

/* Range sliders — dark-themed */
input[type=range]{
  -webkit-appearance:none;appearance:none;
  width:100%;height:5px;border-radius:3px;outline:none;cursor:pointer;
  background:var(--surface2);border:1px solid var(--border)}
input[type=range]::-webkit-slider-thumb{
  -webkit-appearance:none;appearance:none;
  width:16px;height:16px;border-radius:50%;
  background:linear-gradient(135deg,var(--accent),var(--accent2));
  border:2px solid var(--surface);box-shadow:0 0 6px rgba(59,130,246,.4);cursor:pointer}
input[type=range]::-moz-range-thumb{
  width:14px;height:14px;border-radius:50%;border:2px solid var(--surface);
  background:linear-gradient(135deg,var(--accent),var(--accent2));
  box-shadow:0 0 6px rgba(59,130,246,.4);cursor:pointer}
input[type=range]::-webkit-slider-runnable-track{
  background:var(--surface2);border-radius:3px;height:5px}
input[type=range]::-moz-range-track{
  background:var(--surface2);border-radius:3px;height:5px}
input[type=range]:focus{outline:none}
input[type=range]:focus::-webkit-slider-thumb{
  box-shadow:0 0 0 3px rgba(59,130,246,.25)}

/* Code / inline monospace */
code{background:var(--surface2);color:var(--accent2);padding:1px 5px;
  border-radius:4px;font-size:.82em;border:1px solid var(--border)}

/* Scrollbar */
::-webkit-scrollbar{width:6px;height:6px}
::-webkit-scrollbar-track{background:var(--bg)}
::-webkit-scrollbar-thumb{background:var(--border);border-radius:3px}
::-webkit-scrollbar-thumb:hover{background:var(--dim)}

/* Selection */
::selection{background:rgba(59,130,246,.25);color:var(--text)}
</style>
</head>
<body>

<header>
  <div class="logo">
    <div class="logo-icon">
      <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
        <rect x="3" y="11" width="18" height="10" rx="2"/>
        <circle cx="12" cy="5" r="2"/><line x1="12" y1="7" x2="12" y2="11"/>
        <line x1="7" y1="16" x2="7" y2="16" stroke-width="3"/>
        <line x1="12" y1="16" x2="12" y2="16" stroke-width="3"/>
        <line x1="17" y1="16" x2="17" y2="16" stroke-width="3"/>
      </svg>
    </div>
    <div>
      <div class="logo-title">RoboController</div>
      <div class="logo-sub">ESP32-S3 &bull; 192.168.4.1</div>
    </div>
  </div>
  <div class="hdr-r">
    <span id="uptime">--:--:--</span>
    <div class="pill"><div class="dot" id="dot"></div><span id="conn">Connecting</span></div>
  </div>
</header>

<div class="tabs">
  <div class="tab active" data-tab="sensors">Sensors</div>
  <div class="tab" data-tab="wifi">WiFi Scanner</div>
  <div class="tab" data-tab="ble">BLE Radar</div>
  <div class="tab" data-tab="ctrl">&#9654; Control</div>
</div>

<!-- â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• SENSORS PAGE â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• -->
<div class="page active" id="pg-sensors">
<main>

  <div class="card c4">
    <div class="card-hdr">
      <span class="card-title">System</span>
      <svg class="card-ico" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
        <rect x="2" y="3" width="20" height="14" rx="2"/><line x1="8" y1="21" x2="16" y2="21"/><line x1="12" y1="17" x2="12" y2="21"/>
      </svg>
    </div>
    <div class="irow"><span class="ilbl">SSID</span><span class="ival" id="s-ssid">â€”</span></div>
    <div class="irow"><span class="ilbl">IP</span><span class="ival" id="s-ip">â€”</span></div>
    <div class="irow"><span class="ilbl">Free Heap</span><span class="ival" id="s-heap">â€”</span></div>
    <div class="irow"><span class="ilbl">IMU</span><span class="ival" id="s-imu">â€”</span></div>
    <div class="bar-wrap">
      <div class="bar-lbl"><span>Heap used</span><span id="s-heap-pct">â€”</span></div>
      <div class="bar-track"><div class="bar-fill" id="heap-bar" style="width:0%"></div></div>
    </div>
  </div>

  <div class="card c2">
    <div class="card-hdr">
      <span class="card-title">IMU Temp</span>
      <svg class="card-ico" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
        <path d="M14 14.76V3.5a2.5 2.5 0 0 0-5 0v11.26a4.5 4.5 0 1 0 5 0z"/>
      </svg>
    </div>
    <div class="bigstat">
      <div class="bigstat-val" id="s-temp">â€”</div>
      <div class="bigstat-unit">degrees C</div>
    </div>
  </div>

  <div class="card c3">
    <div class="card-hdr">
      <span class="card-title">Buttons</span>
      <svg class="card-ico" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
        <circle cx="12" cy="12" r="9"/><circle cx="12" cy="12" r="3"/>
      </svg>
    </div>
    <div class="btn-grid">
      <div class="btn-tile" id="btn1"><div class="btn-name">BTN1 Cycle</div><div class="btn-ico">&#8679;</div></div>
      <div class="btn-tile" id="btn2"><div class="btn-name">BTN2 Select</div><div class="btn-ico">&#10003;</div></div>
    </div>
  </div>

  <div class="card c3">
    <div class="card-hdr">
      <span class="card-title">Hall Sensors</span>
      <svg class="card-ico" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
        <circle cx="12" cy="12" r="3"/>
        <path d="M12 1v4M12 19v4M4.22 4.22l2.83 2.83M16.95 16.95l2.83 2.83M1 12h4M19 12h4M4.22 19.78l2.83-2.83M16.95 7.05l2.83-2.83"/>
      </svg>
    </div>
    <div class="hall-grid">
      <div class="hall-tile"><div class="hall-name">Sensor 1</div><div class="hall-cnt" id="h1c">0</div><div class="hall-spd" id="h1p">0.0 p/s</div></div>
      <div class="hall-tile"><div class="hall-name">Sensor 2</div><div class="hall-cnt" id="h2c">0</div><div class="hall-spd" id="h2p">0.0 p/s</div></div>
    </div>
  </div>

  <div class="card c5">
    <div class="card-hdr">
      <span class="card-title">Proximity Radar</span>
      <svg class="card-ico" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
        <circle cx="12" cy="12" r="10"/><circle cx="12" cy="12" r="6"/><circle cx="12" cy="12" r="2"/>
      </svg>
    </div>
    <div class="radar-wrap">
      <svg width="210" height="210" viewBox="-105 -105 210 210" style="overflow:visible">
        <circle r="90" fill="none" stroke="#1c2230" stroke-width="1"/>
        <circle r="60" fill="none" stroke="#1c2230" stroke-width="1"/>
        <circle r="30" fill="none" stroke="#1c2230" stroke-width="1"/>
        <line x1="-90" y1="0" x2="90" y2="0" stroke="#1c2230" stroke-width="1"/>
        <line x1="0" y1="-90" x2="0" y2="90" stroke="#1c2230" stroke-width="1"/>
        <text x="32" y="-3" font-size="7" fill="#334155" font-family="sans-serif">30</text>
        <text x="62" y="-3" font-size="7" fill="#334155" font-family="sans-serif">60</text>
        <text x="92" y="-3" font-size="7" fill="#334155" font-family="sans-serif">90cm</text>
        <text x="0" y="-96" text-anchor="middle" font-size="9" fill="#64748b" font-family="sans-serif">FRONT</text>
        <text x="0" y="105" text-anchor="middle" font-size="9" fill="#64748b" font-family="sans-serif">BACK</text>
        <text x="-101" y="4" text-anchor="middle" font-size="9" fill="#64748b" font-family="sans-serif">L</text>
        <text x="101" y="4" text-anchor="middle" font-size="9" fill="#64748b" font-family="sans-serif">R</text>
        <line id="u0" x1="0" y1="0" x2="0" y2="-30" stroke="#3b82f6" stroke-width="4" stroke-linecap="round"/>
        <line id="u1" x1="0" y1="0" x2="30" y2="0" stroke="#3b82f6" stroke-width="4" stroke-linecap="round"/>
        <line id="u2" x1="0" y1="0" x2="0" y2="30" stroke="#3b82f6" stroke-width="4" stroke-linecap="round"/>
        <line id="u3" x1="0" y1="0" x2="-30" y2="0" stroke="#3b82f6" stroke-width="4" stroke-linecap="round"/>
        <rect x="-11" y="-15" width="22" height="30" rx="4" fill="#1c2230" stroke="#3b82f6" stroke-width="1.5"/>
        <rect x="-5" y="-19" width="10" height="5" rx="2" fill="#3b82f6" opacity=".8"/>
        <text id="v0" x="0" y="-74" text-anchor="middle" font-size="11" font-weight="600" fill="#e2e8f0" font-family="sans-serif"></text>
        <text id="v1" x="74" y="4" text-anchor="middle" font-size="11" font-weight="600" fill="#e2e8f0" font-family="sans-serif"></text>
        <text id="v2" x="0" y="80" text-anchor="middle" font-size="11" font-weight="600" fill="#e2e8f0" font-family="sans-serif"></text>
        <text id="v3" x="-74" y="4" text-anchor="middle" font-size="11" font-weight="600" fill="#e2e8f0" font-family="sans-serif"></text>
      </svg>
    </div>
  </div>

  <div class="card c7">
    <div class="card-hdr">
      <span class="card-title">Accelerometer &mdash; m/s&sup2;</span>
      <svg class="card-ico" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
        <path d="M12 2L2 7l10 5 10-5-10-5z"/><path d="M2 17l10 5 10-5"/><path d="M2 12l10 5 10-5"/>
      </svg>
    </div>
    <div id="abars"></div>
  </div>

  <div class="card c6">
    <div class="card-hdr">
      <span class="card-title">Gyroscope &mdash; &deg;/s</span>
      <svg class="card-ico" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
        <path d="M21.5 2v6h-6"/><path d="M2.5 12A10 10 0 0 1 19.8 7.2"/>
        <path d="M2.5 22v-6h6"/><path d="M21.5 12A10 10 0 0 1 4.2 16.8"/>
      </svg>
    </div>
    <div id="gbars"></div>
  </div>

</main>
</div><!-- /pg-sensors -->

<!-- â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• WIFI SCANNER PAGE â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• -->
<div class="page" id="pg-wifi">
<main>
  <div class="card c12">
    <div class="card-hdr">
      <span class="card-title">WiFi Networks</span>
      <button class="scan-btn" id="wifi-scan-btn" onclick="doWifiScan()">
        <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round">
          <path d="M21.5 2v6h-6"/><path d="M2.5 12A10 10 0 0 1 19.8 7.2"/>
        </svg>
        Scan
      </button>
    </div>
    <div id="wifi-table"><div class="empty-state">Press Scan to discover networks</div></div>
  </div>
</main>
</div>

<!-- â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• BLE PAGE â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• -->
<div class="page" id="pg-ble">
<main>
  <div class="card c12">
    <div class="card-hdr">
      <span class="card-title">BLE Devices</span>
      <button class="scan-btn" id="ble-scan-btn" onclick="doBLEScan()">
        <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round">
          <path d="M21.5 2v6h-6"/><path d="M2.5 12A10 10 0 0 1 19.8 7.2"/>
        </svg>
        Scan
      </button>
    </div>
    <p style="font-size:.72rem;color:var(--muted);margin-bottom:12px">Scans for ~4 seconds. Displays address, name, and signal strength.</p>
    <div id="ble-table"><div class="empty-state">Press Scan to search for BLE devices</div></div>
  </div>
</main>
</div>

<!-- ══════════════════════ CONTROL PAGE ══════════════════════ -->
<div class="page" id="pg-ctrl">
<main>
  <div class="card c6">
    <div class="card-hdr">
      <span class="card-title">Drive</span>
      <button class="scan-btn" onclick="eStop()" style="background:linear-gradient(135deg,#ef4444,#b91c1c);color:#fff;border:none;padding:4px 10px">&#9632;&nbsp;STOP</button>
    </div>
    <p style="font-size:.72rem;color:var(--muted);margin-bottom:8px">Drag to drive &bull; Release to stop</p>
    <div style="display:flex;justify-content:center">
      <canvas id="joy" width="200" height="200" style="touch-action:none;cursor:crosshair"></canvas>
    </div>
    <div style="display:flex;justify-content:space-between;margin-top:8px;font-size:.75rem;font-variant-numeric:tabular-nums">
      <span>L&nbsp;<strong id="ml">0</strong></span>
      <span id="mm" style="color:var(--muted)">stopped</span>
      <span>R&nbsp;<strong id="mr">0</strong></span>
    </div>
  </div>
  <div class="card c6">
    <div class="card-hdr"><span class="card-title">Servos</span></div>
    <div style="margin-bottom:14px">
      <div style="display:flex;justify-content:space-between;font-size:.75rem;margin-bottom:4px">
        <span style="color:var(--muted)">Servo 1</span>
        <span id="sv1" style="font-weight:600">90&deg;</span>
      </div>
      <input type="range" min="0" max="180" value="90" id="si1" style="width:100%" oninput="sendServo(1,+this.value)">
    </div>
    <div style="margin-bottom:14px;opacity:.45;pointer-events:none" title="Servo 2 pin repurposed for potentiometer">
      <div style="display:flex;justify-content:space-between;font-size:.75rem;margin-bottom:4px">
        <span style="color:var(--muted)">Servo 2 <em style="font-size:.68rem">(N/A — pin repurposed)</em></span>
        <span id="sv2" style="font-weight:600">90&deg;</span>
      </div>
      <input type="range" min="0" max="180" value="90" id="si2" style="width:100%" disabled>
    </div>
    <div style="display:flex;gap:8px;flex-wrap:wrap">
      <button class="scan-btn" onclick="setPose(90,90)">Centre</button>
      <button class="scan-btn" onclick="setPose(0,0)">Min</button>
      <button class="scan-btn" onclick="setPose(180,180)">Max</button>
      <button class="scan-btn" onclick="setPose(45,135)">Cross</button>
    </div>
  </div>
  <div class="card c12">
    <div class="card-hdr"><span class="card-title">Command Log</span></div>
    <div id="clog" style="font-family:monospace;font-size:.7rem;color:var(--muted);height:80px;overflow-y:auto;background:var(--surface2);border-radius:6px;padding:6px"></div>
  </div>
  <div class="card c12">
    <div class="card-hdr"><span class="card-title">BLE GATT Info</span></div>
    <p style="font-size:.75rem;color:var(--muted);line-height:1.6">
      Connect with <strong>nRF Connect</strong> or similar &bull; Service <code style="font-size:.7rem">AB12</code><br>
      Sensor Notify <code style="font-size:.7rem">AA13</code> &bull;
      Motor Write <code style="font-size:.7rem">AA14</code> (format: <code style="font-size:.7rem">l:200,r:-150</code>)<br>
      Servo Write <code style="font-size:.7rem">AA15</code> (format: <code style="font-size:.7rem">1:90</code>)
    </p>
  </div>
</main>
</div>

<footer>RoboController &bull; ESP32-S3 &bull; WebSocket &bull; <span id="ts">â€”</span></footer>

<script>
const MAX_HEAP=327680, US_MAX=90, AR=20, GR=250;
function $(id){return document.getElementById(id)}
function set(id,t){const e=$(id);if(e)e.textContent=t}
function fmt(v,d){return typeof v==='number'?v.toFixed(d):'â€”'}
function clamp(v,a,b){return Math.max(a,Math.min(b,v))}

// â”€â”€ Tabs â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
document.querySelectorAll('.tab').forEach(t=>{
  t.addEventListener('click',()=>{
    document.querySelectorAll('.tab').forEach(x=>x.classList.remove('active'));
    document.querySelectorAll('.page').forEach(x=>x.classList.remove('active'));
    t.classList.add('active');
    $('pg-'+t.dataset.tab).classList.add('active');
  });
});

// â”€â”€ WebSocket â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
let ws, reconnTimer;
function wsConnect(){
  ws = new WebSocket('ws://'+location.host+'/ws');
  ws.onopen  = ()=>{ clearTimeout(reconnTimer); setStatus(true); };
  ws.onclose = ()=>{ setStatus(false); reconnTimer=setTimeout(wsConnect,2000); };
  ws.onerror = ()=>{ ws.close(); };
  ws.onmessage = e=>{ try{ onMsg(JSON.parse(e.data)); }catch(_){} };
}
function setStatus(ok){
  const d=$('dot'),c=$('conn');
  if(d) d.className='dot'+(ok?' live':' err');
  if(c) c.textContent=ok?'Live':'Reconnecting...';
}
wsConnect();

// â”€â”€ Message handler â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
function onMsg(d){
  if(d.t==='data') updateSensors(d);
}

function updateSensors(d){
  const s=d.uptime_ms/1000;
  set('uptime',[Math.floor(s/3600),Math.floor((s%3600)/60),Math.floor(s%60)]
    .map(n=>String(n).padStart(2,'0')).join(':'));

  const hp=(1-d.free_heap/MAX_HEAP)*100;
  set('s-ssid',d.ssid); set('s-ip',d.ip);
  set('s-heap',(d.free_heap/1024).toFixed(1)+' KB free');
  const imuEl=$('s-imu');
  if(imuEl){imuEl.textContent=d.imu_ok?'Found':'Not found';imuEl.className='ival '+(d.imu_ok?'ok':'bad')}
  set('s-heap-pct',hp.toFixed(1)+'%');
  const hb=$('heap-bar');
  if(hb){hb.style.width=hp.toFixed(1)+'%';hb.className='bar-fill'+(hp>80?' bad':hp>60?' warn':'')}

  set('s-temp',d.imu_ok?fmt(d.imu.temp_c,1):'â€”');

  const b1=$('btn1'),b2=$('btn2');
  if(b1)b1.className='btn-tile'+(d.btn1?' on':'');
  if(b2)b2.className='btn-tile'+(d.btn2?' on':'');

  set('h1c',d.hall[0].count); set('h1p',fmt(d.hall[0].pps,1)+' p/s');
  set('h2c',d.hall[1].count); set('h2p',fmt(d.hall[1].pps,1)+' p/s');

  updateRadar(d.us);

  if(d.imu_ok){
    axisBars('abars',[['X',d.imu.accel_x],['Y',d.imu.accel_y],['Z',d.imu.accel_z]],AR,'#3b82f6','#06b6d4');
    axisBars('gbars',[['X',d.imu.gyro_x],['Y',d.imu.gyro_y],['Z',d.imu.gyro_z]],GR,'#a855f7','#ec4899');
  }
  set('ts','Updated '+new Date().toLocaleTimeString());
}

// â”€â”€ Radar â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
const US_DIR=[
  {li:'u0',vi:'v0',dx:0,dy:-1},{li:'u1',vi:'v1',dx:1,dy:0},
  {li:'u2',vi:'v2',dx:0,dy:1},{li:'u3',vi:'v3',dx:-1,dy:0}
];
function distColor(cm){
  if(cm<0) return '#334155';
  if(cm<15) return '#ef4444';
  if(cm<30) return '#eab308';
  return '#3b82f6';
}
function updateRadar(us){
  US_DIR.forEach(({li,vi,dx,dy},i)=>{
    const cm=us[i],ok=cm>=0;
    const len=ok?clamp(cm/US_MAX,0,1)*90:16;
    const col=distColor(cm);
    const ln=$(li),tx=$(vi);
    if(ln){ln.setAttribute('x2',(dx*len).toFixed(1));ln.setAttribute('y2',(dy*len).toFixed(1));ln.setAttribute('stroke',col)}
    if(tx){tx.textContent=ok?cm.toFixed(1)+'cm':'---';tx.style.fill=col}
  });
}

// â”€â”€ Axis bars â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
function axisBars(cid,axes,range,cp,cn){
  const c=$(cid);if(!c)return;
  c.innerHTML=axes.map(([lbl,val])=>{
    const p=clamp(Math.abs(val)/range,0,1)*50;
    const pos=val>=0,col=pos?cp:cn,lft=pos?50:(50-p);
    return `<div class="axis">
      <div class="axis-hdr"><span class="axis-lbl">${lbl}</span><span class="axis-val">${fmt(val,3)}</span></div>
      <div class="axis-track"><div class="axis-fill" style="left:${lft}%;width:${p}%;background:${col}"></div></div>
    </div>`;
  }).join('');
}

// â”€â”€ WiFi Scanner â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
function rssiColor(r){
  if(r>-55) return '#22c55e';
  if(r>-70) return '#eab308';
  return '#ef4444';
}
async function doWifiScan(){
  const btn=$('wifi-scan-btn');
  btn.disabled=true; btn.classList.add('spin');
  $('wifi-table').innerHTML='<div class="empty-state">Scanning...</div>';
  try{
    const r=await fetch('/api/scan');
    const nets=await r.json();
    if(!nets.length){$('wifi-table').innerHTML='<div class="empty-state">No networks found</div>';return;}
    nets.sort((a,b)=>b.rssi-a.rssi);
    const rows=nets.map(n=>{
      const pct=clamp((n.rssi+100)/70*100,0,100);
      const col=rssiColor(n.rssi);
      const sec=n.enc?'<span class="sec-badge wpa">WPA</span>':'<span class="sec-badge open">OPEN</span>';
      const ssidDisplay=n.ssid?htmlEsc(n.ssid):'<em style="color:var(--muted)">(hidden)</em>';
      return `<tr>
        <td>${ssidDisplay}</td>
        <td style="white-space:nowrap">
          <span class="rssi-bar" style="width:${pct.toFixed(0)}px;background:${col}"></span>${n.rssi} dBm
        </td>
        <td>ch ${n.ch}</td>
        <td>${sec}</td>
        <td style="color:var(--muted);font-size:.72rem">${htmlEsc(n.bssid)}</td>
      </tr>`;
    }).join('');
    $('wifi-table').innerHTML=`<table class="tbl">
      <thead><tr><th>SSID</th><th>Signal</th><th>Channel</th><th>Security</th><th>BSSID</th></tr></thead>
      <tbody>${rows}</tbody></table>`;
  }catch(e){$('wifi-table').innerHTML='<div class="empty-state">Scan failed: '+e+'</div>'}
  finally{btn.disabled=false;btn.classList.remove('spin')}
}

// â”€â”€ BLE Scanner â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
async function doBLEScan(){
  const btn=$('ble-scan-btn');
  btn.disabled=true; btn.classList.add('spin');
  btn.querySelector('span')||btn.insertAdjacentHTML('beforeend','<span> Scanning...</span>');
  $('ble-table').innerHTML='<div class="empty-state">Scanning for BLE devices (~4s)...</div>';
  try{
    const r=await fetch('/api/ble');
    const devs=await r.json();
    if(!devs.length){$('ble-table').innerHTML='<div class="empty-state">No BLE devices found</div>';return;}
    devs.sort((a,b)=>b.rssi-a.rssi);
    const rows=devs.map(d=>{
      const pct=clamp((d.rssi+100)/70*100,0,100);
      const col=rssiColor(d.rssi);
      const nameDisplay=d.name?htmlEsc(d.name):'<em style="color:var(--muted)">(unnamed)</em>';
      return `<tr>
        <td>${nameDisplay}</td>
        <td style="white-space:nowrap">
          <span class="rssi-bar" style="width:${pct.toFixed(0)}px;background:${col}"></span>${d.rssi} dBm
        </td>
        <td style="color:var(--muted);font-size:.72rem">${htmlEsc(d.addr)}</td>
      </tr>`;
    }).join('');
    $('ble-table').innerHTML=`<table class="tbl">
      <thead><tr><th>Name</th><th>Signal</th><th>Address</th></tr></thead>
      <tbody>${rows}</tbody></table>`;
  }catch(e){$('ble-table').innerHTML='<div class="empty-state">Scan failed: '+e+'</div>'}
  finally{btn.disabled=false;btn.classList.remove('spin')}
}

// ── Drive Joystick ────────────────────────────────────────────────────────
const jcv=$('joy'),jctx=jcv?jcv.getContext('2d'):null;
const JW=200,JH=200,JR=78,KR=26,JCX=100,JCY=100;
let jx=0,jy=0,jdn=false,jLastL=0,jLastR=0;
function drawJoy(){
  if(!jctx)return;
  jctx.clearRect(0,0,JW,JH);
  jctx.beginPath();jctx.arc(JCX,JCY,JR,0,Math.PI*2);
  jctx.fillStyle='rgba(15,23,42,.9)';jctx.fill();
  jctx.strokeStyle='rgba(59,130,246,.25)';jctx.lineWidth=1.5;jctx.stroke();
  jctx.strokeStyle='rgba(100,116,139,.2)';jctx.lineWidth=1;
  jctx.beginPath();
  jctx.moveTo(JCX-JR,JCY);jctx.lineTo(JCX+JR,JCY);
  jctx.moveTo(JCX,JCY-JR);jctx.lineTo(JCX,JCY+JR);
  jctx.stroke();
  const kx=JCX+jx,ky=JCY+jy;
  const g=jctx.createRadialGradient(kx-3,ky-3,2,kx,ky,KR);
  g.addColorStop(0,'rgba(147,210,255,.9)');g.addColorStop(1,'rgba(37,99,235,.85)');
  jctx.beginPath();jctx.arc(kx,ky,KR,0,Math.PI*2);
  jctx.fillStyle=g;jctx.fill();
  jctx.strokeStyle='rgba(147,210,255,.4)';jctx.lineWidth=1.5;jctx.stroke();
}
function jyPos(ex,ey){
  const rc=jcv.getBoundingClientRect();
  const dx=ex-rc.left-JCX,dy=ey-rc.top-JCY;
  const d=Math.sqrt(dx*dx+dy*dy),mx=JR-KR;
  if(d>mx){jx=dx/d*mx;jy=dy/d*mx;}else{jx=dx;jy=dy;}
  drawJoy();
  const thr=-jy/JR,trn=jx/JR;
  let l=Math.round((thr+trn)*255),r=Math.round((thr-trn)*255);
  l=Math.max(-255,Math.min(255,l));r=Math.max(-255,Math.min(255,r));
  if(l!==jLastL||r!==jLastR){jLastL=l;jLastR=r;sendDrive(l,r);}
}
function sendDrive(l,r){
  if(ws&&ws.readyState===1)ws.send(JSON.stringify({t:'drive',l,r}));
  set('ml',l);set('mr',r);
  const s=Math.max(Math.abs(l),Math.abs(r));
  set('mm',s<5?'stopped':l>0&&r>0?'forward':l<0&&r<0?'reverse':l>r?'turn R':'turn L');
  clog('drive l='+l+' r='+r);
}
function eStop(){
  jx=0;jy=0;jdn=false;drawJoy();
  if(ws&&ws.readyState===1)ws.send(JSON.stringify({t:'stop'}));
  jLastL=jLastR=999;
  set('ml',0);set('mr',0);set('mm','stopped');clog('E-STOP');
}
if(jcv){
  jcv.addEventListener('mousedown',e=>{jdn=true;jyPos(e.clientX,e.clientY)});
  jcv.addEventListener('mousemove',e=>{if(jdn)jyPos(e.clientX,e.clientY)});
  document.addEventListener('mouseup',()=>{if(jdn){jdn=false;eStop();}});
  jcv.addEventListener('touchstart',e=>{e.preventDefault();jdn=true;jyPos(e.touches[0].clientX,e.touches[0].clientY);},{passive:false});
  jcv.addEventListener('touchmove',e=>{e.preventDefault();if(jdn)jyPos(e.touches[0].clientX,e.touches[0].clientY);},{passive:false});
  jcv.addEventListener('touchend',e=>{e.preventDefault();jdn=false;eStop();},{passive:false});
  drawJoy();
}

// ── Servo sliders ─────────────────────────────────────────────────────────
function sendServo(id,angle){
  set('sv'+id,angle+'\u00b0');
  if(ws&&ws.readyState===1)ws.send(JSON.stringify({t:'servo',id,angle}));
  clog('servo '+id+'='+angle+'\u00b0');
}
function setPose(a1,a2){
  const s1=$('si1'),s2=$('si2');
  if(s1){s1.value=a1;sendServo(1,a1);}
  if(s2){s2.value=a2;sendServo(2,a2);}
}

// ── Command log ───────────────────────────────────────────────────────────
const CMAX=30;let clines=[];
function clog(msg){
  const t=new Date().toLocaleTimeString('en',{hour12:false});
  clines.push(t+' \u203a '+msg);if(clines.length>CMAX)clines.shift();
  const el=$('clog');
  if(el){el.innerHTML=clines.map(l=>'<div>'+l+'</div>').join('');el.scrollTop=el.scrollHeight;}
}
</script>
</body>
</html>
)rawhtml";

// Parse and execute an inbound WebSocket command JSON string
static void _handleWsCmd(const char* msg) {
    // {"t":"stop"}
    if (strstr(msg, "\"t\":\"stop\"")) {
        motor_coast(MotorId::MOTOR1);
        motor_coast(MotorId::MOTOR2);
        Serial.println("[ws] cmd: stop");
        return;
    }
    // {"t":"drive","l":N,"r":N}
    if (strstr(msg, "\"t\":\"drive\"")) {
        int l = 0, r = 0;
        const char* lp = strstr(msg, "\"l\":");
        const char* rp = strstr(msg, "\"r\":");
        if (lp) sscanf(lp + 4, "%d", &l);
        if (rp) sscanf(rp + 4, "%d", &r);
        l = constrain(l, -255, 255);
        r = constrain(r, -255, 255);
        motor_set(MotorId::MOTOR1, l);
        motor_set(MotorId::MOTOR2, r);
        return;
    }
    // {"t":"servo","id":N,"angle":N}
    if (strstr(msg, "\"t\":\"servo\"")) {
        int id = 1, angle = 90;
        const char* idp  = strstr(msg, "\"id\":");
        const char* angp = strstr(msg, "\"angle\":");
        if (idp)  sscanf(idp  + 5, "%d", &id);
        if (angp) sscanf(angp + 8, "%d", &angle);
        angle = angle < 0 ? 0 : angle > 180 ? 180 : angle;
        servo_set_angle(id == 2 ? ServoId::SERVO2 : ServoId::SERVO1, angle);
        return;
    }
}

// ---------------------------------------------------------------------------
// WebSocket event handler
// ---------------------------------------------------------------------------
static void _onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client,
                        AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[ws] client #%u connected\n", client->id());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[ws] client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len
                && info->opcode == WS_TEXT) {
            char msg[128];
            size_t copy = len < 127 ? len : 127;
            memcpy(msg, data, copy);
            msg[copy] = '\0';
            _handleWsCmd(msg);
        }
    }
}

// ---------------------------------------------------------------------------
// Route: GET /api/scan â€” WiFi network scan
// ---------------------------------------------------------------------------
static void _handle_scan(AsyncWebServerRequest* req) {
    int n = WiFi.scanNetworks(false, true); // blocking, include hidden
    String json = "[";
    for (int i = 0; i < n; i++) {
        if (i) json += ",";
        String ssid = WiFi.SSID(i);
        ssid.replace("\"", "\\\"");
        char entry[256];
        snprintf(entry, sizeof(entry),
            "{\"ssid\":\"%s\",\"rssi\":%d,\"ch\":%d,\"enc\":%s,\"bssid\":\"%s\"}",
            ssid.c_str(), WiFi.RSSI(i), WiFi.channel(i),
            (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "false" : "true",
            WiFi.BSSIDstr(i).c_str());
        json += entry;
    }
    json += "]";
    WiFi.scanDelete();
    req->send(200, "application/json", json);
}

// ---------------------------------------------------------------------------
// Route: GET /api/ble â€” BLE device scan (blocks for WIFI_BLE_SCAN_SEC s)
// ---------------------------------------------------------------------------
static void _handle_ble(AsyncWebServerRequest* req) {
    if (_bleBusy) { req->send(503, "application/json", "{\"error\":\"scan in progress\"}"); return; }
    _bleBusy = true;
    _bleResults.clear();

    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&_bleCb, true);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);
    scan->start(WIFI_BLE_SCAN_SEC, false);

    String json = "[";
    for (size_t i = 0; i < _bleResults.size(); i++) {
        if (i) json += ",";
        String name = _bleResults[i].name;
        name.replace("\"", "\\\"");
        char entry[192];
        snprintf(entry, sizeof(entry),
            "{\"addr\":\"%s\",\"name\":\"%s\",\"rssi\":%d}",
            _bleResults[i].addr.c_str(), name.c_str(), _bleResults[i].rssi);
        json += entry;
    }
    json += "]";
    _bleResults.clear();
    _bleBusy = false;
    req->send(200, "application/json", json);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void wifi_server_init() {
    // Soft AP
    WiFi.mode(WIFI_AP_STA);  // STA needed for WiFi.scanNetworks()
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL);
    Serial.printf("[wifi] AP started â€” SSID: %s  IP: %s\n",
                  WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());

    // BLE init
    NimBLEDevice::init("RoboController");
    Serial.println("[ble] NimBLE initialised");

    // BLE GATT server — service 0xAB12
    //   AA13: sensor data  (Read + Notify)
    //   AA14: motor write  (Write) — format "l:200,r:-150"
    //   AA15: servo write  (Write) — format "1:90"
    _gattServer = NimBLEDevice::createServer();
    {
        NimBLEService* svc = _gattServer->createService("AB12");
        _sensorChar = svc->createCharacteristic("AA13",
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
        _motorChar = svc->createCharacteristic("AA14",
            NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
        _motorChar->setCallbacks(&_motorBleCb);
        _servoChar = svc->createCharacteristic("AA15",
            NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
        _servoChar->setCallbacks(&_servoBleCb);
        svc->start();
    }
    {
        NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
        adv->addServiceUUID("AB12");
        adv->setScanResponse(true);
        adv->start();
    }
    Serial.println("[ble] GATT server advertising (sensor notify + motor/servo write)");

    // WebSocket
    _ws.onEvent(_onWsEvent);
    _server.addHandler(&_ws);

    // Routes
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* req){
        req->send(200, "text/html", _HTML);
    });
    _server.on("/api/scan", HTTP_GET, _handle_scan);
    _server.on("/api/ble",  HTTP_GET, _handle_ble);
    _server.onNotFound([](AsyncWebServerRequest* req){
        req->send(404, "text/plain", "Not found");
    });

    _server.begin();
    Serial.println("[wifi] Async HTTP + WebSocket server started on port 80");
}

void wifi_server_update() {
    // Clean up disconnected WebSocket clients
    _ws.cleanupClients();

    // Refresh ultrasonic cache (blocking read, limited to once per US_CACHE_MS)
    _refreshUsCache();

    // Push sensor data to all connected WS clients at WIFI_WS_PUSH_MS interval
    if (millis() - _lastPush >= WIFI_WS_PUSH_MS && _ws.count() > 0) {
        _lastPush = millis();
        char buf[1024];
        _buildJson(buf, sizeof(buf));
        _ws.textAll(buf);
    }

    // Push compact sensor JSON to any connected BLE central every 500 ms
    if (_gattServer && _sensorChar &&
            _gattServer->getConnectedCount() > 0 &&
            millis() - _lastBlePush >= 500) {
        _lastBlePush = millis();
        char bleBuf[256];
        _buildBleJson(bleBuf, sizeof(bleBuf));
        _sensorChar->setValue((uint8_t*)bleBuf, strlen(bleBuf));
        _sensorChar->notify();
    }
}

// ---------------------------------------------------------------------------
// Local device-side scan helpers
// ---------------------------------------------------------------------------
int wifi_ap_client_count() {
    return (int)WiFi.softAPgetStationNum();
}

int wifi_scan_networks(WifiNetInfo* out, int maxn) {
    int n = WiFi.scanNetworks(false, true);
    if (n < 0) n = 0;
    if (n > maxn) n = maxn;
    for (int i = 0; i < n; i++) {
        strncpy(out[i].ssid, WiFi.SSID(i).c_str(), 32);
        out[i].ssid[32] = '\0';
        out[i].rssi = WiFi.RSSI(i);
        out[i].ch   = WiFi.channel(i);
        out[i].enc  = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    WiFi.scanDelete();
    return n;
}

// Reuse the existing BLE callback/results from the HTTP handler
int ble_scan_devices(BleDevInfo* out, int maxn, int secs) {
    if (_bleBusy) return 0;
    _bleBusy = true;
    _bleResults.clear();

    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&_bleCb, true);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);
    scan->start(secs, false);

    int n = (int)_bleResults.size();
    if (n > maxn) n = maxn;
    for (int i = 0; i < n; i++) {
        strncpy(out[i].addr, _bleResults[i].addr.c_str(), 17);
        out[i].addr[17] = '\0';
        strncpy(out[i].name, _bleResults[i].name.c_str(), 31);
        out[i].name[31] = '\0';
        out[i].rssi = _bleResults[i].rssi;
    }
    _bleResults.clear();
    _bleBusy = false;
    return n;
}
