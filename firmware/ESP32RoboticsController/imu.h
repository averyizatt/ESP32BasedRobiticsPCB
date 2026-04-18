#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// =============================================================================
// IMU Interface — I2C
// =============================================================================
// Wraps I2C communication for an onboard IMU (e.g., MPU-6050, ICM-42688).
// Replace the driver calls below with the library matching your IMU.
//
// Common libraries:
//   MPU-6050:   "MPU6050" by Electronic Cats, or "I2Cdevlib-MPU6050"
//   ICM-42688:  SparkFun ICM-42688-P Arduino Library
//   Generic:    Wire.h with manual register reads
// =============================================================================

struct ImuData {
    float accel_x;   // m/s²
    float accel_y;
    float accel_z;
    float gyro_x;    // deg/s
    float gyro_y;
    float gyro_z;
    float temp_c;    // °C (if available)
};

// Initialise I2C bus and IMU. Returns true on success.
bool imu_init();

// Read latest sensor values. Returns false if the I2C read failed.
bool imu_read(ImuData &data);

// Read the latest sensor data into the provided struct.
// Returns true if data was successfully retrieved.
bool imu_read(ImuData &data);
