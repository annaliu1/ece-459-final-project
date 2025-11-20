#include <Arduino.h>
#include "sensor_manager.h"
#include <Wire.h>
#include "ble_manager.h"

#ifndef SPO2_DEBUG
#define SPO2_DEBUG 1
#endif

// Globals produced by spo2_module_2.cpp
extern volatile float last_acdc_ir_2;
extern volatile float last_acdc_red_2;
extern volatile float last_rawSpO2_2;
extern volatile float ESpO2_2;
extern volatile float heartRate_2;
extern TwoWire Wire1;  // Use built-in Wire1

// Functions owned by module
extern void spo2_sensor_init_2(void);
extern void create_spo2_task_2(UBaseType_t prio, uint32_t stack_words);

uint8_t spo2_i2c_addr_2 = 0;

bool probe_common_addrs_and_record2_2(void) {
  const uint8_t probe_addrs[] = { 0x57 };

  for (size_t i = 0; i < sizeof(probe_addrs) / sizeof(probe_addrs[0]); ++i) {
    uint8_t a = probe_addrs[i];
    if (!sensor_bus_lock(pdMS_TO_TICKS(100))) {
      #if SPO2_DEBUG
      Serial.println("spo2_adapter_2: probe - failed to lock bus");
      #endif
      delay(20);
      continue;
    }

    Wire1.beginTransmission(a);
    uint8_t err = Wire1.endTransmission();

    sensor_bus_unlock();

    #if SPO2_DEBUG
    Serial.printf("spo2_adapter_2: probe 0x%02X result=%u\r\n", a, err);
    Serial.flush();
    #endif
    if (err == 0) {
      #if SPO2_DEBUG
      Serial.printf("spo2_adapter_2: device ACK at 0x%02X\r\n", a);
      #endif
      spo2_i2c_addr_2 = a;
      return true;
    }
    delay(20);
  }
  return false;
}

bool spo2_init_adapter_2(void *ctx) {
  (void)ctx;
  #if SPO2_DEBUG
  Serial.println("spo2_init_adapter_2: start");
  #endif

  Wire1.setPins(12, 13);  // SDA, SCL
  Wire1.begin();
  Wire1.setClock(100000UL);
  delay(10);

  bool found = probe_common_addrs_and_record2_2();
  if (!found) {
    #if SPO2_DEBUG
    Serial.println("spo2_adapter_2: no device found at known addresses");
    #endif
  } else {
    #if SPO2_DEBUG
    Serial.printf("spo2_adapter_2: using addr 0x%02X\n", spo2_i2c_addr_2);
    #endif
  }

  spo2_sensor_init_2();
  create_spo2_task_2(1, 4096);

  #if SPO2_DEBUG
  Serial.println("spo2_init_adapter_2: done");
  #endif
  return true;
}

bool spo2_read_adapter_2(void *ctx, sensor_data_t *out) {
  (void)ctx;
  if (!out) return false;
  memset(out, 0, sizeof(*out));
  if (spo2_i2c_addr_2 == 0) return false;

  float ir_acdc_local = 0.0f;
  float red_acdc_local = 0.0f;
  float rawSpO2_local = 0.0f;
  float esp_local = 0.0f;
  float hr_local = 0.0f;

  taskENTER_CRITICAL();
  ir_acdc_local  = last_acdc_ir_2;
  red_acdc_local = last_acdc_red_2;
  rawSpO2_local  = last_rawSpO2_2;
  esp_local      = ESpO2_2;
  hr_local       = heartRate_2;
  taskEXIT_CRITICAL();

  size_t p = 0;
  union { float f; uint8_t b[4]; } u;

  u.f = ir_acdc_local;  for (int i = 3; i >= 0; --i) out->bytes[p++] = u.b[i];
  u.f = red_acdc_local; for (int i = 3; i >= 0; --i) out->bytes[p++] = u.b[i];
  u.f = rawSpO2_local;  for (int i = 3; i >= 0; --i) out->bytes[p++] = u.b[i];
  u.f = esp_local;      for (int i = 3; i >= 0; --i) out->bytes[p++] = u.b[i];
  u.f = hr_local;       for (int i = 3; i >= 0; --i) out->bytes[p++] = u.b[i];

  out->len = p;
  return true;
}

void spo2_print_adapter_2(void *ctx, const sensor_data_t *d) {
  (void)ctx;
  if (!d || d->len < 20) {
    Serial.println("  SPO2_2: (no data)");
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
  print_both("  SPO2_2: ESpO2=%.2f HR=%.1f rawSpO2=%.2f IRacdc=%.2f REDacdc=%.2f\r\n",
             esp, hr, rawSpO2, ir, red);
}