#include "imu.h"
#include "ble_manager.h"

// Implemented in imu_sensor_module.cpp
extern void imu_sensor_init(void);
extern bool readIMU(euler_t*);


bool imu_init_adapter(void *ctx){
    (void)ctx; //... does what?
    Serial.println("imu_init_adapter: start");

    imu_sensor_init();
    return true;
}

bool imu_read_adapter(void *ctx, sensor_data_t *out){
    (void)ctx;

    if(!out) return false;

    euler_t ypr_in;

    //Reads IMU data
    if(!readIMU(&ypr_in)){
        //Serial.println("  readIMU failed");
        return false; //return false if error with readIMU()
    }

    //Pack up ypr data (type euler_t{ float yaw, float pitch, float roll}) into sensor_data_t
    memcpy(out, &ypr_in, sizeof(euler_t));

    out->len = 12; //len of data??? Should be 12 (3 floats)
    return true;
}

bool imu_print_adapter(void *ctx, const sensor_data_t *d)
{
    (void)ctx;

    if (!d || d->len < 2) {
        print_both("  IMU: (no data)\r\n");
        return false;
    }

    euler_t ypr_out;
    memcpy(&ypr_out, d, sizeof(euler_t));   // keep your original behavior

    // Prints everything on one line (tab-delimited)
    //print_both("status: %d\t", sensorValue.status);   // optional
    // print_both("yaw: %f\t",   ypr_out.yaw);
    // print_both("pitch: %f\t", ypr_out.pitch);
    // print_both("roll: %f\r\n", ypr_out.roll);

    // Classify position
    if (ypr_out.roll <= -90.f) {
        print_both("extreme right, 3\r\n");
    } else if (ypr_out.roll > -90.f && ypr_out.roll <= -10.f) {
        print_both("medium right, 3\r\n");
    } else if (ypr_out.roll > -10.f && ypr_out.roll <= 10.f) {
        print_both("relatively up, 3\r\n");
    } else if (ypr_out.roll > 10.f && ypr_out.roll < 90.f) {
        print_both("medium left, 3\r\n");
    } else {
        print_both("extreme left, 3\r\n");
    }

    return true;
}
