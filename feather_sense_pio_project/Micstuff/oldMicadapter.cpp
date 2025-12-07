#include "mic.h"

extern bool mic_sensor_init(void);
extern bool readMic(mic_data*);

float filteredBuffer[BUFFER_SIZE];

float s1_x1=0, s1_x2=0, s1_y1=0, s1_y2=0;
float s2_x1=0, s2_x2=0, s2_y1=0, s2_y2=0;

float b0 = 0.07213175337228960; float b1 = 0.00000000000000000; float b2 = -0.07213175337228960;
float a1 = -1.8430765105656570; float a2 = 0.8557364932554208;

float process_two_stage(float in) {
    // stage 1
    float out1 = b0*in + b1*s1_x1 + b2*s1_x2 - a1*s1_y1 - a2*s1_y2;
    s1_x2 = s1_x1; s1_x1 = in;
    s1_y2 = s1_y1; s1_y1 = out1;

    // stage 2
    float out2 = b0*out1 + b1*s2_x1 + b2*s2_x2 - a1*s2_y1 - a2*s2_y2;
    s2_x2 = s2_x1; s2_x1 = out1;
    s2_y2 = s2_y1; s2_y1 = out2;

    return out2;
}

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
    printf("IN PRINT ADAPTER\n");
    mic_data mic_out;

    memcpy(&mic_out.buffer, d, sizeof(mic_data));
    
    if(buffer_full == 1){
        int snore_val_count = 0;
        for(int i=0; i<BUFFER_SAMPLES; i++){
            float in = (float) ringBuffer[i]; 
            filteredBuffer[i] = process_two_stage(in);
        }

        for(int i=0; i<BUFFER_SAMPLES; i++){
            if(filteredBuffer[i] > 20){
                snore_val_count++;
                printf("snore_val_count: %d\n", snore_val_count);
            }
        }


        // for(int i=0; i<BUFFER_SAMPLES; i++){
        //     if(ringBuffer[i] > 20){
        //         snore_val_count++;
        //         printf("snore_val_count: %d\n", snore_val_count);
        //     }
        // }

        if(snore_val_count > 10){
            printf("SNORE DETECTED\n");
        }
        memset(&ringBuffer, 0, sizeof(ringBuffer));
        buffer_full = 0;
    }

    //Print first 5 samples
    for(int i=0; i<5; i++){
       // Serial.printf("Sample %d: %d", i, mic_out.buffer[i]);    
    }



    return true;
}
