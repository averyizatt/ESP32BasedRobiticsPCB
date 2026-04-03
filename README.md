# ESP32-S3 Robotics Controller PCB

A 4-layer, ESP32-S3 based robotics controller hub designed to consolidate motor control, sensing, and user interaction into a single, clean, and scalable platform for robotics development.

## Board Overview

![PCB Layout](https://github.com/user-attachments/assets/de8ea07d-7a7d-4f35-b29c-0b182f77b6d8)

![PCB 3D Render](https://github.com/user-attachments/assets/73806aac-859a-40c9-a0b6-220a0715fab4)

![PCB Back](https://github.com/user-attachments/assets/2d12b77d-34e1-484e-9a60-13efce319d09)

At the core of the system is the **ESP32-S3**, chosen for its processing capability, flexible GPIO matrix, integrated WiFi and Bluetooth, and strong support for real-time control tasks.

### Integrated Features

| Feature | Details |
|---|---|
| Motor Drivers | Dual DRV8871 — bidirectional DC motor control |
| Servo Outputs | 2× servo channels with standard 50 Hz PWM |
| Ultrasonic Sensors | 4× HC-SR04 interfaces for environmental awareness |
| Hall Effect Sensors | 2× inputs for wheel speed and position feedback |
| SPI Display | Onboard telemetry, debugging, and UI |
| IMU | I2C IMU header (e.g., MPU-6050 / ICM-42688) |
| Push Buttons | 3× user buttons with pull-up configuration |
| Power | Onboard linear regulation and power distribution |
| Expansion | Dedicated header for unused GPIO (37, 38, 40, 43, 44, 48) |

---

## PCB Design

The PCB uses a **4-layer stackup**:

| Layer | Purpose |
|---|---|
| Top | Signal routing and high-current paths |
| Inner 1 | Solid ground plane |
| Inner 2 | Power distribution (VM and regulated rails) |
| Bottom | Additional signal routing and localized pours |

Key design considerations:
- Continuous ground plane with stitching vias for low-impedance return paths
- Wide copper traces and local pours for high-current motor paths
- Decoupling capacitors placed close to ESP32-S3 power pins and motor drivers
- Sensitive sensor lines routed away from noisy motor and power traces
- Short, direct SPI signal routes to the display

### Repository Structure

```
ESP32BasedRobiticsPCB/
├── firmware/                  # Arduino/ESP32-S3 firmware template
│   └── ESP32RoboticsController/
│       ├── ESP32RoboticsController.ino
│       ├── config.h
│       ├── motors.h / motors.cpp
│       ├── servos.h / servos.cpp
│       ├── ultrasonics.h / ultrasonics.cpp
│       ├── hall_sensors.h / hall_sensors.cpp
│       ├── display.h / display.cpp
│       ├── imu.h / imu.cpp
│       └── buttons.h / buttons.cpp
└── hardware/                  # KiCad PCB design files
    ├── ESP32RoboticsController.kicad_pro
    ├── ESP32RoboticsController.kicad_sch
    ├── ESP32RoboticsController.kicad_pcb
    └── ...
```

---

## GPIO Pin Reference

| GPIO | Function |
|---|---|
| 1 | Hall Sensor 1 |
| 2 | Hall Sensor 2 |
| 4 | Motor 1 PWM A |
| 5 | Motor 1 PWM B |
| 6 | Motor 2 PWM A |
| 7 | Motor 2 PWM B |
| 8 | Servo 1 |
| 9 | Servo 2 |
| 10 | Ultrasonic 1 TRIG |
| 11 | Ultrasonic 1 ECHO |
| 12 | Ultrasonic 2 TRIG |
| 13 | Ultrasonic 2 ECHO |
| 14 | Ultrasonic 3 TRIG |
| 15 | Ultrasonic 3 ECHO |
| 16 | Ultrasonic 4 TRIG |
| 17 | Ultrasonic 4 ECHO |
| 18 | I2C SDA (IMU) |
| 21 | I2C SCL (IMU) |
| 33 | SPI MOSI (Display SDA) |
| 34 | SPI SCLK (Display SCL) |
| 35 | SPI CS |
| 36 | SPI DC |
| 39 | SPI RES |
| 41 | Button 1 |
| 42 | Button 2 |
| 47 | Button 3 |
| 37, 38, 40, 43, 44, 48 | Expansion header GPIO |

---

## How to Use

### Motor Control (DRV8871)

| Motor | PWM A | PWM B |
|---|---|---|
| Motor 1 | GPIO 4 | GPIO 5 |
| Motor 2 | GPIO 6 | GPIO 7 |

| State | PWM A | PWM B |
|---|---|---|
| Forward | HIGH (PWM) | LOW |
| Reverse | LOW | HIGH (PWM) |
| Coast | LOW | LOW |
| Brake | HIGH | HIGH |

### Servo Outputs

| Servo | GPIO |
|---|---|
| Servo 1 | 8 |
| Servo 2 | 9 |

Standard 50 Hz PWM: ~1.0 ms → 0°, ~1.5 ms → center, ~2.0 ms → 180°

### Ultrasonic Sensors

| Sensor | TRIG | ECHO |
|---|---|---|
| Sensor 1 | 10 | 11 |
| Sensor 2 | 12 | 13 |
| Sensor 3 | 14 | 15 |
| Sensor 4 | 16 | 17 |

Operation: Send 10 µs pulse on TRIG → measure pulse width on ECHO → convert to distance.

### Hall Effect Sensors

| Sensor | GPIO |
|---|---|
| Hall 1 | 1 |
| Hall 2 | 2 |

Used for speed measurement, rotation counting, and position feedback.

### IMU (I2C)

SDA → GPIO 18 | SCL → GPIO 21

Compatible with MPU-6050, ICM-42688, or similar I2C IMUs.

### SPI Display

| Signal | GPIO |
|---|---|
| MOSI (SDA) | 33 |
| SCLK (SCL) | 34 |
| CS | 35 |
| DC | 36 |
| RES | 39 |

### Push Buttons

| Button | GPIO |
|---|---|
| Button 1 | 41 |
| Button 2 | 42 |
| Button 3 | 47 |

All buttons use `pinMode(pin, INPUT_PULLUP)`.

### Expansion Header

Available GPIO for expansion: **37, 38, 40, 43, 44, 48**

---

## Firmware Setup

1. Install [Arduino IDE](https://www.arduino.cc/en/software) or [PlatformIO](https://platformio.org/)
2. Add ESP32-S3 board support (Arduino: Boards Manager → "esp32" by Espressif)
3. Open `firmware/ESP32RoboticsController/ESP32RoboticsController.ino`
4. Select board: **ESP32S3 Dev Module**
5. Install required libraries (see `firmware/ESP32RoboticsController/config.h` for details)
6. Upload to board

---

## Hardware Files

KiCad design files are located in the `hardware/` directory. Open `hardware/ESP32RoboticsController.kicad_pro` with KiCad 7 or later.

---

## License

MIT License — see [LICENSE](LICENSE) for details.
