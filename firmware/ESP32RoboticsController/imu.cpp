#include "imu.h"

// =============================================================================
// MPU-6050 driver — raw I2C register reads, no external library required.
// Supports both 0x68 (AD0=LOW) and 0x69 (AD0=HIGH) addresses.
// =============================================================================

static uint8_t _addr = 0x68;

// Write one register byte; returns true on success.
static bool _wreg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

bool imu_init() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_CLOCK_HZ);

    // Probe 0x68 then 0x69
    for (uint8_t a = 0x68; a <= 0x69; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            _addr = a;
            Serial.printf("[imu] MPU-6050 at 0x%02X\n", a);
            goto found;
        }
    }
    Serial.println("[imu] no device found");
    return false;

found:
    _wreg(0x6B, 0x00);   // PWR_MGMT_1: clear sleep bit, use internal 8MHz osc
    delay(100);
    _wreg(0x19, 0x07);   // SMPLRT_DIV: 125 Hz output rate
    _wreg(0x1A, 0x03);   // CONFIG: DLPF 44 Hz (reduces vibration noise)
    _wreg(0x1B, 0x00);   // GYRO_CONFIG:  ±250 °/s
    _wreg(0x1C, 0x00);   // ACCEL_CONFIG: ±2 g
    return true;
}

bool imu_read(ImuData &data) {
    // Burst-read 14 bytes starting at ACCEL_XOUT_H (0x3B)
    Wire.beginTransmission(_addr);
    Wire.write(0x3B);
    if (Wire.endTransmission(false) != 0) return false;

    uint8_t n = Wire.requestFrom(_addr, (uint8_t)14);
    if (n < 14) return false;

    auto rd16 = []() -> int16_t {
        return (int16_t)((Wire.read() << 8) | Wire.read());
    };

    int16_t ax = rd16(), ay = rd16(), az = rd16();
    int16_t tp = rd16();
    int16_t gx = rd16(), gy = rd16(), gz = rd16();

    // ±2 g range → 16384 LSB/g → convert to m/s²
    constexpr float A = 9.81f / 16384.0f;
    // ±250 °/s range → 131 LSB/(°/s)
    constexpr float G = 1.0f / 131.0f;

    data.accel_x = ax * A;
    data.accel_y = ay * A;
    data.accel_z = az * A;
    data.gyro_x  = gx * G;
    data.gyro_y  = gy * G;
    data.gyro_z  = gz * G;
    data.temp_c  = tp / 340.0f + 36.53f;
    return true;
}
