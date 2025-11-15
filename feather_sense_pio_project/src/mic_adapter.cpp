#include "imu.h"


bool mic_init_adapter(void *ctx){
    (void)ctx; //... does what?
    Serial.println("imu_init_adapter: start");

    imu_sensor_init();

}

bool mic_read_adapter(void *ctx, sensor_data_t *out){
    (void)ctx;

    if(!out) return false;

    euler_t ypr_in;

    //Reads IMU data
    if(!readIMU(&ypr_in)){
        //Serial.println("  readIMU failed");
        return false; //return false if error with readIMU()
    }
    //Serial.println("   readIMU success");
    //Serial.printf("YPR_IN yaw: %f, pitch: %f, roll: %f \n", ypr_in.yaw, ypr_in.pitch, ypr_in.roll);

    //Pack up ypr data (type euler_t{ float yaw, float pitch, float roll}) into sensor_data_t
    memcpy(out, &ypr_in, sizeof(euler_t));
    //Serial.printf("OUT yaw: %f, pitch: %f, roll: %f \n", ypr_in.yaw, ypr_in.pitch, ypr_in.roll);

    out->len = 12; //len of data??? Should be 12 (3 floats)
    return true;
}

bool mic_print_adapter(void *ctx, const sensor_data_t *d){
    (void)ctx;
    if (!d || d->len < 2){
        Serial.println("  IMU: (no data)");
        return false;
    }

    euler_t ypr_out;

    memcpy(&ypr_out, d, sizeof(euler_t));

    //Prints everything on one line (tab-delimited)
    //Serial.printf("status: %d", sensorValue.status);     Serial.print("\t");  // This is accuracy in the range of 0 to 3 (shoud implement similar to ypr so must pack into d)
    Serial.printf("yaw: %f", ypr_out.yaw);                Serial.print("\t");
    Serial.printf("pitch: %f", ypr_out.pitch);              Serial.print("\t");
    Serial.printf("roll: %f", ypr_out.roll);                Serial.print("\n");


    //Classify position
    if(ypr_out.roll <= -90.f){
        Serial.printf("extreme right\n");
    }
    else if(ypr_out.roll > -90.f && ypr_out.roll <= -30.f){
        Serial.printf("medium right\n");
    }
    else if(ypr_out.roll > -30.f && ypr_out.roll <= 30.f){
        Serial.printf("relatively up\n");
    }
    else if(ypr_out.roll > 30.f && ypr_out.roll < 90.f){
        Serial.printf("medium left\n");
    }
    else{
        Serial.printf("extreme left\n");
    }

    return true;
}
