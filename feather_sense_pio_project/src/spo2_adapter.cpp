// src/spo2_adapter.cpp
//
// SPO2 adapter: initialize bus, probe for MAX30105, and start the spo2 processing task.
// The spo2_module.cpp file owns the actual MAX30105 instance and does particleSensor.begin()
// inside its setup; the adapter just ensures the bus is up and the SPO2 task is started.

#include <Arduino.h>
#include "sensor_manager.h"
#include <Wire.h>

// Globals produced by spo2_module.cpp - the adapter reads these.
extern float last_acdc_ir;
extern float last_acdc_red;
extern float last_rawSpO2;
extern float ESpO2;
extern float heartRate;

// Forward declaration: spo2_module provides this to create its FreeRTOS task.
extern void create_spo2_task(UBaseType_t priority, uint16_t stack_words);

// Track whether we saw the MAX30105 on the bus (0 = unknown / not found)
static uint8_t spo2_i2c_found = 0;
static const uint8_t MAX30105_ADDR = 0x57; // typical address for MAX30105

// Probe a single address with small delay/retry (non-blocking)
static bool probe_addr_once(uint8_t addr) {
  Wire.beginTransmission(addr);
  uint8_t err = Wire.endTransmission();
  return (err == 0);
}

bool spo2_init_adapter(void *ctx) {
  (void)ctx;
  Serial.println("spo2_init_adapter: start");

  // Ensure Wire is started (idempotent)
  Wire.begin();
  delay(10);

  // probe the usual MAX30105 address
  Serial.printf("spo2_init_adapter: probing 0x%02X ...\r\n", MAX30105_ADDR);
  bool found = probe_addr_once(MAX30105_ADDR);
  spo2_i2c_found = found ? 1 : 0;
  Serial.printf("spo2_init_adapter: probe result=%u\r\n", (unsigned)found);

  // Start the spo2 processing task (the module will perform particleSensor.begin() itself)
  Serial.println("spo2_init_adapter: creating spo2 task...");
  create_spo2_task(2, 4096);

  Serial.println("spo2_init_adapter: done");
  return true;
}

bool spo2_read_adapter(void *ctx, sensor_data_t *out) {
  (void)ctx;
  if (!out) return false;
  memset(out, 0, sizeof(*out));

  // If we never saw the device on the bus, return no-data
  if (!spo2_i2c_found) return false;

  // Don't publish until module has written a window (avoid showing zeros)
  // last_rawSpO2 starts at 0.0; wait until it's nonzero (or negative sentinel)
  if (last_rawSpO2 == 0.0f) {
    return false;
  }

  // Pack five floats (IR_acdc, RED_acdc, rawSpO2, EstimatedSpO2, heartRate) as IEEE-754 32-bit big-endian.
  size_t p = 0;
  union { float f; uint8_t b[4]; } u;

  u.f = last_acdc_ir;  for (int i = 3; i >= 0; --i) { if (p < SENSOR_DATA_BYTES) out->bytes[p++] = u.b[i]; }
  u.f = last_acdc_red; for (int i = 3; i >= 0; --i) { if (p < SENSOR_DATA_BYTES) out->bytes[p++] = u.b[i]; }
  u.f = last_rawSpO2;  for (int i = 3; i >= 0; --i) { if (p < SENSOR_DATA_BYTES) out->bytes[p++] = u.b[i]; }
  u.f = ESpO2;         for (int i = 3; i >= 0; --i) { if (p < SENSOR_DATA_BYTES) out->bytes[p++] = u.b[i]; }
  u.f = heartRate;     for (int i = 3; i >= 0; --i) { if (p < SENSOR_DATA_BYTES) out->bytes[p++] = u.b[i]; }

  out->len = p;
  return true;
}

void spo2_print_adapter(void *ctx, const sensor_data_t *d) {
  (void)ctx;
  if (!d || d->len < 20) {
    Serial.println("  SPO2: (no data)");
    return;
  }
  auto getf = [&](int offs)->float {
    union { float f; uint8_t b[4]; } u;
    for (int i = 0; i < 4; ++i) u.b[3 - i] = d->bytes[offs + i];
    return u.f;
  };
  float ir = getf(0);
  float red = getf(4);
  float rawSpO2 = getf(8);
  float esp = getf(12);
  float hr = getf(16);
  Serial.printf("  SPO2: ESpO2=%.2f HR=%.1f rawSpO2=%.2f IRacdc=%.2f REDacdc=%.2f\r\n",
                esp, hr, rawSpO2, ir, red);
}
