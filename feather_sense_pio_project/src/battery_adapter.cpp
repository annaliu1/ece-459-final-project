#include <string.h>
#include <stdio.h>
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include "ble_manager.h"
#include "sensor_manager.h"

#define VBATPIN A6

static TickType_t battery_period = pdMS_TO_TICKS(2000);
static int percent;
static int prev_percent;

bool battery_init_adapter(void *pv) {
  (void)pv;
  return true;
}

bool battery_read_adapter(void *pv, sensor_data_t *out) {
    (void)pv;

    if (!out) return false;
    memset(out, 0, sizeof(*out));

    // Read VBAT
    float measuredvbat = analogRead(VBATPIN);
    measuredvbat *= 2;      // voltage divider compensation
    measuredvbat *= 3.6;    // reference voltage
    measuredvbat /= 1024;   // to volts

    uint8_t percent = 0;

    // Simple LiPo curve approximation
    if (measuredvbat >= 3.75f && measuredvbat < 4.2f) {
        percent = 80 + (uint8_t)((measuredvbat - 3.75f) * (20.0f / 0.45f));
    }
    else if (measuredvbat >= 3.5f && measuredvbat < 3.75f) {
        percent = 20 + (uint8_t)((measuredvbat - 3.5f) * (60.0f / 0.25f));
    }
    else if (measuredvbat >= 3.2f && measuredvbat < 3.5f) {
        percent = (uint8_t)((measuredvbat - 3.2f) * (20.0f / 0.3f));
    }
    else {
        percent = 0;
    }

    // Clamp 0–100%
    percent = (percent > 100) ? 100 : percent;

    // Pack EXACTLY like the temp sensor: into bytes[]
    out->bytes[0] = percent;
    out->len = 1;   // one byte

    return true;
}


void battery_print_adapter(void *pv, const sensor_data_t *d) {
    (void)pv;

    if (!d || d->len < 1) {
        print_both("Battery: (no data), ");
        return;
    }

    uint8_t percent = d->bytes[0];

    // Prevent rising % (your original behavior)
    static uint8_t prev_percent = 100;
    uint8_t finalPercent = (percent > prev_percent) ? prev_percent : percent;

    print_both(", %u \n", finalPercent);

    prev_percent = finalPercent;
}

