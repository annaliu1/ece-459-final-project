#include "mic.h"


// Buffer to read samples into, each sample is 16-bits
short sampleBuffer[512];

short ringBuffer[BUFFER_SAMPLES];  // the actual definition

// Number of audio samples read
volatile int samplesRead;

// default number of output channels
static const char channels = 1;

// default PCM output frequency
static const int frequency = 16000;

// head/tail in SAMPLES (not bytes)
static volatile uint32_t headSamples = 0; // next write index (in samples)
static volatile uint32_t tailSamples = 0; // next read index (in samples)

// start buffer_full flag at 0
bool buffer_full = 0;

// Helper: convert byte indices to sample indices. We keep indices in bytes for generality.
inline uint32_t byte_to_sample_index(uint32_t byteIndex) {
  return (byteIndex >> 1) & (BUFFER_SAMPLES - 1); // divide by 2, wrap to sample array length
}// Buffer to read samples into, each sample is 16-bits

void ring_write_from_isr(const int16_t* sampleBuf, size_t nsamples) {
  // copy with wrap handling using memcpy for speed
  uint32_t h = headSamples & BUFFER_MASK;
  // how many samples we can place until end of buffer
  uint32_t first = min<uint32_t>(nsamples, BUFFER_SAMPLES - h);

  
  // copy first contiguous chunk
  memcpy((void*)&ringBuffer[h], (const void*)sampleBuf, first * sizeof(int16_t));
  // Serial.printf("Copy into buffer, head at: %d\n", h);
  // if wrapped, copy remainder to start of ringBuffer
  if (first < nsamples) {
    // Serial.println("BUFFER FULL");
    buffer_full = 1;
    memcpy((void*)&ringBuffer[0],
          (const void*)(sampleBuf + first),
          (nsamples - first) * sizeof(int16_t));
  }
  // advance head (keep in samples). This update happens in ISR.
  headSamples = (headSamples + nsamples) & 0xFFFFFFFFu; // 32-bit wrap is fine
}

// Returns true if we copied 'wantSamples' into dest (dest must be int16_t*)
bool ring_pop_samples(int16_t* dest, size_t wantSamples) {
  if (wantSamples == 0) return false;

  // quick check and reserve: do pointer math atomically
  noInterrupts();
  uint32_t h = headSamples; // samples written by ISR
  uint32_t t = tailSamples; // samples read so far
  uint32_t available = (h - t) & BUFFER_SAMPLES; // available samples
  if (available < wantSamples) {
    interrupts();
    return false; // not enough data yet
  }
  uint32_t oldTail = t & BUFFER_SAMPLES;
  // reserve samples by advancing tail
  tailSamples = (tailSamples + wantSamples) & 0xFFFFFFFFu;
  interrupts();

  // copy out — handle possible wrap
  uint32_t first = min<uint32_t>(wantSamples, BUFFER_SAMPLES - oldTail);
  memcpy(dest, (const void*)&ringBuffer[oldTail], first * sizeof(int16_t));
  if (first < wantSamples) {
    memcpy(dest + first, (const void*)&ringBuffer[0], (wantSamples - first) * sizeof(int16_t));
  }
  return true;
}



void onPDMdata() {
    // Query the number of available bytes
    int bytesAvailable = PDM.available();

    if(bytesAvailable > 1024){
      printf("BYTES GREATER THAN 1024!!!!");
    }

    // Read into the sample buffer
    PDM.read(sampleBuffer, bytesAvailable);

    // 16-bit, 2 bytes per sample
    samplesRead = bytesAvailable / 2;
    if(!buffer_full){
      ring_write_from_isr(sampleBuffer, samplesRead);
    }
    else{
      headSamples = 0;
    }
}

bool mic_sensor_init(void) {

  Serial.println("In mic sensor init\n");
  PDM.setPins(11, 6, -1); //set mic pins

  PDM.onReceive(onPDMdata);

  Serial.println("Before PDM begin\n");
  // Do a single quick probe attempt using the library's I2C begin.
  bool found = false;
  found = PDM.begin(channels, frequency);
  Serial.printf("PDM.begin = %d", found);

  Serial.println("After PDM.begin()\n");
  // Release the bus ASAP
  // sensor_bus_unlock();

  if (!found) {
    Serial.println("mic_sensor_init: Failed to find PDM mic. Continuing without mic.");
    return false;
  }

  Serial.println("mic_sensor_init: Mic Found!");

//   // Configure reports — wrap in bus lock because it likely uses I2C internally.
//   if (sensor_bus_lock(pdMS_TO_TICKS(200))) {
//     setReports(reportType, reportIntervalUs);
//     sensor_bus_unlock();
//   } else {
//     Serial.println("imu_sensor_init: could not lock bus to call setReports(); try will occur later in reads.");
//   }
  return true;
}

bool readMic(mic_data* data_struct){
  if (!data_struct->buffer) return false;

  if (samplesRead) {
    memcpy(data_struct->buffer, sampleBuffer, sizeof(sampleBuffer));
    // Print samples to the serial monitor or plotter
    // for (int i = 0; i < samplesRead; i++) {
    //   if (channels == 2) {
    //     Serial.print("L:");
    //     Serial.print(sampleBuffer[i]);
    //     Serial.print(" R:");
    //     i++;
    //   }
    //   //Serial.println(sampleBuffer[i]);
    // }

    // Clear the read count
    samplesRead = 0;

    // if (millis() - timeStart > sampleBuffer[2]) {
    //   digitalWrite(LEDB, state);
    //   state = !state;
    // }
    return true;
  }

  return false; //data was not detected
}