#include "imu.h"
#include <sensor_manager.h>
#include <string.h> // for memcpy

// Implemented in imu_sensor_module.cpp
extern void imu_sensor_init(void);
extern bool readIMU(euler_t*);

bool imu_init_adapter(void *ctx) {
  (void)ctx;
  Serial.println("imu_init_adapter: start");
  imu_sensor_init();   // void -> no assignment
  return true;         // always create the task; don't block startup
}

bool imu_read_adapter(void *ctx, sensor_data_t *out){
    (void)ctx;

    if(!out) return false;

    euler_t ypr_in;

    // Read IMU data (returns false if no new data or on error)
    if(!readIMU(&ypr_in)){
        return false; // no data this cycle
    }

    // Pack ypr_in into out->bytes (do NOT memcpy into the whole sensor_data_t)
    static_assert(sizeof(euler_t) <= SENSOR_DATA_BYTES, "euler_t too large for sensor_data_t payload");
    memcpy(out->bytes, &ypr_in, sizeof(euler_t));
    out->len = (uint8_t)sizeof(euler_t); // 12 bytes for 3 floats (typical)

    return true;
}

bool imu_print_adapter(void *ctx, const sensor_data_t *d){
    (void)ctx;
    if (!d || d->len < (int)sizeof(euler_t)){
        Serial.println("  IMU: (no data)");
        return false;
    }

    euler_t ypr_out;
    memcpy(&ypr_out, d->bytes, sizeof(euler_t));

    // Print YPR values
    Serial.printf("yaw: %f\t",  ypr_out.yaw);
    Serial.printf("pitch: %f\t", ypr_out.pitch);
    Serial.printf("roll: %f\n",  ypr_out.roll);

    return true;
}