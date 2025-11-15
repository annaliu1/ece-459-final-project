// src/temp_sensor_module.cpp
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_TinyUSB.h>
#include <sensor_manager.h>

// temp_i2c_addr is set by the adapter when a device is discovered.
// Declare it as extern here so this module uses the address found by the adapter.
extern uint8_t temp_i2c_addr;

// fallback default when temp_i2c_addr == 0
static const uint8_t default_temp_addr = 0x48;

void temp_sensor_init(void) {
  // minimal I2C init only (do not probe here)
  Wire.begin();
  // Optionally set I2C clock:
  // Wire.setClock(100000);
}

#include <Wire.h>

uint16_t readTemp(void) {
  // Use configured or default I²C address
  uint8_t addr = (temp_i2c_addr != 0) ? temp_i2c_addr : default_temp_addr;

  // Begin transmission and point to temperature register 0x00
  Wire.beginTransmission(addr);
  Wire.write(0x00);
  uint8_t err = Wire.endTransmission();
  if (err != 0) {
    // 0xFFFF = error sentinel
    return 0xFFFF;
  }

  // Request 2 bytes from the device
  Wire.requestFrom(addr, (uint8_t)2);
  if (Wire.available() < 2) {
    return 0xFFFF;
  }

  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();

  // Combine MSB + LSB into a 16-bit raw value
  uint16_t raw = ((uint16_t)msb << 8) | lsb;
  return raw;
}

