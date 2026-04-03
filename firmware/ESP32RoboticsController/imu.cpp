#include "imu.h"

// =============================================================================
// Placeholder IMU implementation.
// Replace the body of imu_init() and imu_read() with calls to your chosen
// IMU library once you have identified the specific sensor on your board.
// =============================================================================

bool imu_init() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_CLOCK_HZ);

    // TODO: Add IMU library init, e.g. for MPU-6050:
    // mpu.initialize();
    // return mpu.testConnection();

    // Probe the I2C bus to verify a device is present
    Wire.beginTransmission(0x68); // Common MPU-6050 address
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
        Serial.println("[imu] device found at 0x68");
        return true;
    }

    Wire.beginTransmission(0x69); // Alternate address
    error = Wire.endTransmission();
    if (error == 0) {
        Serial.println("[imu] device found at 0x69");
        return true;
    }

    Serial.println("[imu] no device found — check wiring and address");
    return false;
}

bool imu_read(ImuData &data) {
    // TODO: Replace with library read calls, e.g.:
    // mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    // data.accel_x = ax / 16384.0f * 9.81f;
    // ...

    // Zeroed placeholder until driver is wired up
    data = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    return false;
}
