#include <Arduino.h>
#include "sensor_manager.h"
#include "ble_manager.h"
#include <bluefruit.h>
#include "storage.h"

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

// Called on BLE central connect
void my_connect_cb(uint16_t conn_handle) {
  (void)conn_handle;
  Serial.println("BLE connected -> starting storage upload task");
  // Kick upload in a short task to avoid blocking the BLE callback
  xTaskCreate([](void *pv) {
      (void)pv;
      storage_upload_over_ble();
      vTaskDelete(NULL);
  }, "stor-up", 8192, NULL, 2, NULL);
}

void my_disconnect_cb(uint16_t conn_handle, uint8_t reason) {
  (void)conn_handle; (void)reason;
  Serial.println("BLE disconnected");
}


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

    // BLE callbacks
    Bluefruit.Periph.setConnectCallback(my_connect_cb);
    Bluefruit.Periph.setDisconnectCallback(my_disconnect_cb);

    // Initialize storage (flush every 60s, 4KB RAM buffer)
    if (!storage_init(60*1000, 4*1024)) {
        Serial.println("storage_init failed!");
        // non-fatal for demo: continue but no persistent logs will be saved
    } else {
        Serial.println("storage_init OK");
    }

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
