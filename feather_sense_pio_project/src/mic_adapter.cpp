#include "mic.h"

extern bool mic_sensor_init(void);
extern bool readMic(mic_data*);

// fft_nrf52840_cmsis.cpp
// Requires CMSIS-DSP (arm_math.h) and correct FPU/toolchain flags.

#include "arm_math.h"
#include <cstdint>
#include <cstdio>
#include <cmath>

constexpr uint32_t N = 16384u/8;
constexpr float SAMPLE_RATE = 16128.0f;

// You can choose to allocate these statically (recommended for embedded)
static float input_f32[N];      // converted samples (len N)
static float mag[N/2];          // magnitudes (len N/2)
static arm_rfft_fast_instance_f32 rfft_inst;

// apply Hann window (simple)
static inline void apply_hann_window_f(float *buf, uint32_t len) {
    if (len < 2) return;
    for (uint32_t n = 0; n < len; ++n) {
        // Use arm_cos_f32 for potential FPU speed
        float w = 0.5f * (1.0f - arm_cos_f32(2.0f * PI * (float)n / (float)(len - 1)));
        buf[n] *= w;
    }
}

// reuse input_f32 for output as well (in-place)
void do_fft_on_shorts_inplace(const int16_t *pcm_shorts) {
    // 1) convert to float & normalize into input_f32
    for (uint32_t i = 0; i < N; ++i) {
        input_f32[i] = (float)pcm_shorts[i] / 32768.0f;
    }

    // 2) window
    apply_hann_window_f(input_f32, N);

    // 3) init rfft (do once ideally)
    if (arm_rfft_fast_init_f32(&rfft_inst, N) != ARM_MATH_SUCCESS) {
        printf("RFFT init failed\n");
        return;
    }

    // 4) compute in-place: input_f32 -> input_f32 (now holds interleaved complex)
    arm_rfft_fast_f32(&rfft_inst, input_f32, input_f32, 0);

    // 5) compute magnitudes: input_f32 is interleaved complex
    arm_cmplx_mag_f32(input_f32, mag, N / 2);

    // 6) normalize
    float invN = 1.0f / (float)N;
    for (uint32_t k = 0; k < N/2; ++k) mag[k] *= invN;

    const float MIN_FREQ = 700.0f;
    const float MAX_FREQ = 900.0f;
    const float MIN_MAG  = 0.000008f;
    const float MAX_MAG  = 0.00005f;

    // Compute bin range (do once)
    uint32_t k_start = (uint32_t)ceilf((MIN_FREQ * N) / SAMPLE_RATE);
    uint32_t k_end   = (uint32_t)floorf((MAX_FREQ * N) / SAMPLE_RATE);
    if (k_end >= N/2) k_end = N/2 - 1;
    if (k_start > k_end) {
        printf("No frequency detected (band out of range)\n");
        return;
    }

    // Find the strongest bin that also lies inside the magnitude bounds
    float best_mag = 0.0f;
    uint32_t best_k = 0;
    bool found = false;

    for (uint32_t k = k_start; k <= k_end; ++k) {
        float m = mag[k];
        if (m >= MIN_MAG && m <= MAX_MAG) {
            if (!found || m > best_mag) {
                best_mag = m;
                best_k = k;
                found = true;
            }
        }
    }

    if (found) {
        float freq = (float)best_k * SAMPLE_RATE / (float)N;
        printf("Detected freq in band with magnitude bounds: %.2f Hz, mag=%.6f (bin %u)\n",
            freq, best_mag, (unsigned)best_k);
    }
    // else {
    //     printf("No frequency detected\n");
    // }

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
    // printf("IN PRINT ADAPTER\n");
    mic_data mic_out;

    memcpy(&mic_out.buffer, d, sizeof(mic_data));
    
    if(buffer_full == 1){
        // int snore_val_count = 0;      

        // for(int i=0; i<BUFFER_SAMPLES; i++){
        //     if(ringBuffer[i] > 20){
        //         snore_val_count++;
        //         printf("snore_val_count: %d\n", snore_val_count);
        //     }
        // }

        // if(snore_val_count > 10){
        //     printf("SNORE DETECTED\n");
        // }

        do_fft_on_shorts_inplace(ringBuffer);

        memset(&ringBuffer, 0, sizeof(ringBuffer));
        buffer_full = 0;
    }

    //Print first 5 samples
    for(int i=0; i<5; i++){
       // Serial.printf("Sample %d: %d", i, mic_out.buffer[i]);    
    }



    return true;
}
