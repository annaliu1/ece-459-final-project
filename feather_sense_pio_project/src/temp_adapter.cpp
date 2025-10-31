// src/temp_adapter.cpp
#include <Arduino.h>
#include "sensor_manager.h"
#include <Wire.h>
#include <Adafruit_TinyUSB.h>

// These are implemented in temp_sensor_module.cpp
extern void temp_sensor_init(void);
extern uint16_t readTemp(void);

// Global set by adapter when it finds the device (0 = unknown)
uint8_t temp_i2c_addr = 0;
#define SCL 3
#define SDA 2

// Bus recovery: toggle SCL a few times and emit a STOP condition
// Uses Arduino's SCL and SDA macros so it matches your board pin mapping.
static void i2c_bus_recover(void) {
  Serial.println("i2c_bus_recover: attempting bus recovery...");
  Serial.printf("i2c_bus_recover: SCL pin=%d SDA pin=%d\r\n", SCL, SDA);

  // Make SCL an output; leave SDA as input with pullup so we can read its state
  pinMode(SCL, OUTPUT);
  pinMode(SDA, INPUT_PULLUP);

  // If SDA is released (high), nothing is stuck — still clock a few pulses to be safe
  for (int i = 0; i < 16; ++i) {
    digitalWrite(SCL, HIGH);
    delayMicroseconds(300);
    digitalWrite(SCL, LOW);
    delayMicroseconds(300);
  }

  // Try to generate a STOP: SDA low -> SCL high -> SDA high
  // Force SDA low briefly then release it and pulse SCL high so slave sees STOP
  pinMode(SDA, OUTPUT);
  digitalWrite(SDA, LOW);
  delayMicroseconds(300);

  digitalWrite(SCL, HIGH);
  delayMicroseconds(300);
  digitalWrite(SDA, HIGH);
  delayMicroseconds(300);

  // Restore pins to inputs pulled-up
  pinMode(SDA, INPUT_PULLUP);
  pinMode(SCL, INPUT_PULLUP);

  // Small delay then re-init Wire
  delay(10);
  Wire.begin(); // re-init I2C master
  Serial.println("i2c_bus_recover: done, Wire.begin() called");
}

// Probe a small set of likely addresses only (non-blocking, with recovery)
bool probe_common_addrs_and_record(void) {
  const uint8_t probe_addrs[] = { 0x48, 0x76, 0x77 };

  // Try a quick recovery first (safe)
  i2c_bus_recover();

  // A short wait for bus settle
  delay(20);

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

  float temp_c = (float)raw / 256.0f; // adjust if needed for your sensor
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
    Serial.println("  Temp: (no data)");
    return;
  }
  int16_t t = (int16_t)((d->bytes[0] << 8) | d->bytes[1]);
  Serial.printf("  Temp: %.2f C\r\n", t / 100.0f);
}
