

#include "sensor_manager.h"
#include <PDM.h>
#include <arduinoFFT.h>

#include "arm_math.h"
#include <cstdint>
#include <cstdio>
#include <cmath>

// #define PIN_PDM_DIN  11   // example: DATA from mic -> MCU pin 2
// #define PIN_PDM_CLK  6   // MCU drives CLK -> mic CLK
// #define PIN_PDM_PWR  -1   // optional: MCU pin to enable mic Vdd

struct mic_data{
    short buffer[512];
}; 

#define BUFFER_SIZE 32768/8 //32 KB
#define BUFFER_SAMPLES (BUFFER_SIZE / 2)
#define  BUFFER_MASK (BUFFER_SIZE - 1)

extern short ringBuffer[BUFFER_SAMPLES];  // declaration only (no 'static', no definition)

static volatile uint32_t head = 0; // next write index (in bytes or samples - we'll use bytes below)
static volatile uint32_t tail = 0; // next read index

extern bool buffer_full;


extern float filteredBuffer[BUFFER_SIZE];

extern float s1_x1, s1_x2, s1_y1, s1_y2;
extern float s2_x1, s2_x2, s2_y1, s2_y2;

extern float b0, b1, b2;
extern float a1, a2;

float process_two_stage(float in);

void do_fft_on_shorts_inplace(const int16_t *pcm_shorts);