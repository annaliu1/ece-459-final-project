#include "imu.h"
#include "sensor_manager.h"
#include <PDM.h>

// Buffer to read samples into, each sample is 16-bits
short sampleBuffer[512];

// Number of audio samples read
volatile int samplesRead;

// default number of output channels
static const char channels = 1;

// default PCM output frequency
static const int frequency = 16000;


void onPDMdata() {
    // Query the number of available bytes
    int bytesAvailable = PDM.available();

    // Read into the sample buffer
    PDM.read(sampleBuffer, bytesAvailable);

    // 16-bit, 2 bytes per sample
    samplesRead = bytesAvailable / 2;
}

void mic_sensor_init(void) {


  PDM.setPins(11, 6, -1); //set mic pins

  PDM.onReceive(onPDMdata);


  // Do a single quick probe attempt using the library's I2C begin.
  bool found = false;
  if (!PDM.begin(channels, frequency)) {
    found = false;
  }
  else{
    found = true;
  }

  // Release the bus ASAP
  sensor_bus_unlock();

  if (!found) {
    Serial.println("mic_sensor_init: Failed to find PDM mic. Continuing without mic.");
    return;
  }

  Serial.println("mic_sensor_init: Mic Found!");

//   // Configure reports — wrap in bus lock because it likely uses I2C internally.
//   if (sensor_bus_lock(pdMS_TO_TICKS(200))) {
//     setReports(reportType, reportIntervalUs);
//     sensor_bus_unlock();
//   } else {
//     Serial.println("imu_sensor_init: could not lock bus to call setReports(); try will occur later in reads.");
//   }
}

// Returns true and fills ypr_in if new data available; false otherwise.
bool readMic(euler_t* ypr_in){
  if (!ypr_in) return false;

  if (samplesRead) {
    memcpy(, sampleBuffer, sizeof(sampleBuffer));
    // Print samples to the serial monitor or plotter
    for (int i = 0; i < samplesRead; i++) {
      if (channels == 2) {
        Serial.print("L:");
        Serial.print(sampleBuffer[i]);
        Serial.print(" R:");
        i++;
      }
      Serial.println(sampleBuffer[i]);
    }

    // Clear the read count
    samplesRead = 0;

    // if (millis() - timeStart > sampleBuffer[2]) {
    //   digitalWrite(LEDB, state);
    //   state = !state;
    // }
  }

  return false; //data was not detected
}