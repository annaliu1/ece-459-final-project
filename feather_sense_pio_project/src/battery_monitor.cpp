#include <string.h>
#include <stdio.h>
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include "ble_manager.h"

#define VBATPIN A6
// #define VBAT_MV_PER_LSB   (0.87890625)   // 3.0V ADC range and 12-bit ADC resolution = 3000mV/4096

// #ifdef NRF52840_XXAA
// #define VBAT_DIVIDER      (0.5F)          // 150K + 150K voltage divider on VBAT
// #define VBAT_DIVIDER_COMP (2.0F)          // Compensation factor for the VBAT divider
// #else
// #define VBAT_DIVIDER      (0.71275837F)   // 2M + 0.806M voltage divider on VBAT = (2M / (0.806M + 2M))
// #define VBAT_DIVIDER_COMP (1.403F)        // Compensation factor for the VBAT divider
// #endif

// #define REAL_VBAT_MV_PER_LSB (VBAT_DIVIDER_COMP * VBAT_MV_PER_LSB)

// float readVBAT(void) {
//   float raw;

//   // Set the analog reference to 3.0V (default = 3.6V)
//   analogReference(AR_INTERNAL_3_0);

//   // Set the resolution to 12-bit (0..4095)
//   analogReadResolution(12); // Can be 8, 10, 12 or 14

//   // Let the ADC settle
//   delay(1);

//   // Get the raw 12-bit, 0..3000mV ADC value
//   raw = analogRead(VBATPIN);

//   // Set the ADC back to the default settings
//   analogReference(AR_DEFAULT);
//   analogReadResolution(10);

//   // Convert the raw value to compensated mv, taking the resistor-
//   // divider into account (providing the actual LIPO voltage)
//   // ADC range is 0..3000mV and resolution is 12-bit (0..4095)
//   return raw * REAL_VBAT_MV_PER_LSB;
// }

// uint8_t mvToPercent(float mvolts) {
//   if(mvolts<3300)
//     return 0;

//   if(mvolts <3600) {
//     mvolts -= 3300;
//     return mvolts/30;
//   }

//   mvolts -= 3600;
//   return 10 + (mvolts * 0.15F );  // thats mvolts /6.66666666
// }

static TickType_t battery_period = pdMS_TO_TICKS(2000);
extern void print_both(const char *fmt, ...);

static void battery_monitor_task(void *pv) {
    (void)pv;

    int prev_percent = 100;
    int percent = 0;

    for (;;) {
        // Get a raw ADC reading
        // float vbat_mv = readVBAT();
        // print_both("vbat in mv: %d\n", vbat_mv);

        // Convert from raw mv to percentage (based on LIPO chemistry)
        // uint8_t vbat_per = mvToPercent(vbat_mv);
        float measuredvbat = analogRead(VBATPIN);
        measuredvbat *= 2;    // we divided by 2, so multiply back
        measuredvbat *= 3.6;  // Multiply by 3.6V, our reference voltage
        measuredvbat /= 1024; // convert to voltage

        //4.2 to 3.75 100 - 80
        //3.75 to 3.5 is 80-20
        //3.5 to 3.0 is 20 to 0

        if (measuredvbat < 4.2 && measuredvbat >= 3.75)
        {
            percent = 80 + (int)((measuredvbat - 3.75) * (20.0 / 0.45));

        }
        else if (measuredvbat < 3.75 && measuredvbat >= 3.5)
        {
            percent = 20 + int((measuredvbat - 3.5) * (60.0 / 0.25));

        }
        else if (measuredvbat < 3.5 && measuredvbat >= 3.2)
        {
            percent = int((measuredvbat - 3.2) * (20.0 / 0.3));
        }
        else percent = 0;

        int finalPercent = (percent > prev_percent) ? prev_percent : percent;
        print_both("vbat %d\n", finalPercent);

        prev_percent = finalPercent;
    
        vTaskDelay(battery_period);
    }
}

BaseType_t create_battery_monitor_task(UBaseType_t priority, uint16_t stack_words, TickType_t period_ms)
{
    return xTaskCreate(battery_monitor_task, "bat", stack_words ? stack_words : 4096, NULL, priority ? priority : 1, NULL);
}