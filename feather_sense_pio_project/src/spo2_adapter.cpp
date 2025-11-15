/* spo2_adapter.cpp
   Adapter: probe I2C for MAX30105, start background SPO2 task, snapshot globals for sensor manager.
   Debug prints are disabled by default (SPO2_DEBUG=0).
*/

#include <Arduino.h>
#include "sensor_manager.h"
#include <Wire.h>
#include "ble_manager.h"


#ifndef SPO2_DEBUG
#define SPO2_DEBUG 1
#endif

// Globals produced by spo2_module.cpp - the adapter reads these.
extern volatile float last_acdc_ir;
extern volatile float last_acdc_red;
extern volatile float last_rawSpO2;
extern volatile float ESpO2;
extern volatile float heartRate;

// Functions owned by module
extern void spo2_sensor_init(void);
extern void create_spo2_task(UBaseType_t prio, uint32_t stack_words);

uint8_t spo2_i2c_addr = 0;
#define SCL 25
#define SDA 26

// Probe a small set of likely addresses only (non-blocking, with recovery)
bool probe_common_addrs_and_record2(void) {
  const uint8_t probe_addrs[] = { 0x57 };

  for (size_t i = 0; i < sizeof(probe_addrs); ++i) {
    uint8_t a = probe_addrs[i];
    if (!sensor_bus_lock(pdMS_TO_TICKS(100))) {
      #if SPO2_DEBUG
      Serial.println("spo2_adapter: probe - failed to lock bus");
      #endif
      delay(20);
      continue;
    }

    Wire.beginTransmission(a);
    uint8_t err = Wire.endTransmission();

    sensor_bus_unlock();
    
    #if SPO2_DEBUG
    Serial.printf("sp02_adapter: probe 0x%02X result=%u\r\n", a, err);
    Serial.flush();
    #endif
    if (err == 0) {
      #if SPO2_DEBUG
      Serial.printf("spo2_adapter: device ACK at 0x%02X\r\n", a);
      #endif
      spo2_i2c_addr = a;
      return true;
    }
    delay(20);
  }
  return false;
}

bool spo2_init_adapter(void *ctx) {
  (void)ctx;
  #if SPO2_DEBUG
  Serial.println("spo2_init_adapter: start (with recovery+targeted probes)");
  #endif

  // Initialize Wire in a portable way
  #if defined(ARDUINO_ARCH_ESP32)
    #if SPO2_DEBUG
    Serial.printf("spo2_init_adapter: Wire.begin(SDA=%d, SCL=%d)\r\n", SDA, SCL);
    #endif
    Wire.begin(SDA, SCL);
  #else
    #if SPO2_DEBUG
    Serial.println("spo2_init_adapter: Wire.begin() default pins");
    #endif
    Wire.begin();
  #endif

  Wire.setClock(100000UL);
  delay(10);

  bool found = probe_common_addrs_and_record2();
  if (!found) {
    #if SPO2_DEBUG
    Serial.println("spo2_adapter: no device found at known addresses (0x57)");
    #endif
  } else {
    #if SPO2_DEBUG
    Serial.printf("spo2_adapter: using addr 0x%02X\n", spo2_i2c_addr);
    #endif
  }

  // call minimal module init (this will call particleSensor.begin/setup)
  spo2_sensor_init();

  // start the continuous SPO2 processing task (background, low priority)
  create_spo2_task(1, 4096);
  #if SPO2_DEBUG
  Serial.println("spo2_init_adapter: create_spo2_task started");
  Serial.println("spo2_init_adapter: done (return true)");
  #endif
  return true;
}

bool spo2_read_adapter(void *ctx, sensor_data_t *out) {
  (void)ctx;
  if (!out) return false;
  memset(out, 0, sizeof(*out));
  if (spo2_i2c_addr == 0) return false; // no known device

  // Snapshot the volatile globals under a critical section to avoid torn reads.
  float ir_acdc_local = 0.0f;
  float red_acdc_local = 0.0f;
  float rawSpO2_local = 0.0f;
  float esp_local = 0.0f;
  float hr_local = 0.0f;

  taskENTER_CRITICAL();
  ir_acdc_local  = last_acdc_ir;
  red_acdc_local = last_acdc_red;
  rawSpO2_local  = last_rawSpO2;
  esp_local      = ESpO2;
  hr_local       = heartRate;
  taskEXIT_CRITICAL();

  // Pack five floats (IR_acdc, RED_acdc, rawSpO2, EstimatedSpO2, heartRate) as IEEE-754 32-bit big-endian.
  size_t p = 0;
  union { float f; uint8_t b[4]; } u;

  u.f = ir_acdc_local;  for (int i = 3; i >= 0; --i) { if (p < SENSOR_DATA_BYTES) out->bytes[p++] = u.b[i]; }
  u.f = red_acdc_local; for (int i = 3; i >= 0; --i) { if (p < SENSOR_DATA_BYTES) out->bytes[p++] = u.b[i]; }
  u.f = rawSpO2_local;  for (int i = 3; i >= 0; --i) { if (p < SENSOR_DATA_BYTES) out->bytes[p++] = u.b[i]; }
  u.f = esp_local;      for (int i = 3; i >= 0; --i) { if (p < SENSOR_DATA_BYTES) out->bytes[p++] = u.b[i]; }
  u.f = hr_local;       for (int i = 3; i >= 0; --i) { if (p < SENSOR_DATA_BYTES) out->bytes[p++] = u.b[i]; }

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
  print_both("  SPO2: ESpO2=%.2f HR=%.1f rawSpO2=%.2f IRacdc=%.2f REDacdc=%.2f\r\n",
                esp, hr, rawSpO2, ir, red);
}
