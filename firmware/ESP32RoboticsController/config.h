#pragma once

// =============================================================================
// ESP32-S3 Robotics Controller — Pin Configuration
// =============================================================================
// Board: ESP32S3 Dev Module
// Required libraries:
//   - ESP32Servo       (servo control)
//   - Wire             (I2C — built-in)
//   - SPI              (SPI — built-in)
//   - TFT_eSPI         (display driver, replaces Adafruit_ST7735)
//   - NimBLE-Arduino   (BLE GATT + scanner)
//   - ESPAsyncWebServer + AsyncTCP  (web dashboard)
// =============================================================================

// --- Motor 1 (DRV8871) ---
#define MOTOR1_PWM_A  4
#define MOTOR1_PWM_B  5

// --- Motor 2 (DRV8871) ---
#define MOTOR2_PWM_A  6
#define MOTOR2_PWM_B  7

// --- Servo Outputs ---
#define SERVO1_PIN    8
#define SERVO2_PIN    9

// --- Ultrasonic Sensors (HC-SR04) ---
#define ULTRASONIC1_TRIG  10
#define ULTRASONIC1_ECHO  11
#define ULTRASONIC2_TRIG  12
#define ULTRASONIC2_ECHO  13
#define ULTRASONIC3_TRIG  14
#define ULTRASONIC3_ECHO  15
#define ULTRASONIC4_TRIG  16
#define ULTRASONIC4_ECHO  17

// --- I2C (IMU) ---
#define I2C_SDA_PIN   18
#define I2C_SCL_PIN   21
#define I2C_CLOCK_HZ  400000UL  // 400 kHz fast-mode

// --- Onboard RGB LED (WS2812 / NeoPixel on ESP32-S3-DevKitC-1) ---
#define LED_PIN       48

// --- Potentiometer (analog dial for menu navigation) ---
// Repurposed from SERVO2_PIN (GPIO 9) — ADC1 channel.
#define POT_PIN       9

// --- SPI Display (I2C-style labels: SDA=MOSI, SCL=SCLK) ---
#define DISPLAY_MOSI  37   // SDA on module
#define DISPLAY_SCLK  38   // SCL on module
#define DISPLAY_CS    35
#define DISPLAY_DC    36
#define DISPLAY_RES   39

// --- Push Buttons (INPUT_PULLUP, active LOW) ---
#define BUTTON1_PIN   41
#define BUTTON2_PIN   42
// BTN3/GPIO47 is not populated on the current hardware.

// --- Hall Effect Sensors ---
#define HALL1_PIN     1
#define HALL2_PIN     2

// --- Expansion Header GPIO ---
// These pins are broken out on the expansion header for future use.
// #define EXP_GPIO_37  37   // now used: DISPLAY_MOSI
// #define EXP_GPIO_38  38   // now used: DISPLAY_SCLK
// #define EXP_GPIO_39  39   // now used: DISPLAY_RES
// #define EXP_GPIO_40  40
// #define EXP_GPIO_43  43
// #define EXP_GPIO_44  44
// #define EXP_GPIO_48  48

// --- LEDC PWM (motor control) ---
#define MOTOR_PWM_FREQ      20000   // 20 kHz — above audible range
#define MOTOR_PWM_RESOLUTION    8   // 8-bit: 0–255
#define MOTOR1A_CHANNEL         0
#define MOTOR1B_CHANNEL         1
#define MOTOR2A_CHANNEL         2
#define MOTOR2B_CHANNEL         3

// --- Servo ---
#define SERVO_MIN_US    1000    // 0°
#define SERVO_MAX_US    2000    // 180°

// --- Ultrasonic ---
#define ULTRASONIC_TIMEOUT_US   30000UL // 30 ms → ~5 m max range
#define SOUND_SPEED_CM_PER_US   0.0343f // cm per µs (at ~20°C)

// --- Buttons ---
#define BUTTON_DEBOUNCE_MS  20
