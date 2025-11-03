/*
  SPO2 adapter: integrates MAX3010x SPO2 reading code with sensor_manager
  Drop this file into your project (e.g. src/spo2_adapter.cpp).
  It exposes three functions the sensor manager expects:
    - bool spo2_init_adapter(void *ctx);
    - bool spo2_read_adapter(void *ctx, sensor_data_t *out);
    - void spo2_print_adapter(void *ctx, const sensor_data_t *d);
  This file assumes your existing SPO2 code produces the following globals (adjust names if different):
    float last_acdc_ir, last_acdc_red, last_rawSpO2, ESpO2, heartRate;
  If your project uses different variable names, either change the externs below or assign values into these names
  in your SPO2 processing loop.
*/

#include <Arduino.h>
#include "sensor_manager.h" // expects this to be available in include/ or project include path
#include <Adafruit_TinyUSB.h>
#include <Wire.h>
#include "MAX30105.h"

MAX30105 particleSensor2;

// If your existing SPO2 sketch defines these, we reference them here. If not, define them in your SPO2 code.
extern float last_acdc_ir;
extern float last_acdc_red;
extern float last_rawSpO2;
extern float ESpO2;
extern float heartRate;

// init adapter: no-op if the sensor is already initialized elsewhere in your sketch
bool spo2_init_adapter(void *ctx) {
    (void)ctx;

    Wire.begin();

    if (!particleSensor2.begin(Wire, I2C_SPEED_STANDARD)) {
        Serial.println("MAX30105 not found");
        return false;
    }

    // Optional: set up default configuration
    particleSensor2.setup();  // Configure sensor with default parameters
    Serial.println("MAX30105 initialized successfully");
    return true;
}

// read adapter: pack recent computed SPO2 metrics into sensor_data_t
bool spo2_read_adapter(void *ctx, sensor_data_t *out) {
    (void)ctx;
    if (!out) return false;
    memset(out, 0, sizeof(*out));

    // If your SPO2 code computes values only inside loop windows, ensure those globals are updated prior to this call.
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

// print adapter: human readable print via Serial
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
