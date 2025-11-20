

#include "sensor_manager.h"
#include <PDM.h>

// #define PIN_PDM_DIN  11   // example: DATA from mic -> MCU pin 2
// #define PIN_PDM_CLK  6   // MCU drives CLK -> mic CLK
// #define PIN_PDM_PWR  -1   // optional: MCU pin to enable mic Vdd

struct mic_data{
    short buffer[512];
}; 

#define BUFFER_SIZE 32768 //32 KB
#define BUFFER_SAMPLES (BUFFER_SIZE / 2)
#define  BUFFER_MASK (BUFFER_SIZE - 1)

extern short ringBuffer[BUFFER_SAMPLES];  // declaration only (no 'static', no definition)

static volatile uint32_t head = 0; // next write index (in bytes or samples - we'll use bytes below)
static volatile uint32_t tail = 0; // next read index