# ESP32-S3 Robotics Controller PCB

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-red?logo=espressif)](https://www.espressif.com/en/products/socs/esp32-s3)
[![KiCad](https://img.shields.io/badge/KiCad-7%2B-blue?logo=kicad)](https://www.kicad.org/)
[![Arduino](https://img.shields.io/badge/Arduino-Compatible-teal?logo=arduino)](https://www.arduino.cc/)
[![PCB](https://img.shields.io/badge/PCB-4--Layer-green)](hardware/)

**A professional-grade, 4-layer ESP32-S3 robotics controller PCB that consolidates motor control, multi-axis sensing, and user interaction into a single, scalable platform.**

</div>

---

## Table of Contents

1. [Board Gallery](#board-gallery)
2. [System Architecture](#system-architecture)
3. [Feature Overview](#feature-overview)
4. [PCB Design](#pcb-design)
   - [Layer Stackup](#layer-stackup)
   - [Signal Integrity](#signal-integrity)
   - [Key Components (BOM Summary)](#key-components-bom-summary)
5. [GPIO Pin Reference](#gpio-pin-reference)
6. [Peripheral Details](#peripheral-details)
   - [Motor Control — DRV8871](#motor-control--drv8871)
   - [Servo Outputs](#servo-outputs)
   - [Ultrasonic Sensors — HC-SR04](#ultrasonic-sensors--hc-sr04)
   - [Hall Effect Sensors](#hall-effect-sensors)
   - [IMU — I2C](#imu--i2c)
   - [SPI Display](#spi-display)
   - [Push Buttons](#push-buttons)
   - [Expansion Header](#expansion-header)
7. [Firmware](#firmware)
   - [Quick Start](#quick-start)
   - [Library Dependencies](#library-dependencies)
   - [Firmware Architecture](#firmware-architecture)
   - [PWM Configuration](#pwm-configuration)
   - [WiFi Access Point & Web Dashboard](#wifi-access-point--web-dashboard)
   - [BLE GATT Server](#ble-gatt-server)
   - [On-Device UI](#on-device-ui)
     - [Main Menu](#main-menu)
     - [Games](#games)
8. [Repository Structure](#repository-structure)
9. [Hardware Files](#hardware-files)
10. [Contributing](#contributing)
11. [License](#license)

---

## Board Gallery

<div align="center">

| PCB Layout (Top) | 3D Render | PCB Back |
|:---:|:---:|:---:|
| ![PCB Layout](https://github.com/user-attachments/assets/de8ea07d-7a7d-4f35-b29c-0b182f77b6d8) | ![PCB 3D Render](https://github.com/user-attachments/assets/73806aac-859a-40c9-a0b6-220a0715fab4) | ![PCB Back](https://github.com/user-attachments/assets/2d12b77d-34e1-484e-9a60-13efce319d09) |

</div>

---

## System Architecture

The diagram below shows how all onboard subsystems connect to the ESP32-S3:

```
                        ┌─────────────────────────────────────┐
                        │           ESP32-S3-WROOM-1           │
                        │     240 MHz Xtensa LX7 Dual-Core     │
                        │     WiFi 802.11 b/g/n  •  BT 5.0    │
                        └───┬───┬───┬───┬───┬───┬───┬───┬─────┘
                            │   │   │   │   │   │   │   │
          ┌─────────────────┘   │   │   │   │   │   │   └──────────────────┐
          │ LEDC PWM            │   │   │   │   │   │ GPIO ISR             │
          ▼                     │   │   │   │   │   ▼                      │
 ┌─────────────────┐            │   │   │   │   │  ┌────────────────┐      │
 │   DRV8871 ×2    │            │   │   │   │   │  │  Hall Sensors  │      │
 │  DC Motor Ctrl  │            │   │   │   │   │  │   ×2 inputs    │      │
 │  (PWM IN1/IN2)  │            │   │   │   │   │  └────────────────┘      │
 └────────┬────────┘            │   │   │   │   │                          │
          │ VM (motor power)    │   │   │   │   │ INPUT_PULLUP             │
          ▼                     │   │   │   │   ▼                          │
    ┌───────────┐               │   │   │   │  ┌──────────────┐            │
    │  Motor ×2 │               │   │   │   │  │  Buttons ×3  │            │
    └───────────┘               │   │   │   │  └──────────────┘            │
                      PWM 50Hz  │   │   │   │                              │
                                ▼   │   │   │ SPI                          │
                       ┌────────────┐  │   ▼                               │
                       │  Servo ×2  │  │  ┌──────────────────┐             │
                       └────────────┘  │  │   SPI Display    │             │
                                       │  │  (ST7735 etc.)   │             │
                    HC-SR04 TRIG/ECHO  │  └──────────────────┘             │
                                       ▼                                   │
                           ┌────────────────────┐                          │
                           │  Ultrasonic ×4     │                          │
                           │  (distance 2–400cm)│                          │
                           └────────────────────┘                          │
                                                                           │
          ┌────────────────────────────────────────────────────────────────┘
          │ I2C 400 kHz
          ▼
 ┌──────────────────┐     ┌──────────────────────────────┐
 │  IMU Header      │     │      Power Architecture       │
 │  MPU-6050 /      │     │  VM (7–12 V) ──► DRV8871     │
 │  ICM-42688 etc.  │     │  VM ──► 3.3 V LDO ──► ESP32  │
 └──────────────────┘     │  VM ──► 5 V (servo/sensors)  │
                          └──────────────────────────────┘
```

---

## Feature Overview

| Subsystem | Component | Capability |
|---|---|---|
| **Microcontroller** | ESP32-S3-WROOM-1 | 240 MHz dual-core, WiFi, BT 5.0, 45 GPIOs |
| **Motor Drivers** | 2× DRV8871 | H-bridge, 3.6 A peak, bidirectional DC control |
| **Servo Outputs** | 2× channels | 50 Hz PWM, 1–2 ms pulse width |
| **Ultrasonic Sensors** | 4× HC-SR04 | 2–400 cm range, 15° beam angle |
| **Hall Effect Sensors** | 2× inputs | Wheel speed, pulse counting, position feedback |
| **IMU** | I2C header | MPU-6050, ICM-42688, or any I2C IMU |
| **SPI Display** | ST7735S 160×80 | TFT_eSPI driver, tabbed on-device UI |
| **Push Buttons** | 3× SMD tactile | Internal pull-up, debounced in firmware |
| **Expansion** | 6× GPIO header | GPIO 37, 38, 40, 43, 44, 48 broken out |
| **Power** | LDO + distribution | Onboard 3.3 V regulation, motor VM bus |
| **WiFi AP** | Soft Access Point | SSID `RoboController`, live sensor dashboard |
| **Web Dashboard** | AsyncWebSocket | Tabbed UI: sensors, WiFi scan, BLE radar, drive control |
| **Remote Control** | WebSocket JSON | Browser joystick → motor drive; servo sliders |
| **BLE GATT Server** | NimBLE-Arduino | Sensor notify + motor/servo write characteristics |
| **BLE Scanner** | Passive scan | Detect nearby BLE devices, shown on TFT and dashboard |

---

## PCB Design

### Layer Stackup

The board uses a **4-layer stackup** (1.6 mm total thickness) to provide dedicated power and ground planes, minimizing noise and improving signal integrity:

```
  ┌─────────────────────────────────────────────┐  ← Component side
  │  Layer 1 — F.Cu (Top)                        │  Signal routing + high-current motor traces
  ├─────────────────────────────────────────────┤
  │  Prepreg                                     │
  ├─────────────────────────────────────────────┤
  │  Layer 2 — In1.Cu                            │  Solid GND plane (unbroken)
  ├─────────────────────────────────────────────┤
  │  Core                                        │
  ├─────────────────────────────────────────────┤
  │  Layer 3 — In2.Cu                            │  VM + 3.3 V power distribution
  ├─────────────────────────────────────────────┤
  │  Prepreg                                     │
  ├─────────────────────────────────────────────┤
  │  Layer 4 — B.Cu (Bottom)                     │  Additional signals + local copper pours
  └─────────────────────────────────────────────┘  ← Solder side
```

| Layer | Role | Trace Width |
|---|---|---|
| F.Cu (Top) | Signal routing + high-current motor paths | 0.2 mm signal / 2.0 mm motor VM |
| In1.Cu | Solid GND reference plane | — |
| In2.Cu | VM + 3.3 V power distribution | 0.8 mm+ |
| B.Cu (Bottom) | Additional signals + local copper pours | 0.2 mm signal |

### Signal Integrity

> **Design decisions to ensure reliable operation in a noisy motor-drive environment:**

- **Continuous GND plane** on In1.Cu with stitching vias throughout — minimises return-path inductance for both digital signals and motor switching transients.
- **Wide copper pours** on VM traces and high-current motor paths — rated for DRV8871 peak current with thermal headroom.
- **Local decoupling capacitors** placed directly at each ESP32-S3 power pin and each DRV8871 VCC/VM pin — suppresses switching noise at the source.
- **Physical separation** between sensitive I2C/SPI/UART traces and high-current motor/PWM traces — reduces capacitive and inductive coupling.
- **Short SPI routes** between ESP32-S3 and the display connector — maintains signal integrity at SPI bus speeds.
- **Motor VM routed on power layer (In2.Cu)** — keeps motor current off the signal layers.

### Key Components (BOM Summary)

| Reference | Part | Package | Description |
|---|---|---|---|
| U1 | ESP32-S3-WROOM-1 | SMD module | Main MCU — 240 MHz, WiFi, BT 5.0 |
| U2 | DRV8871 | HSOP-8 | Motor driver — Motor 1 |
| U3 | DRV8871 | HSOP-8 | Motor driver — Motor 2 |
| U4 | 3.3 V LDO | SOT-223 | 3.3 V regulated power for MCU and logic |
| J1–J4 | HC-SR04 headers | 4-pin 2.54 mm | Ultrasonic sensor connectors |
| J5–J6 | Hall sensor headers | 3-pin 2.54 mm | Hall effect sensor connectors |
| J7–J8 | Motor terminals | 2-pin screw terminal | Motor output connections |
| J9 | Expansion header | 8-pin 2.54 mm | Breakout for unused GPIO |
| J10 | Display header | 7-pin 2.54 mm | SPI display connector |
| SW1–SW3 | Tactile switches | SMD | User input buttons |

> Full BOM with part numbers and quantities is available in [`hardware/bom/`](hardware/bom/).

---

## GPIO Pin Reference

| GPIO | Function | Type | Notes |
|---|---|---|---|
| 1 | Hall Sensor 1 | Digital Input | Interrupt-driven pulse counting |
| 2 | Hall Sensor 2 | Digital Input | Interrupt-driven pulse counting |
| 4 | Motor 1 PWM A | LEDC Output | 20 kHz PWM, 8-bit resolution |
| 5 | Motor 1 PWM B | LEDC Output | 20 kHz PWM, 8-bit resolution |
| 6 | Motor 2 PWM A | LEDC Output | 20 kHz PWM, 8-bit resolution |
| 7 | Motor 2 PWM B | LEDC Output | 20 kHz PWM, 8-bit resolution |
| 8 | Servo 1 | PWM Output | 50 Hz via ESP32Servo library |
| 9 | Servo 2 | PWM Output | 50 Hz via ESP32Servo library |
| 10 | Ultrasonic 1 TRIG | Digital Output | 10 µs trigger pulse |
| 11 | Ultrasonic 1 ECHO | Digital Input | Pulse width → distance |
| 12 | Ultrasonic 2 TRIG | Digital Output | |
| 13 | Ultrasonic 2 ECHO | Digital Input | |
| 14 | Ultrasonic 3 TRIG | Digital Output | |
| 15 | Ultrasonic 3 ECHO | Digital Input | |
| 16 | Ultrasonic 4 TRIG | Digital Output | |
| 17 | Ultrasonic 4 ECHO | Digital Input | |
| 18 | I2C SDA (IMU) | I2C | 400 kHz fast mode |
| 21 | I2C SCL (IMU) | I2C | 400 kHz fast mode |
| 33 | SPI MOSI (Display) | SPI | |
| 34 | SPI SCLK (Display) | SPI | |
| 35 | SPI CS | SPI | Active low chip select |
| 36 | SPI DC | Digital Output | Data/Command select |
| 39 | SPI RES | Digital Output | Display reset |
| 41 | Button 1 | Digital Input | `INPUT_PULLUP`, active LOW |
| 42 | Button 2 | Digital Input | `INPUT_PULLUP`, active LOW |
| 47 | Button 3 | Digital Input | `INPUT_PULLUP`, active LOW |
| 37, 38, 40, 43, 44, 48 | Expansion Header | GPIO | General purpose — broken out to J9 |

---

## Peripheral Details

### Motor Control — DRV8871

The **DRV8871** is a brushed DC motor driver with an integrated H-bridge, supporting up to **3.6 A peak** current with current-limit protection. Two independent drivers are included for dual-motor operation.

| Motor | PWM A GPIO | PWM B GPIO | LEDC Channel A | LEDC Channel B |
|---|---|---|---|---|
| Motor 1 | 4 | 5 | 0 | 1 |
| Motor 2 | 6 | 7 | 2 | 3 |

**Control truth table:**

| Mode | PWM A | PWM B | Description |
|---|---|---|---|
| Forward | PWM duty | LOW | Speed proportional to duty cycle |
| Reverse | LOW | PWM duty | Speed proportional to duty cycle |
| Coast | LOW | LOW | Motor freewheels (no braking) |
| Brake | HIGH | HIGH | Active braking, motor stops quickly |

```cpp
// Example: drive Motor 1 forward at ~70% speed
motor_set(MotorId::MOTOR1, 180);   // 180/255 ≈ 70%

// Brake Motor 1
motor_brake(MotorId::MOTOR1);

// Coast Motor 2
motor_coast(MotorId::MOTOR2);
```

> **PWM frequency:** 20 kHz (above audible range — eliminates motor whine).  
> **Resolution:** 8-bit (0–255 duty cycle values).

---

### Servo Outputs

Two standard RC servo channels output 50 Hz PWM signals compatible with any hobby servo.

| Servo | GPIO | Pulse Width Range |
|---|---|---|
| Servo 1 | 8 | 1000–2000 µs |
| Servo 2 | 9 | 1000–2000 µs |

**Pulse width to angle mapping:**

```
1000 µs  ──►  0°   (full left)
1500 µs  ──►  90°  (center / neutral)
2000 µs  ──►  180° (full right)
```

```cpp
// Example: center both servos
servo_set_angle(ServoId::SERVO1, 90);
servo_set_angle(ServoId::SERVO2, 90);
```

---

### Ultrasonic Sensors — HC-SR04

Four **HC-SR04** interfaces provide omnidirectional environmental awareness. Each sensor uses two GPIO pins: one for triggering and one for echo timing.

| Sensor | TRIG GPIO | ECHO GPIO | Max Range | Beam Angle |
|---|---|---|---|---|
| Sensor 1 | 10 | 11 | ~400 cm | ~15° |
| Sensor 2 | 12 | 13 | ~400 cm | ~15° |
| Sensor 3 | 14 | 15 | ~400 cm | ~15° |
| Sensor 4 | 16 | 17 | ~400 cm | ~15° |

**Operating principle:**

```
MCU                    HC-SR04
 │                        │
 │── 10 µs HIGH ─────────►│  TRIG pulse sent
 │                        │── 40 kHz ultrasonic burst ──►
 │                        │                    ◄── echo return
 │◄── pulse width (ECHO) ─│  width ∝ distance
 │
 Distance (cm) = pulse_width_µs × 0.0343 / 2
```

> **Timeout:** 30 ms (~5 m) — sensors beyond range return -1.

---

### Hall Effect Sensors

Two digital Hall effect sensor inputs support wheel speed measurement, odometry, and position feedback. Sensors are read via GPIO interrupts for accurate pulse counting.

| Sensor | GPIO | Mode |
|---|---|---|
| Hall 1 | 1 | Interrupt (rising edge) |
| Hall 2 | 2 | Interrupt (rising edge) |

```cpp
// Example: read rotational speed
float pps1 = hall_get_pulses_per_second(1);   // pulses per second
float pps2 = hall_get_pulses_per_second(2);
```

To calculate RPM, divide pulses per second by the number of magnet poles on the wheel.

---

### IMU — I2C

A dedicated I2C header supports any standard I2C IMU module.

| Signal | GPIO | Frequency |
|---|---|---|
| SDA | 18 | 400 kHz (fast mode) |
| SCL | 21 | 400 kHz (fast mode) |

**Compatible IMUs (non-exhaustive):**

| Part | Axes | Interface |
|---|---|---|
| MPU-6050 | 6-axis (accel + gyro) | I2C (0x68 / 0x69) |
| ICM-42688 | 6-axis (accel + gyro) | I2C / SPI |
| BNO055 | 9-axis (+ magnetometer, fusion) | I2C (0x28 / 0x29) |
| LSM6DSO | 6-axis | I2C / SPI |

```cpp
// Example: read IMU data
ImuData imu;
if (imu_read(imu)) {
    // accel in g, gyro in °/s
    Serial.printf("ax=%.2f  ay=%.2f  az=%.2f\n",
                  imu.accel_x, imu.accel_y, imu.accel_z);
}
```

---

### SPI Display

An SPI display connector supports ST7735, ST7789, ILI9341, or any SPI display with a compatible driver.

| Signal | GPIO | Function |
|---|---|---|
| MOSI (SDA) | 33 | Data to display |
| SCLK (SCL) | 34 | SPI clock |
| CS | 35 | Chip select (active LOW) |
| DC | 36 | Data / Command select |
| RES | 39 | Hardware reset |

The firmware uses **TFT_eSPI** for hardware-accelerated graphics and text rendering on the ST7735S 160×80 display. A custom colour palette and tabbed on-device UI are implemented in `display.h/cpp` and `ui.h/cpp`.

---

### Push Buttons

Three SMD tactile push buttons provide user input. All are wired active-LOW with internal pull-ups enabled in firmware.

| Button | GPIO | Pull Configuration |
|---|---|---|
| Button 1 | 41 | `INPUT_PULLUP` (active LOW) |
| Button 2 | 42 | `INPUT_PULLUP` (active LOW) |
| Button 3 | 47 | `INPUT_PULLUP` (active LOW) |

Software debouncing is applied with a configurable debounce window (`BUTTON_DEBOUNCE_MS`, default 20 ms).

```cpp
// Example: detect button press events
buttons_update();   // call every loop iteration

if (button_just_pressed(ButtonId::BTN1)) {
    // single-shot event — true on the first loop after press
}
```

---

### Expansion Header

Six GPIO pins are broken out on **J9** for custom peripherals, additional sensors, or future expansion.

| Header Pin | GPIO | Notes |
|---|---|---|
| 1 | 37 | General purpose |
| 2 | 38 | General purpose |
| 3 | 40 | General purpose |
| 4 | 43 | UART0 TX (also used for USB CDC) |
| 5 | 44 | UART0 RX (also used for USB CDC) |
| 6 | 48 | General purpose / RGB LED capable |
| 7 | 3.3 V | Power rail |
| 8 | GND | Ground |

> GPIO 43 and 44 are the default UART0 pins and can be used for serial communication with external devices.

---

## Firmware

### Quick Start

```bash
# 1. Clone the repository
git clone https://github.com/averyizatt/ESP32BasedRobiticsPCB.git

# 2. Open in PlatformIO (recommended)
#    File → Open Folder → ESP32BasedRobiticsPCB
#    PlatformIO auto-resolves all lib_deps from platformio.ini

# 3. Build
pio run --environment esp32s3

# 4. Upload
pio run --target upload --environment esp32s3
```

1. Install **PlatformIO** (VS Code extension or CLI)
2. Open the repo root as the workspace
3. PlatformIO resolves all library dependencies automatically from `platformio.ini`
4. Select the **esp32s3** environment, build, and upload
5. After boot, connect to WiFi SSID `RoboController` (password `robot1234`) and navigate to `http://192.168.4.1`

### Library Dependencies

All dependencies are declared in `platformio.ini` and resolved automatically by PlatformIO. For manual Arduino IDE installs use the Library Manager names below.

| Library | Version | Purpose |
|---|---|---|
| `madhephaestus/ESP32Servo` | ^3.0.6 | RC servo 50 Hz PWM |
| `bodmer/TFT_eSPI` | ^2.5.43 | ST7735S SPI display driver + graphics |
| `ESP32Async/AsyncTCP` | ^3.3.2 | Async TCP foundation for web server |
| `ESP32Async/ESPAsyncWebServer` | ^3.7.4 | Async HTTP + WebSocket server |
| `h2zero/NimBLE-Arduino` | ^1.4.3 | BLE stack (GATT server + scanning) |
| `Wire` | built-in | I2C for IMU |
| `WiFi` | built-in | Soft AP + network scanning |

> **TFT_eSPI user config:** The library reads pin assignments from `User_Setup.h`. Ensure the file in your PlatformIO build includes the ST7735S settings matching `config.h` (MOSI=37, SCLK=38, CS=35, DC=36, RST=39).

### Firmware Architecture

The firmware is organized as a set of independent peripheral modules — each with its own header and source file — all coordinated from `main.cpp`:

```
ESP32RoboticsController/
├── main.cpp                         ← setup() + loop() entry point
├── config.h                         ← All pin definitions and tuning constants
│
├── motors.h / motors.cpp            ← DRV8871 LEDC-based motor control
├── servos.h / servos.cpp            ← RC servo PWM control
├── ultrasonics.h / ultrasonics.cpp  ← HC-SR04 distance measurement
├── hall_sensors.h / hall_sensors.cpp← Interrupt-based pulse counting
├── imu.h / imu.cpp                  ← I2C IMU read abstraction (MPU-6050)
├── buttons.h / buttons.cpp          ← Debounced button state machine
├── led.h / led.cpp                  ← Onboard LED control
├── pot.h                            ← Potentiometer / ADC helper
│
├── display.h / display.cpp          ← TFT_eSPI display abstraction + colour palette
├── ui.h / ui.cpp                    ← On-device tabbed UI state machine
├── games.h / games.cpp              ← Mini-game / demo logic
│
└── wifi_server.h / wifi_server.cpp  ← WiFi soft AP, AsyncWebServer, WebSocket push,
                                        BLE GATT server, WiFi/BLE scanner helpers
```

All pin assignments and tunable parameters are centralized in `config.h` — adapting the firmware to a modified hardware revision only requires editing that single file.

### PWM Configuration

Motor PWM is generated using the ESP32-S3's **LEDC peripheral**, which provides hardware-timed PWM independent of the CPU:

| Parameter | Value | Notes |
|---|---|---|
| Frequency | 20 kHz | Above human hearing — eliminates audible motor whine |
| Resolution | 8-bit | 0–255 duty cycle values |
| Channels | 4 (0–3) | One per motor H-bridge input |

---

### WiFi Access Point & Web Dashboard

The firmware starts a **soft Access Point** on boot. Connect any browser-capable device to control and monitor the robot.

| Setting | Value |
|---|---|
| SSID | `RoboController` |
| Password | `robot1234` |
| IP Address | `192.168.4.1` |
| WebSocket | `ws://192.168.4.1/ws` |

The dashboard is a single-page app served from PROGMEM with four tabs:

| Tab | Contents |
|---|---|
| **Sensors** | Live IMU (accel + gyro axis bars), ultrasonic proximity radar, heap gauge, hall encoder counts, button tiles, uptime |
| **WiFi Scanner** | Scan nearby networks — SSID, signal strength bar, channel, security badge |
| **BLE Radar** | Scan for BLE devices — name, RSSI, MAC address |
| **Control** | Virtual joystick (touch + mouse), dual servo sliders, emergency stop, command log |

Sensor data is pushed over **WebSocket** (no polling) at a configurable interval (`WIFI_WS_PUSH_MS`, default 200 ms).

#### REST Endpoints

| Endpoint | Method | Response |
|---|---|---|
| `/` | GET | Full HTML dashboard |
| `/api/scan` | GET | JSON array of nearby WiFi networks |
| `/api/ble` | GET | JSON array of scanned BLE devices (~4 s scan) |

#### WebSocket Commands (browser → robot)

The robot accepts inbound JSON commands over the same WebSocket connection:

```json
// Drive both motors (speed: -255 to +255)
{ "t": "drive", "l": 200, "r": -150 }

// Set servo angle (id: 1 or 2, angle: 0–180°)
{ "t": "servo", "id": 1, "angle": 90 }

// Emergency stop — coasts both motors
{ "t": "stop" }
```

---

### BLE GATT Server

The firmware advertises a custom BLE GATT service alongside the WiFi AP — connect from **nRF Connect**, **LightBlue**, or any GATT-capable app.

| Item | UUID | Properties | Format |
|---|---|---|---|
| Service | `AB12` | — | — |
| Sensor Notify | `AA13` | Read + Notify | Compact JSON: accel, gyro, ultrasonic |
| Motor Write | `AA14` | Write | `l:<speed>,r:<speed>` e.g. `l:200,r:-150` |
| Servo Write | `AA15` | Write | `<id>:<angle>` e.g. `1:90` |

The sensor characteristic sends a live data notification every **500 ms** when a central is connected:

```json
{"ax":0.01,"ay":-0.03,"az":9.81,"gx":0.2,"gy":-0.1,"gz":0.0,"u":[45.2,120.0,-1.0,88.4]}
```

(`u` values are ultrasonic distances in cm; `-1` = no echo / out of range)

---

### On-Device UI

A tabbed menu system runs on the 160×80 TFT display. Navigation uses two push buttons and the onboard potentiometer. An auto-idle screensaver activates after 20 seconds of inactivity.

**Navigation:**
- **BTN1 (CYCLE)** — scroll to next item / primary action in games
- **BTN2 (SELECT)** — confirm / enter / secondary action in games
- **Potentiometer** — scroll lists, pilot characters in games, adjust values

#### Main Menu

| Menu Item | Screen | Description |
|---|---|---|
| Sensors | Live readout | All sensor values: ultrasonic distances, hall encoder counts, button states, uptime |
| IMU Data | Live readout | Accelerometer (X/Y/Z in g) and gyroscope (X/Y/Z in °/s) with temperature |
| Tilt Demo | Animation | Graphical liquid-level tilt visualiser driven by IMU pitch/roll |
| Motors | Live readout | Current motor speed and direction for both motor channels |
| Ultrasonic | Radar demo | Real-time 4-direction proximity radar on TFT |
| Hall Demo | Live readout | Wheel pulse counts and pulses-per-second for both encoder channels |
| Servo Demo | Auto sweep | Automated servo 1 & 2 sweep from 0° → 180° → 0° |
| System | Info screen | Firmware uptime, heap usage, CPU frequency, chip info |
| Games | Sub-menu | Arcade game launcher (see Games section below) |
| WiFi Info | Info screen | AP SSID, password, IP address, connected client count |
| WiFi Scan | Scanner | Nearby WiFi networks with RSSI bars; pot scrolls list |
| BLE Radar | Scanner | Nearby BLE devices with RSSI bars; 4-second active scan |

#### Games

The Games menu launches a sub-menu with three sprite-rendered arcade games. All games use a **full-screen off-screen framebuffer** (TFT_eSprite) pushed via DMA for zero-flicker 40 fps rendering. Win/loss statistics are stored in RAM for the session and displayed in the game menu next to each title.

---

##### Snake

> Classic snake — eat food to grow, avoid your own tail. Walls wrap.

| Detail | Value |
|---|---|
| Grid | 40 × 14 cells (4 px each) |
| Win condition | Score 20 food pieces |
| Controls | Potentiometer (turn left/right) or BTN1/BTN2 (turn right/left) |
| Speed | Increases with score (160 ms → 50 ms minimum tick) |
| Visual effects | Gradient tail fade, pulsing food, particle burst on eat/death, screen shake on collision |

---

##### Pong

> Single-player Pong — keep the ball alive and score paddle hits.

| Detail | Value |
|---|---|
| Win condition | 10 paddle hits |
| Lives | 3 |
| Controls | Potentiometer maps directly to paddle Y position; BTN1 nudges up |
| Difficulty | Ball speed +0.15 per hit, capped at 5.5 px/frame |
| Visual effects | 6-frame ball trail, glowing neon paddle, particle bursts on wall/paddle hits, screen shake on miss |

---

##### Asteroids

> Side-scrolling asteroid dodge — survive as long as possible.

| Detail | Value |
|---|---|
| Win condition | Survive 1500 score ticks (~37 s at 40 fps) |
| Lives | 3 |
| Controls | Potentiometer steers ship Y (smoothed); BTN1 held = thrust boost upward |
| Difficulty | Rock speed 2.2 → 5.0 px/frame; spawn interval 1100 → 350 ms |
| Visual effects | 3-layer parallax starfield, procedural 8-vertex asteroid shapes, thruster flame + exhaust particles, invulnerability blink after hit, screen shake on collision |

---

All three games share:
- **Particle system** — 24-particle pool with gravity and colour fade, reused across all effects
- **Screen shake** — configurable intensity applied as DMA push offset
- **Session W/L stats** — shown in the games menu and on the game-over screen
- **LED feedback** — green on score/win, red on death/hit, blue on game start

---

## Repository Structure

```
ESP32BasedRobiticsPCB/
│
├── platformio.ini                     ← PlatformIO project config + lib_deps
│
├── firmware/
│   └── ESP32RoboticsController/
│       ├── main.cpp                   ← setup() + loop() entry point
│       ├── config.h                   ← Pin map & constants (start here)
│       ├── motors.h / motors.cpp      ← DRV8871 LEDC motor control
│       ├── servos.h / servos.cpp      ← RC servo PWM
│       ├── ultrasonics.h / ultrasonics.cpp  ← HC-SR04 distance
│       ├── hall_sensors.h / hall_sensors.cpp← Interrupt pulse counting
│       ├── imu.h / imu.cpp            ← MPU-6050 I2C abstraction
│       ├── buttons.h / buttons.cpp    ← Debounced buttons
│       ├── led.h / led.cpp            ← Onboard LED
│       ├── pot.h                      ← ADC potentiometer
│       ├── display.h / display.cpp    ← TFT_eSPI wrapper + colour palette
│       ├── ui.h / ui.cpp              ← On-device tabbed UI state machine
│       ├── games.h / games.cpp        ← Demo / mini-game logic
│       └── wifi_server.h / wifi_server.cpp  ← WiFi AP, web dashboard,
│                                              WebSocket, BLE GATT
│
└── hardware/                          # KiCad 7 PCB design files
    ├── kicadProject/
    │   ├── Tank.kicad_pro
    │   ├── Tank.kicad_sch
    │   ├── Tank.kicad_pcb
    │   └── Tank.step
    ├── gerbers/                        ← Fabrication-ready Gerber files
    └── bom/                            ← Bill of materials (bom.csv)
```

---

## Hardware Files

KiCad design files are in the [`hardware/`](hardware/) directory.

1. Install [KiCad 7](https://www.kicad.org/download/) or later
2. Open `hardware/ESP32RoboticsController.kicad_pro`
3. The schematic opens in **Eeschema**; the PCB layout opens in **Pcbnew**

**Generating Gerbers for fabrication** (Pcbnew → File → Fabrication Outputs → Gerbers):

| File | Layer |
|---|---|
| `*-F_Cu.gbr` | Top copper |
| `*-In1_Cu.gbr` | Inner GND plane |
| `*-In2_Cu.gbr` | Inner power plane |
| `*-B_Cu.gbr` | Bottom copper |
| `*-F_SilkS.gbr` / `*-B_SilkS.gbr` | Silkscreen |
| `*-F_Mask.gbr` / `*-B_Mask.gbr` | Solder mask |
| `*-F_Paste.gbr` | Solder paste |
| `*-Edge_Cuts.gbr` | Board outline |
| `*.drl` | Drill file (Excellon) |

Pre-generated Gerber files are available in [`hardware/gerbers/`](hardware/gerbers/).

---

## Contributing

Contributions are welcome! To contribute:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-improvement`)
3. Commit your changes with a clear message
4. Open a pull request describing what you changed and why

**For hardware changes:** please include updated Gerber files and a brief description of the PCB modification.  
**For firmware changes:** please test on hardware before submitting.

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

<div align="center">

Made with ❤️ for the robotics community

</div>
