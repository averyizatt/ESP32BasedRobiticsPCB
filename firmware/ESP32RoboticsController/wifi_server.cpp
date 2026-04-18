#include "wifi_server.h"
#include "imu.h"
#include "ultrasonics.h"
#include "hall_sensors.h"
#include "buttons.h"
#include "motors.h"
#include "config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static WebServer _server(80);

// ---------------------------------------------------------------------------
// HTML dashboard (served once; JS polls /api/data for live updates)
// ---------------------------------------------------------------------------
static const char _HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>RoboController Dashboard</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:'Segoe UI',Arial,sans-serif;background:#0d0d0d;color:#e0e0e0;padding:16px}
  h1{text-align:center;color:#00c8ff;font-size:1.6rem;margin-bottom:4px}
  .subtitle{text-align:center;color:#666;font-size:.8rem;margin-bottom:18px}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:14px}
  .card{background:#1a1a1a;border:1px solid #2a2a2a;border-radius:10px;padding:14px}
  .card h2{font-size:.85rem;text-transform:uppercase;letter-spacing:.1em;color:#00c8ff;margin-bottom:10px;border-bottom:1px solid #2a2a2a;padding-bottom:6px}
  .row{display:flex;justify-content:space-between;align-items:center;padding:3px 0;font-size:.9rem}
  .label{color:#999}
  .value{color:#fff;font-variant-numeric:tabular-nums}
  .value.ok{color:#4ade80}
  .value.warn{color:#facc15}
  .value.bad{color:#f87171}
  .badge{display:inline-block;padding:2px 8px;border-radius:4px;font-size:.75rem;font-weight:bold}
  .badge.pressed{background:#00c8ff;color:#000}
  .badge.released{background:#333;color:#888}
  #status{text-align:center;font-size:.75rem;color:#555;margin-top:14px}
</style>
</head>
<body>
<h1>RoboController Dashboard</h1>
<div class="subtitle">ESP32-S3 Robotics Controller &mdash; Live Sensor Data</div>
<div class="grid" id="grid">
  <!-- cards injected by JS -->
</div>
<div id="status">Connecting...</div>

<script>
const POLL_MS = 800;

function colorDist(v){
  if(v<0) return 'bad';
  if(v<15) return 'warn';
  return 'ok';
}
function colorBool(v){ return v ? 'ok' : 'bad'; }

function card(title, rows){
  return `<div class="card"><h2>${title}</h2>${rows.map(r=>
    `<div class="row"><span class="label">${r[0]}</span><span class="value ${r[2]||''}">${r[1]}</span></div>`
  ).join('')}</div>`;
}

function badge(pressed){
  return `<span class="badge ${pressed?'pressed':'released'}">${pressed?'PRESSED':'RELEASED'}</span>`;
}

async function poll(){
  try{
    const res = await fetch('/api/data');
    if(!res.ok) throw new Error(res.status);
    const d = await res.json();

    const uptimeSec = d.uptime_ms / 1000;
    const h = String(Math.floor(uptimeSec/3600)).padStart(2,'0');
    const m = String(Math.floor((uptimeSec%3600)/60)).padStart(2,'0');
    const s = String(Math.floor(uptimeSec%60)).padStart(2,'0');

    const heapKB = (d.free_heap / 1024).toFixed(1);

    let html = '';

    html += card('Device Info',[
      ['SSID', d.ssid],
      ['IP Address', d.ip],
      ['Uptime', `${h}:${m}:${s}`],
      ['Free Heap', `${heapKB} KB`],
      ['IMU Present', d.imu_ok ? 'Yes' : 'No', colorBool(d.imu_ok)],
    ]);

    if(d.imu_ok){
      html += card('IMU — Accelerometer (m/s²)',[
        ['X', d.imu.accel_x.toFixed(3)],
        ['Y', d.imu.accel_y.toFixed(3)],
        ['Z', d.imu.accel_z.toFixed(3)],
        ['Temperature', `${d.imu.temp_c.toFixed(1)} °C`],
      ]);
      html += card('IMU — Gyroscope (°/s)',[
        ['X', d.imu.gyro_x.toFixed(2)],
        ['Y', d.imu.gyro_y.toFixed(2)],
        ['Z', d.imu.gyro_z.toFixed(2)],
      ]);
    }

    html += card('Ultrasonics (cm)',[
      ['Front (US1)', d.us[0]<0?'---':d.us[0].toFixed(1)+' cm', colorDist(d.us[0])],
      ['Right (US2)', d.us[1]<0?'---':d.us[1].toFixed(1)+' cm', colorDist(d.us[1])],
      ['Back  (US3)', d.us[2]<0?'---':d.us[2].toFixed(1)+' cm', colorDist(d.us[2])],
      ['Left  (US4)', d.us[3]<0?'---':d.us[3].toFixed(1)+' cm', colorDist(d.us[3])],
    ]);

    html += card('Hall Effect Sensors',[
      ['Sensor 1 count', d.hall[0].count],
      ['Sensor 1 speed', `${d.hall[0].pps.toFixed(1)} p/s`],
      ['Sensor 2 count', d.hall[1].count],
      ['Sensor 2 speed', `${d.hall[1].pps.toFixed(1)} p/s`],
    ]);

    const btn1 = `<span class="badge ${d.btn1?'pressed':'released'}">${d.btn1?'PRESSED':'RELEASED'}</span>`;
    const btn2 = `<span class="badge ${d.btn2?'pressed':'released'}">${d.btn2?'PRESSED':'RELEASED'}</span>`;
    html += `<div class="card"><h2>Buttons</h2>
      <div class="row"><span class="label">BTN1 (Cycle)</span>${btn1}</div>
      <div class="row"><span class="label">BTN2 (Select)</span>${btn2}</div>
    </div>`;

    document.getElementById('grid').innerHTML = html;
    document.getElementById('status').textContent =
      'Last update: ' + new Date().toLocaleTimeString();
  } catch(e){
    document.getElementById('status').textContent = 'Error: ' + e;
  }
}

poll();
setInterval(poll, POLL_MS);
</script>
</body>
</html>
)rawhtml";

// ---------------------------------------------------------------------------
// Route handlers
// ---------------------------------------------------------------------------
static void _handle_root() {
    _server.sendHeader("Cache-Control", "no-cache");
    _server.send_P(200, "text/html", _HTML);
}

static void _handle_data() {
    // Gather sensor data
    ImuData imu = {};
    bool imu_ok = imu_read(imu);

    float us[4];
    ultrasonics_read_all_cm(us);

    char buf[1024];
    int n = snprintf(buf, sizeof(buf),
        "{"
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
    (void)n;

    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.sendHeader("Cache-Control", "no-cache");
    _server.send(200, "application/json", buf);
}

static void _handle_not_found() {
    _server.send(404, "text/plain", "Not found");
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void wifi_server_init() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL);

    Serial.printf("[wifi] AP started — SSID: %s  IP: %s\n",
                  WIFI_AP_SSID,
                  WiFi.softAPIP().toString().c_str());

    _server.on("/",         HTTP_GET, _handle_root);
    _server.on("/api/data", HTTP_GET, _handle_data);
    _server.onNotFound(_handle_not_found);
    _server.begin();

    Serial.println("[wifi] HTTP server started on port 80");
}

void wifi_server_update() {
    _server.handleClient();
}
