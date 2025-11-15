#include <Arduino.h>
#include "sensor_manager.h"
#include "ble_manager.h"
#include <bluefruit.h>

// Forward declarations of your adapter functions (must be defined elsewhere in the project)
extern bool temp_init_adapter(void *ctx);
extern bool temp_read_adapter(void *ctx, sensor_data_t *out);
extern void temp_print_adapter(void *ctx, const sensor_data_t *d);

extern bool spo2_init_adapter(void *ctx);
extern bool spo2_read_adapter(void *ctx, sensor_data_t *out);
extern void spo2_print_adapter(void *ctx, const sensor_data_t *d);

extern bool imu_init_adapter(void *ctx);
extern bool imu_read_adapter(void *ctx, sensor_data_t *out);
extern void imu_print_adapter(void *ctx, const sensor_data_t *d);

void setup() {
  Serial.begin(115200);

  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 5000) delay(10);

  // Print the boot banner multiple times so a late-opening monitor has several chances to catch at least one line
    for (int i = 0; i < 3; ++i) {
        Serial.println();
        Serial.println("BOOT: starting...");
        Serial.printf("BOOT: millis=%lu\r\n", millis());
        delay(100);
    }

    Serial.println("starting ble init");
    ble_init();   
    Serial.println("done ble init");

    //Initialize the sensor manager
    Serial.println("Initializing sensor manager...");
    if (!sensor_manager_init()) {
        Serial.println("sensor_manager_init failed!");
        while (1) delay(1000);
    }
    Serial.println("sensor_manager_init OK");

   // Register temperature sensor (uses temp_adapter/temp_sensor_module)
    int temp_idx = sensor_register(
        "temp",
        temp_init_adapter,
        temp_read_adapter,
        temp_print_adapter,
        NULL,
        1,  // frequency in Hz
        true   // start enabled
    );
    Serial.printf("registered sensor temp_idx=%d\r\n", temp_idx);

    // Register SPO2 sensor (uses spo2_adapter/spo2_module)
    int spo2_idx = sensor_register(
        "spo2",
        spo2_init_adapter,
        spo2_read_adapter,
        spo2_print_adapter,
        NULL,
        0.2, // frequency in Hz
        true   // start enabled
    );
    Serial.printf("registered sensor spo2_idx=%d\r\n", spo2_idx);

    // Register IMU sensor (uses imu_adapter/imu_module)
    int imu_idx = sensor_register(
        "imu",
        imu_init_adapter,
        imu_read_adapter,
        imu_print_adapter,
        NULL,
        1, // frequency in Hz
        true   // start enabled
    );
    Serial.printf("registered sensor imu_idx=%d\r\n", imu_idx);

    // Create a periodic print task (every 1 second) for quick feedback
    create_sensor_printer_task(1, 4096, 1000);

    Serial.println("setup() complete, scheduler running...");
}

void loop() {

    // char buf[64];
    // sprintf(buf, "hello");
    // ble_write(buf, 64);

    // This loop runs as the Arduino main/idle task under FreeRTOS.
    // Print a low-frequency alive message so we can see the MCU is still responsive.
    static unsigned long last = 0;
    if (millis() - last >= 5000) {
        last = millis();
        Serial.printf("main loop alive (millis=%lu)\r\n", last);
    }
    // Yield to the RTOS scheduler
    vTaskDelay(pdMS_TO_TICKS(100));
}
