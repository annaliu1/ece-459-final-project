// /* Converted SPO2 module: provides create_spo2_task() which starts a FreeRTOS task
//    running the original sketch loop body. Also exposes globals used by spo2_adapter.cpp.
//    - Call create_spo2_task() from spo2_init_adapter() or from main setup.
// */
// #include <Arduino.h>
// #include <FreeRTOS.h>
// #include <task.h>
// #include <Wire.h>
// #include "MAX30105.h"
// #include "heartRate.h"

// // Globals exported for adapter
// float last_acdc_ir = 0.0f;
// float last_acdc_red = 0.0f;
// float last_rawSpO2 = 0.0f;
// float ESpO2 = 0.0f;
// float heartRate = 0.0f;

// // MAX30105 - RMS SpO2 + robust HR (MIN_ACDC_RATIO reverted to 0.02)
// // Feather nRF52840 Sense
// MAX30105 particleSensor;

// // ---------- TUNED PARAMETERS ----------
// const int Num = 100;                  // samples/window for SpO2
// const double frate = 0.95;            // DC IIR factor (keep running between windows)
// const double FSpO2 = 0.80;            // SpO2 smoothing (higher => more smoothing)
// const unsigned long PRINT_INTERVAL_MS = 1000; // print every 1s

// // HR filtering/smoothing thresholds
// const int HR_AVG_BEATS = 8;           // average across last N beats for stability
// const float HR_STDEV_THRESHOLD = 5.0; // allowed stdev (bpm) among stored beats to accept HR
// const float HR_ALPHA = 0.80;          // exponential smoothing factor for HR
// const int HR_MIN = 35;                // plausible min HR
// const int HR_MAX = 220;               // plausible max HR
// const int HR_MAX_DELTA = 15;          // max allowed jump (bpm) between displayed values per second

// // Signal quality gating & safety
// const double MIN_ACDC_RATIO = 0.02;   // REVERTED: accept AC/DC >= 2% (more permissive)
// const double DC_MIN = 10.0;           // require DC baseline > this to accept window
// const double MAX_ACDC_CLAMP = 2.0;    // clamp AC/DC to avoid spikes

// // Sensor hardware setup (tweak if needed)
// const byte ledBrightness = 50;        // 0..255 - increase if signal weak
// const byte sampleAverage = 4;         // 1,2,4,8,16,32
// const byte ledMode = 2;               // 1=Red only, 2=Red+IR, 3=Red+IR+Green
// const int sampleRate = 200;           // 50..3200
// const int pulseWidth = 411;           // 69,118,215,411
// const int adcRange = 16384;           // 2048..16384
// // ------------------------------------------------

// // runtime accumulators & state
// double avered = 0.0, aveir = 0.0;      // running DC estimates (keep between windows)
// double sumredrms = 0.0, sumirrms = 0.0;
// int sampleCounter = 0;

// int storedBeats[HR_AVG_BEATS];
// uint8_t storedIdx = 0;
// uint8_t storedCount = 0;
// float hr_smooth = 0.0;
// int displayedHR = 0;

// long lastBeatMillis = 0;
// int spo2 = 0;
// bool validHeartRate = false;
// bool validSPO2 = false;
// unsigned long lastPrint = 0;

// void spo2_original_setup() {
//   Serial.begin(115200);
//   unsigned long t0 = millis();
//   while (!Serial && millis() - t0 < 5000) delay(10);

//   Serial.println();
//   Serial.println("MAX30105 RMS SpO2 + Robust HR (MIN_ACDC_RATIO = 0.02)");

//   Wire.begin();
//   Wire.setClock(100000UL); // stable 100 kHz
//   delay(50);

//   if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
//     Serial.println("ERROR: MAX30105 not found. Check wiring & VCC.");
//     for (;; ) {
//       digitalWrite(LED_BUILTIN, HIGH); delay(300);
//       digitalWrite(LED_BUILTIN, LOW); delay(300);
//     }
//   }

//   particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
//   particleSensor.enableDIETEMPRDY();

//   // initialize accumulators
//   avered = aveir = 0.0;
//   sumredrms = sumirrms = 0.0;
//   sampleCounter = 0;

//   Serial.println("Columns: HR, SpO2, AC_IR, AC_Red, rawSpO2, ESpO2");
//   Serial.println("Place finger on sensor and keep steady. Collect lines to verify.");
// }

// void spo2_original_loop_body() {
//   // Read FIFO
//   particleSensor.check();
//   while (particleSensor.available()) {
//     uint32_t red = particleSensor.getFIFORed();
//     uint32_t ir  = particleSensor.getFIFOIR();

//     double fred = (double)red;
//     double fir  = (double)ir;

//     // Running DC estimate (do NOT reset between windows)
//     avered = avered * frate + fred * (1.0 - frate);
//     aveir  = aveir  * frate + fir  * (1.0 - frate);

//     // accumulate squared AC deviations
//     double devR = fred - avered;
//     double devI = fir  - aveir;
//     sumredrms += (devR * devR);
//     sumirrms += (devI * devI);

//     sampleCounter++;

//     // Heartbeat detection on IR
//     if (checkForBeat((long)ir)) {
//       unsigned long now = millis();
//       if (lastBeatMillis > 0) {
//         unsigned long delta = now - lastBeatMillis;
//         float instBPM = 60.0f / (delta / 1000.0f);
//         if (instBPM >= HR_MIN && instBPM <= HR_MAX) {
//           // store in circular buffer
//           storedBeats[storedIdx] = (int)round(instBPM);
//           storedIdx = (storedIdx + 1) % HR_AVG_BEATS;
//           if (storedCount < HR_AVG_BEATS) storedCount++;

//           // compute average & stdev
//           float sum = 0;
//           for (uint8_t k = 0; k < storedCount; k++) sum += storedBeats[k];
//           float avg = sum / storedCount;
//           float var = 0;
//           for (uint8_t k = 0; k < storedCount; k++) {
//             float d = storedBeats[k] - avg;
//             var += d * d;
//           }
//           float stdev = (storedCount > 1) ? sqrt(var / (storedCount - 1)) : 0;

//           // gating: require several beats and low variance
//           if (storedCount >= 3 && stdev < HR_STDEV_THRESHOLD) {
//             if (hr_smooth == 0.0f) hr_smooth = avg;
//             else hr_smooth = HR_ALPHA * hr_smooth + (1.0 - HR_ALPHA) * avg;
//             heartRate = (int)round(hr_smooth);
//             validHeartRate = true;
//           } else {
//             validHeartRate = false;
//           }
//         }
//       }
//       lastBeatMillis = now;
//     }

//     particleSensor.nextSample();
//   } // end FIFO processing

//   // compute SpO2 when enough samples collected
//   if (sampleCounter >= Num) {
//     // temporaries for window computation (local)
//     double acdc_red = -1.0;
//     double acdc_ir  = -1.0;
//     double rawSpO2  = -1.0;

//     if (avered > DC_MIN && aveir > DC_MIN && sumirrms > 0.0) {
//       double ac_red_rms = sqrt(sumredrms / (double)sampleCounter);
//       double ac_ir_rms  = sqrt(sumirrms  / (double)sampleCounter);

//       acdc_red = ac_red_rms / avered;
//       acdc_ir  = ac_ir_rms  / aveir;

//       // clamp AC/DC to avoid spikes
//       if (acdc_red > MAX_ACDC_CLAMP) acdc_red = MAX_ACDC_CLAMP;
//       if (acdc_ir  > MAX_ACDC_CLAMP) acdc_ir  = MAX_ACDC_CLAMP;

//       double R = acdc_red / (acdc_ir + 1e-12);
//       rawSpO2 = -23.3 * (R - 0.4) + 100.0;

//       // smoother SpO2
//       ESpO2 = FSpO2 * ESpO2 + (1.0 - FSpO2) * rawSpO2;

//       // validity: numeric range and signal quality (AC/DC)
//       if (ESpO2 > 50 && ESpO2 <= 100 && acdc_ir > MIN_ACDC_RATIO && acdc_red > MIN_ACDC_RATIO) {
//         spo2 = (int)round(ESpO2);
//         validSPO2 = true;
//       } else {
//         validSPO2 = false;
//       }
//     } else {
//       validSPO2 = false;
//     }

//     // store for logging / adapter consumption: always write something (globals)
//     last_acdc_ir  = (float)acdc_ir;
//     last_acdc_red = (float)acdc_red;
//     last_rawSpO2  = (float)rawSpO2;

//     // reset accumulators for next window BUT keep DC running (do not zero avered/aveir)
//     sumredrms = sumirrms = 0.0;
//     sampleCounter = 0;
//     // avered & aveir intentionally kept to maintain stable DC estimate

//     // printing block (once per second)
//     if (millis() - lastPrint >= PRINT_INTERVAL_MS) {
//       lastPrint = millis();

//       // clamp displayed HR change
//       if (validHeartRate) {
//         int targetHR = heartRate;
//         if (displayedHR == 0) displayedHR = targetHR;
//         else {
//           int delta = targetHR - displayedHR;
//           if (abs(delta) > HR_MAX_DELTA) {
//             displayedHR += (delta > 0) ? HR_MAX_DELTA : -HR_MAX_DELTA;
//           } else displayedHR = targetHR;
//         }
//       }

//       // Print columns: HR, SpO2, AC_IR, AC_Red, rawSpO2, ESpO2
//       Serial.print("HR=");
//       if (validHeartRate) Serial.print(displayedHR); else Serial.print("invalid");
//       Serial.print(", SpO2=");
//       if (validSPO2) Serial.print(spo2); else Serial.print("invalid");

//       Serial.print(", AC_IR=");
//       Serial.print(last_acdc_ir, 4);
//       Serial.print(", AC_Red=");
//       Serial.print(last_acdc_red, 4);

//       Serial.print(", rawSpO2=");
//       Serial.print(last_rawSpO2, 2);
//       Serial.print(", ESpO2=");
//       Serial.println(ESpO2, 2);
//     }
//   } // end window check

// } // end loop


// static void spo2_task_wrapper(void *pv) {
//     (void)pv;
//     // Call any original setup once
//     spo2_original_setup();
//     // Then run the loop body repeatedly (original loop likely contains its own timing)
//     while (1) {
//         spo2_original_loop_body();
//         vTaskDelay(pdMS_TO_TICKS(10)); // small yield so other tasks aren't starved
//     }
// }

// // Create the FreeRTOS task that runs the SPO2 processing
// void create_spo2_task(UBaseType_t priority, uint16_t stack_words) {
//     xTaskCreate(spo2_task_wrapper, "spo2", stack_words ? stack_words : 4096, NULL, priority ? priority : 2, NULL);
// }

/* Temporary SPO2 test module — faster windows, minimal status output.
   Use this for debugging only; revert to main module afterwards. */
#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

float last_acdc_ir = 0.0f;
float last_acdc_red = 0.0f;
float last_rawSpO2 = 0.0f;
float ESpO2 = 0.0f;
float heartRate = 0.0f;

volatile bool spo2_task_started = false;
volatile unsigned long spo2_last_window_ms = 0;

MAX30105 particleSensor;

const int Num = 50;
const double frate = 0.95;
const double FSpO2 = 0.80;
const unsigned long PRINT_INTERVAL_MS = 1000;

const int HR_AVG_BEATS = 8;
const float HR_STDEV_THRESHOLD = 5.0;
const float HR_ALPHA = 0.80;
const int HR_MIN = 35;
const int HR_MAX = 220;

const double MIN_ACDC_RATIO = 0.00;
const double DC_MIN = 1.0;
const double MAX_ACDC_CLAMP = 2.0;

const byte ledBrightness = 120;
const byte sampleAverage = 1;
const byte ledMode = 2;
const int sampleRate = 200;
const int pulseWidth = 411;
const int adcRange = 16384;

double avered = 0.0, aveir = 0.0;
double sumredrms = 0.0, sumirrms = 0.0;
int sampleCounter = 0;

int storedBeats[HR_AVG_BEATS];
uint8_t storedIdx = 0;
uint8_t storedCount = 0;
float hr_smooth = 0.0;
int displayedHR = 0;

long lastBeatMillis = 0;
int spo2 = 0;
bool validHeartRate = false;
bool validSPO2 = false;
unsigned long lastPrint = 0;

void spo2_original_setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);
  Serial.println();
  Serial.println("SPO2 TEST MODULE START");
  Wire.begin();
  Wire.setClock(100000UL);
  delay(50);
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("ERROR: MAX30105 not found. Check wiring & VCC.");
    for (;;) { digitalWrite(LED_BUILTIN, HIGH); delay(200); digitalWrite(LED_BUILTIN, LOW); delay(200); }
  }
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  particleSensor.enableDIETEMPRDY();
  avered = aveir = 0.0;
  sumredrms = sumirrms = 0.0;
  sampleCounter = 0;
}

void spo2_original_loop_body() {
  particleSensor.check();
  bool anyAvail = false;
  while (particleSensor.available()) {
    anyAvail = true;
    uint32_t red = particleSensor.getFIFORed();
    uint32_t ir  = particleSensor.getFIFOIR();
    double fred = (double)red;
    double fir  = (double)ir;
    avered = avered * frate + fred * (1.0 - frate);
    aveir  = aveir  * frate + fir  * (1.0 - frate);
    double devR = fred - avered;
    double devI = fir  - aveir;
    sumredrms += (devR * devR);
    sumirrms += (devI * devI);
    sampleCounter++;
    if (checkForBeat((long)ir)) {
      unsigned long now = millis();
      if (lastBeatMillis > 0) {
        unsigned long delta = now - lastBeatMillis;
        float instBPM = 60.0f / (delta / 1000.0f);
        if (instBPM >= HR_MIN && instBPM <= HR_MAX) {
          storedBeats[storedIdx] = (int)round(instBPM);
          storedIdx = (storedIdx + 1) % HR_AVG_BEATS;
          if (storedCount < HR_AVG_BEATS) storedCount++;
          float sum = 0;
          for (uint8_t k = 0; k < storedCount; k++) sum += storedBeats[k];
          float avg = sum / storedCount;
          float var = 0;
          for (uint8_t k = 0; k < storedCount; k++) { float d = storedBeats[k] - avg; var += d * d; }
          float stdev = (storedCount > 1) ? sqrt(var / (storedCount - 1)) : 0;
          if (storedCount >= 3 && stdev < HR_STDEV_THRESHOLD) {
            if (hr_smooth == 0.0f) hr_smooth = avg;
            else hr_smooth = HR_ALPHA * hr_smooth + (1.0 - HR_ALPHA) * avg;
            heartRate = (int)round(hr_smooth);
            validHeartRate = true;
          } else validHeartRate = false;
        }
      }
      lastBeatMillis = now;
    }
    particleSensor.nextSample();
  }
  static bool printed_no_fifo = false;
  if (!anyAvail && !printed_no_fifo && millis() > 3000) {
    Serial.println("TEST MODULE: FIFO never had available() == true within 3s");
    printed_no_fifo = true;
  }
  if (sampleCounter >= Num) {
    double acdc_red = -1.0, acdc_ir = -1.0, rawSpO2 = -1.0;
    if (avered > DC_MIN && aveir > DC_MIN && sumirrms > 0.0) {
      double ac_red_rms = sqrt(sumredrms / (double)sampleCounter);
      double ac_ir_rms  = sqrt(sumirrms  / (double)sampleCounter);
      acdc_red = ac_red_rms / avered;
      acdc_ir  = ac_ir_rms  / aveir;
      if (acdc_red > MAX_ACDC_CLAMP) acdc_red = MAX_ACDC_CLAMP;
      if (acdc_ir  > MAX_ACDC_CLAMP) acdc_ir  = MAX_ACDC_CLAMP;
      double R = acdc_red / (acdc_ir + 1e-12);
      rawSpO2 = -23.3 * (R - 0.4) + 100.0;
      ESpO2 = FSpO2 * ESpO2 + (1.0 - FSpO2) * rawSpO2;
      if (ESpO2 > 30 && ESpO2 <= 100 && acdc_ir >= 0.0 && acdc_red >= 0.0) {
        spo2 = (int)round(ESpO2);
        validSPO2 = true;
      } else validSPO2 = false;
    } else validSPO2 = false;
    last_acdc_ir  = (float)acdc_ir;
    last_acdc_red = (float)acdc_red;
    last_rawSpO2  = (float)rawSpO2;
    spo2_last_window_ms = millis();
    sumredrms = sumirrms = 0.0;
    sampleCounter = 0;
    Serial.printf("W: ESpO2=%.2f rawSpO2=%.2f IRac=%.4f REDac=%.4f HR=%d valid=%d\n",
                  ESpO2, (double)last_rawSpO2, (double)last_acdc_ir, (double)last_acdc_red, heartRate, validSPO2 ? 1 : 0);
  }
}

static void spo2_task_wrapper(void *pv) {
  (void)pv;
  spo2_original_setup();
  spo2_task_started = true;
  while (1) {
    spo2_original_loop_body();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void create_spo2_task(UBaseType_t priority, uint16_t stack_words) {
  xTaskCreate(spo2_task_wrapper, "spo2", stack_words ? stack_words : 4096, NULL, priority ? priority : 2, NULL);
}
