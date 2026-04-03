// =============================================================================
// ESP32-S3 Robotics Controller — Main Firmware Template
// =============================================================================
// Board:    ESP32S3 Dev Module
// Target:   averyizatt/ESP32BasedRobiticsPCB custom 4-layer PCB
//
// This template initialises all onboard peripherals and demonstrates basic
// usage. Customise the loop() function for your application logic.
//
// Required libraries (install via Arduino Library Manager):
//   - ESP32Servo       (servo control)
//   - Adafruit GFX     (display graphics)
//   - Adafruit ST7735  (or your display driver — see display.cpp)
//   - Wire / SPI       (built-in with ESP32 core)
// =============================================================================

#include "config.h"
#include "motors.h"
#include "servos.h"
#include "ultrasonics.h"
#include "hall_sensors.h"
#include "display.h"
#include "imu.h"
#include "buttons.h"

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {} // Wait up to 3 s for serial monitor
    Serial.println("[boot] ESP32-S3 Robotics Controller starting...");

    // Peripherals
    motors_init();
    Serial.println("[boot] motors ready");

    servos_init();
    Serial.println("[boot] servos ready");

    ultrasonics_init();
    Serial.println("[boot] ultrasonics ready");

    hall_sensors_init();
    Serial.println("[boot] hall sensors ready");

    display_init();
    display_clear();
    display_print_line(0, "Robotics Controller");
    display_print_line(1, "Booting...");
    Serial.println("[boot] display ready");

    bool imu_ok = imu_init();
    Serial.printf("[boot] imu %s\n", imu_ok ? "ready" : "NOT found");

    buttons_init();
    Serial.println("[boot] buttons ready");

    display_clear();
    display_print_line(0, "Ready!");
    Serial.println("[boot] initialisation complete");
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    // Update debounce state for all buttons every cycle
    buttons_update();

    // --- Button demo ---
    if (button_just_pressed(ButtonId::BTN1)) {
        Serial.println("[btn1] pressed");
        display_print_line(3, "BTN1 pressed");
    }
    if (button_just_pressed(ButtonId::BTN2)) {
        Serial.println("[btn2] pressed");
        display_print_line(3, "BTN2 pressed");
    }
    if (button_just_pressed(ButtonId::BTN3)) {
        Serial.println("[btn3] pressed");
        display_print_line(3, "BTN3 pressed");
    }

    // --- Ultrasonic sensor demo (read every 100 ms) ---
    static unsigned long last_us_read = 0;
    if (millis() - last_us_read >= 100) {
        last_us_read = millis();
        float distances[4];
        ultrasonics_read_all_cm(distances);
        for (int i = 0; i < 4; i++) {
            if (distances[i] >= 0) {
                Serial.printf("[us%d] %.1f cm\n", i + 1, distances[i]);
                display_status(4 + i, ("US" + String(i + 1)).c_str(), distances[i]);
            } else {
                Serial.printf("[us%d] out of range\n", i + 1);
            }
        }
    }

    // --- Hall sensor speed (report every 500 ms) ---
    static unsigned long last_hall_read = 0;
    if (millis() - last_hall_read >= 500) {
        last_hall_read = millis();
        float pps1 = hall_get_pulses_per_second(1);
        float pps2 = hall_get_pulses_per_second(2);
        Serial.printf("[hall] ch1=%.1f pps  ch2=%.1f pps\n", pps1, pps2);
        display_status(8, "Hall1", pps1);
        display_status(9, "Hall2", pps2);
    }

    // --- IMU demo (read every 50 ms) ---
    static unsigned long last_imu_read = 0;
    if (millis() - last_imu_read >= 50) {
        last_imu_read = millis();
        ImuData imu;
        if (imu_read(imu)) {
            Serial.printf("[imu] ax=%.2f ay=%.2f az=%.2f | gx=%.1f gy=%.1f gz=%.1f\n",
                          imu.accel_x, imu.accel_y, imu.accel_z,
                          imu.gyro_x, imu.gyro_y, imu.gyro_z);
        }
    }

    // --- Motor demo: drive forward for 2 s, then stop ---
    // Uncomment to test motors:
    //
    // motor_set(MotorId::MOTOR1, 180);   // ~70% forward
    // motor_set(MotorId::MOTOR2, 180);
    // delay(2000);
    // motor_coast(MotorId::MOTOR1);
    // motor_coast(MotorId::MOTOR2);
    // delay(1000);

    // --- Servo demo: sweep 0–180° ---
    // Uncomment to test servos:
    //
    // for (int angle = 0; angle <= 180; angle += 5) {
    //     servo_set_angle(ServoId::SERVO1, angle);
    //     servo_set_angle(ServoId::SERVO2, 180 - angle);
    //     delay(20);
    // }

    display_update();
}
