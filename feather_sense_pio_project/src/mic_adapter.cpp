#include "mic.h"

extern bool mic_sensor_init(void);
extern bool readMic(mic_data*);

bool mic_init_adapter(void *ctx){
    (void)ctx; //... does what?
    Serial.println("mic_init_adapter: start");

    return mic_sensor_init();


}

bool mic_read_adapter(void *ctx, sensor_data_t *out){
    (void)ctx;
    // Serial.println("In read adapter\n");
    if(!out) return false;

    mic_data data_struct;
    //Reads IMU data
    if(!readMic(&data_struct)){
        Serial.println("READ MIC failed");
        return false; //return false if error with readIMU()
    }
    //Serial.println("   readIMU success");
    //Serial.printf("YPR_IN yaw: %f, pitch: %f, roll: %f \n", ypr_in.yaw, ypr_in.pitch, ypr_in.roll);

    //Pack up ypr data (type euler_t{ float yaw, float pitch, float roll}) into sensor_data_t
    memcpy(out->bytes, data_struct.buffer, sizeof(mic_data));
    //Serial.printf("OUT yaw: %f, pitch: %f, roll: %f \n", ypr_in.yaw, ypr_in.pitch, ypr_in.roll);

    out->len = 12; //len of data??? Should be 12 (3 floats)
    return true;
}

bool mic_print_adapter(void *ctx, const sensor_data_t *d){
    (void)ctx;
    // Serial.println("In print adapter\n");

    if (!d || d->len < 2){
        //Serial.println("  MIC: (no data)");
        return false;
    }

    mic_data mic_out;

    memcpy(&mic_out.buffer, d, sizeof(mic_data));

    //Print first 5 samples
    for(int i=0; i<5; i++){
       // Serial.printf("Sample %d: %d", i, mic_out.buffer[i]);    
    }

    return true;
}
