// src/temp_adapter.cpp
#include <Arduino.h>
#include "sensor_manager.h"
#include <Wire.h>
#include <Adafruit_TinyUSB.h>
#include "ble_manager.h"

// These are implemented in temp_sensor_module.cpp
extern void temp_sensor_init(void);
extern uint16_t readTemp(void);

// Global set by adapter when it finds the device (0 = unknown)
uint8_t temp_i2c_addr = 0;
#define SCL 25
#define SDA 26

// These let you calibrate the observed temperature reading without changing the module.
// Measured_temp_C = ((raw / RAW_DIV) * TEMP_SCALE) + TEMP_OFFSET_C
// Adapter will then pack measured_temp_C as int16_t of (temp_c * 100).
// Default values approximate the previous behavior; tweak as needed.
static const float RAW_DIV = 256.0f;       // existing conversion used in adapter
static const float TEMP_SCALE = 1.0f;      // scale factor (1.0 = no scale)
static const float TEMP_OFFSET_C = -192.0f;   // offset in Celsius to add after scaling

// Probe a small set of likely addresses only (non-blocking, with recovery)
bool probe_common_addrs_and_record(void) {
  const uint8_t probe_addrs[] = { 0x48, 0x76, 0x77 };

  for (size_t i = 0; i < sizeof(probe_addrs); ++i) {
    uint8_t a = probe_addrs[i];
    Serial.printf("temp_adapter: probe 0x%02X ...\r\n", a);
    Serial.flush();
    Wire.beginTransmission(a);
    uint8_t err = Wire.endTransmission();
    Serial.printf("temp_adapter: probe 0x%02X result=%u\r\n", a, err);
    Serial.flush();
    if (err == 0) {
      Serial.printf("temp_adapter: device ACK at 0x%02X\r\n", a);
      temp_i2c_addr = a;
      return true;
    }
    // small pause so we don't hammer the bus
    delay(20);
  }
  return false;
}

bool temp_init_adapter(void *ctx) {
  (void)ctx;
  Serial.println("temp_init_adapter: start (with recovery+targeted probes)");
  Wire.begin(); // ensure Wire exists
  delay(10);

  bool found = probe_common_addrs_and_record();
  if (!found) {
    Serial.println("temp_adapter: no device found at known addresses (0x48/0x76/0x77)");
  } else {
    Serial.printf("temp_adapter: using addr 0x%02X\n", temp_i2c_addr);
  }

  // call minimal module init (should not probe)
  temp_sensor_init();

  Serial.println("temp_init_adapter: done (return true)");
  return true;
}

bool temp_read_adapter(void *ctx, sensor_data_t *out) {
  (void)ctx;
  if (!out) return false;
  memset(out, 0, sizeof(*out));
  if (temp_i2c_addr == 0) return false; // no known device

  uint16_t raw = readTemp();
  if (raw == 0xFFFF) { out->len = 0; return false; }

  // Convert raw -> Celsius using configurable scale/offset:
  float temp_c = ((float)raw) / RAW_DIV;
  temp_c = temp_c * TEMP_SCALE + TEMP_OFFSET_C;

  // Pack as int16_t scaled by 100 (same format temp_print_adapter expects)
  int16_t t_scaled = (int16_t)roundf(temp_c * 100.0f);
  size_t idx = 0;
  out->bytes[idx++] = (uint8_t)((t_scaled >> 8) & 0xFF);
  out->bytes[idx++] = (uint8_t)(t_scaled & 0xFF);
  out->len = idx;
  return true;
}

void temp_print_adapter(void *ctx, const sensor_data_t *d) {
  (void)ctx;
  if (!d || d->len < 2) {
    print_both("  Temp: (no data), ");
    return;
  }
  int16_t t = (int16_t)((d->bytes[0] << 8) | d->bytes[1]);
  print_both("%.2f, ", t / 100.0f);
}
