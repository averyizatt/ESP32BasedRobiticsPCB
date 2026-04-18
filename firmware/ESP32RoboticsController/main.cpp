// =============================================================================
// ESP32-S3 Robotics Controller — Main Entry Point
// =============================================================================

#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "buttons.h"
#include "motors.h"
#include "servos.h"
#include "ultrasonics.h"
#include "hall_sensors.h"
#include "imu.h"
#include "led.h"
#include "pot.h"
#include "wifi_server.h"
#include "ui.h"

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[main] ESP32-S3 Robotics Controller");

    led_init();
    led_blue();

    Serial.println("[main] Initializing IMU on I2C...");
    bool imu_found = imu_init();
    Serial.printf("[main] IMU %s\n", imu_found ? "found" : "not found");

    // Display now uses its own SPI pins, so the IMU stays available on I2C.
    display_init();
    pot_init();

    wifi_server_init();

    // Boot sequence — shows splash, self-test, menu
    ui_boot(imu_found);
}

void loop() {
    buttons_update();
    pot_update();
    ui_update();
    wifi_server_update();
}
