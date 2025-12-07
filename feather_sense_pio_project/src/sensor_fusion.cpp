#include "spo2_fusion.h"
#include "sensor_manager.h"
#include <Arduino.h>
#include "ble_manager.h"

// External globals from both modules
extern volatile float ESpO2;
extern volatile float heartRate;
extern volatile float last_acdc_ir;
extern volatile float last_acdc_red;

extern volatile float ESpO2_2;
extern volatile float heartRate_2;
extern volatile float last_acdc_ir_2;
extern volatile float last_acdc_red_2;

#ifndef SPO2_DEBUG
#define SPO2_DEBUG 1
#endif

static float compute_confidence(float acdc_ir, float acdc_red, float spo2) {
  // if (acdc_ir < 0.02f || acdc_red < 0.02f) return 0.0f;
  // if (spo2 < 50.0f || spo2 > 100.0f) return 0.0f;

  float signal_quality = fminf(acdc_ir, acdc_red);
  float range_penalty = fabsf(spo2 - 98.0f) / 50.0f;
  float confidence = signal_quality * (1.0f - range_penalty);
  return fmaxf(0.0f, fminf(confidence, 1.0f));
}

bool spo2_init_fusion_adapter(void *ctx) {
  (void)ctx;
  #if SPO2_DEBUG
  Serial.println("spo2_fusion_adapter: initialized");
  #endif
  return true;
}

bool spo2_read_fusion_adapter(void *ctx, sensor_data_t *out) {
  (void)ctx;
  if (!out) return false;
  memset(out, 0, sizeof(*out));

  float spo2_1, hr_1, acdc_ir_1, acdc_red_1;
  float spo2_2, hr_2, acdc_ir_2, acdc_red_2;

  taskENTER_CRITICAL();
  spo2_1 = ESpO2;
  hr_1 = heartRate;
  acdc_ir_1 = last_acdc_ir;
  acdc_red_1 = last_acdc_red;

  spo2_2 = ESpO2_2;
  hr_2 = heartRate_2;
  acdc_ir_2 = last_acdc_ir_2;
  acdc_red_2 = last_acdc_red_2;
  taskEXIT_CRITICAL();

  float conf1 = compute_confidence(acdc_ir_1, acdc_red_1, spo2_1);
  float conf2 = compute_confidence(acdc_ir_2, acdc_red_2, spo2_2);

  float fused_spo2 = 0.0f;
  float fused_hr = 0.0f;

  //Serial.printf("conf1: %f, conf2: %f\n", conf1, conf2);

  if (conf1 > 0.8f && conf2 > 0.8f) {
    fused_spo2 = 0.5f * (spo2_1 + spo2_2);
    fused_hr = 0.5f * (hr_1 + hr_2);
  } else if (conf1 > conf2 && conf1 > 0.5f) {
    fused_spo2 = spo2_1;
    fused_hr = hr_1;
  } else if (conf2 > 0.5f) {
    fused_spo2 = spo2_2;
    fused_hr = hr_2;
  } else {
    return false;  // no reliable data
  }

  union { float f; uint8_t b[4]; } u;
  size_t p = 0;
  u.f = fused_spo2; for (int i = 3; i >= 0; --i) out->bytes[p++] = u.b[i];
  u.f = fused_hr;   for (int i = 3; i >= 0; --i) out->bytes[p++] = u.b[i];
  out->len = p;

  #if SPO2_DEBUG
  //Serial.printf("FUSION: conf1=%.2f conf2=%.2f → SpO2=%.1f HR=%.1f\n", conf1, conf2, fused_spo2, fused_hr);
  #endif

  return true;
}

void spo2_print_fusion_adapter(void *ctx, const sensor_data_t *d) {
  (void)ctx;
//   if (!d || d->len < 8) {
//     Serial.println("  SPO2_FUSION: (no data)");
//     return;
//   }
  auto getf = [&](int offs)->float {
    union { float f; uint8_t b[4]; } u;
    for (int i = 0; i < 4; ++i) u.b[3 - i] = d->bytes[offs + i];
    return u.f;
  };
  float spo2 = getf(0);
  float hr = getf(4);
  // print_both("HR: %.1f\n", hr);
  // print_both("SPO2: %.2f\n", spo2);

  print_both("%.1f, ", hr);
  print_both("%.2f, ", spo2);
  
}