/* spo2_module.cpp
   SPO2 module: background task that drains MAX30105 FIFO and computes SpO2/HR.
   Debug prints are disabled by default; set SPO2_DEBUG to 1 to enable verbose logging.
*/

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "sensor_manager.h"

// enable/disable verbose debug prints for SPO2
#ifndef SPO2_DEBUG
#define SPO2_DEBUG 0
#endif

// Globals exported for adapter (volatile to avoid torn reads/writes across tasks)
volatile float last_acdc_ir_2 = 0.0f;
volatile float last_acdc_red_2 = 0.0f;
volatile float last_rawSpO2_2 = 0.0f;
volatile float ESpO2_2 = 0.0f;
volatile float heartRate_2 = 0.0f;

// MAX30105 instance
MAX30105 particleSensor_2;
TwoWire Wire1(NRF_TWIM1, NRF_TWIS1, PWM1_IRQn, 12, 13);  // SDA=12, SCL=13

// ---------- TUNED PARAMETERS ----------
const int Num = 100;                  // samples/window for SpO2
const double frate = 0.95;            // DC IIR factor (keep running between windows)
const double FSpO2 = 0.80;            // SpO2 smoothing (higher => more smoothing)
const unsigned long PRINT_INTERVAL_MS = 1000; // print every 1s

// HR filtering/smoothing thresholds
const int HR_AVG_BEATS = 4;
const float HR_STDEV_THRESHOLD = 5.0;
const float HR_ALPHA = 0.75;
const int HR_MIN = 35;
const int HR_MAX = 220;
const int HR_MAX_DELTA = 15;

// Signal quality gating & safety
const double MIN_ACDC_RATIO = 0.02;
const double DC_MIN = 10.0;
const double MAX_ACDC_CLAMP = 2.0;

// Sensor hardware setup
const byte ledBrightness = 75;        // 0..255
const byte sampleAverage = 4;         // 1,2,4,8,16,32
const byte ledMode = 2;               // 1=Red only, 2=Red+IR, 3=Red+IR+Green
const int sampleRate = 200;           // 50..3200
const int pulseWidth = 411;           // 69,118,215,411
const int adcRange = 16384;           // 2048..16384

// runtime accumulators & state
static double avered = 0.0, aveir = 0.0;
static double sumredrms = 0.0, sumirrms = 0.0;
static int sampleCounter = 0;

static int storedBeats[HR_AVG_BEATS];
static uint8_t storedIdx = 0;
static uint8_t storedCount = 0;
static float hr_smooth = 0.0;
static int displayedHR = 0;

static long lastBeatMillis = 0;
static int spo2 = 0;
static bool validHeartRate = false;
static bool validSPO2 = false;
static unsigned long lastPrint = 0;

// ----------------- Utility (median + MAD) -----------------
static float compute_median_int(const int *arr, uint8_t n) {
  if (n == 0) return 0.0f;
  int tmp[HR_AVG_BEATS];
  for (uint8_t i = 0; i < n; ++i) tmp[i] = arr[i];
  for (uint8_t i = 1; i < n; ++i) {
    int v = tmp[i];
    int j = i;
    while (j > 0 && tmp[j-1] > v) {
      tmp[j] = tmp[j-1];
      --j;
    }
    tmp[j] = v;
  }
  if (n % 2 == 1) return (float) tmp[n/2];
  return 0.5f * ( (float)tmp[n/2 - 1] + (float)tmp[n/2] );
}

static float compute_mad_int(const int *arr, uint8_t n, float median) {
  if (n == 0) return 0.0f;
  float absdev[HR_AVG_BEATS];
  for (uint8_t i = 0; i < n; ++i) absdev[i] = fabsf((float)arr[i] - median);
  for (uint8_t i = 1; i < n; ++i) {
    float v = absdev[i];
    uint8_t j = i;
    while (j > 0 && absdev[j-1] > v) {
      absdev[j] = absdev[j-1];
      --j;
    }
    absdev[j] = v;
  }
  if (n % 2 == 1) return absdev[n/2];
  return 0.5f * (absdev[n/2 - 1] + absdev[n/2]);
}
static float mad_to_std(float mad) { return mad * 1.4826f; }

// Forward declare readSpo2 (used by task)
void readSpo2_2(void);

// ----------------- Task management -----------------
static TaskHandle_t spo2TaskHandle = NULL;

static void spo2_task_fn(void *pv) {
  (void)pv;
  for (;;) {
    readSpo2_2(); // drain FIFO and update volatile globals
    vTaskDelay(pdMS_TO_TICKS(20)); // ~50 Hz; tune if needed
  }
}

void create_spo2_task_2(UBaseType_t prio, uint32_t stack_words) {
  if (spo2TaskHandle == NULL) {
    xTaskCreate(spo2_task_fn, "spo2", stack_words ? stack_words : 4096, NULL, prio ? prio : 1, &spo2TaskHandle);
    #if SPO2_DEBUG
    Serial.println("create_spo2_task: spawned background SPO2 task");
    #endif
  }
}

//Sensor init
void spo2_sensor_init_2() {
  Wire1.setPins(12, 13);      // SDA, SCL
  Wire1.begin();              // Initialize bus
  Wire1.setClock(100000UL);   // Set I2C speed

  #if SPO2_DEBUG
  Serial.println("spo2_sensor_init_2: attempting particleSensor_2.begin() ...");
  #endif

  bool begun = particleSensor_2.begin(Wire1);

  if (!begun) {
    Serial.println("ERROR: MAX30105 not found on Wire1");
    return;
  }

  particleSensor_2.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  particleSensor_2.enableDIETEMPRDY();
  particleSensor_2.setPulseAmplitudeRed(ledBrightness);
  particleSensor_2.setPulseAmplitudeIR(ledBrightness);

  avered = aveir = 0.0;
  sumredrms = sumirrms = 0.0;
  sampleCounter = 0;

  #if SPO2_DEBUG
  Serial.println("spo2_sensor_init_2: initialized OK");
  #endif
}

// ----------------- Processing (called frequently) -----------------
#include "sensor_manager.h" // add this at top of spo2_module.cpp if not already included

void readSpo2_2() {
  #if SPO2_DEBUG
  Serial.println("DEBUG: readSpo2()_2 called");
  #endif

  // Attempt to lock the shared I2C bus before reading FIFO.
  // Use a modest timeout so we don't block forever (50 ms recommended).
  if (!sensor_bus_lock(pdMS_TO_TICKS(50))) {
    // Could not acquire bus; skip this cycle and try again next time.
    #if SPO2_DEBUG
    Serial.println("DEBUG: readSpo2()_2 - failed to lock bus, skipping this cycle");
    #endif
    return;
  }

  // I2C-protected region: read FIFO and advance samples
  particleSensor_2.check();
  int samplesProcessedThisCall = 0;

  while (particleSensor_2.available()) {
    uint32_t red = particleSensor_2.getFIFORed();
    uint32_t ir  = particleSensor_2.getFIFOIR();

    double fred = (double)red;
    double fir  = (double)ir;

    // Running DC estimate (do NOT reset between windows)
    avered = avered * frate + fred * (1.0 - frate);
    aveir  = aveir  * frate + fir  * (1.0 - frate);

    // accumulate squared AC deviations
    double devR = fred - avered;
    double devI = fir  - aveir;
    sumredrms += (devR * devR);
    sumirrms += (devI * devI);

    sampleCounter++;
    samplesProcessedThisCall++;

    // Heartbeat detection on IR (this part uses only local vars)
    if (checkForBeat((long)ir)) {
      unsigned long now = millis();
      if (lastBeatMillis > 0) {
        unsigned long delta = now - lastBeatMillis;
        float instBPM = 60.0f / (delta / 1000.0f);
        if (instBPM >= HR_MIN && instBPM <= HR_MAX) {
          storedBeats[storedIdx] = (int)round(instBPM);
          storedIdx = (storedIdx + 1) % HR_AVG_BEATS;
          if (storedCount < HR_AVG_BEATS) storedCount++;

          float median = compute_median_int(storedBeats, storedCount);
          float mad = compute_mad_int(storedBeats, storedCount, median);
          float approx_std = mad_to_std(mad);

          if (storedCount >= 3 && approx_std < HR_STDEV_THRESHOLD) {
            if (hr_smooth == 0.0f) hr_smooth = median;
            else hr_smooth = HR_ALPHA * hr_smooth + (1.0f - HR_ALPHA) * median;
            heartRate_2 = (int)round(hr_smooth);
            validHeartRate = true;
          } else {
            validHeartRate = false;
          }
        }
      }
      lastBeatMillis = now;
    }

    particleSensor_2.nextSample();
  } // end FIFO loop

  // Release I2C bus so others can use it
  sensor_bus_unlock();

  #if SPO2_DEBUG
  Serial.printf("DEBUG: processed %d samples this call, sampleCounter=%d, avered=%.1f aveir=%.1f\n",
                samplesProcessedThisCall, sampleCounter, avered, aveir);
  #endif


  #if SPO2_DEBUG
  Serial.printf("DEBUG: processed %d samples this call, sampleCounter=%d, avered=%.1f aveir=%.1f\n",
                samplesProcessedThisCall, sampleCounter, avered, aveir);
  #endif

  // compute SpO2 when enough samples collected
  if (sampleCounter >= Num) {
    double acdc_red = -1.0;
    double acdc_ir  = -1.0;
    double rawSpO2  = -1.0;

    if (avered > DC_MIN && aveir > DC_MIN && sumirrms > 0.0) {
      double ac_red_rms = sqrt(sumredrms / (double)sampleCounter);
      double ac_ir_rms  = sqrt(sumirrms  / (double)sampleCounter);

      acdc_red = ac_red_rms / avered;
      acdc_ir  = ac_ir_rms  / aveir;

      if (acdc_red > MAX_ACDC_CLAMP) acdc_red = MAX_ACDC_CLAMP;
      if (acdc_ir  > MAX_ACDC_CLAMP) acdc_ir  = MAX_ACDC_CLAMP;

      double R = acdc_red / (acdc_ir + 1e-12);
      rawSpO2 = -23.3 * (R - 0.4) + 100.0;

      if (rawSpO2 < 0.0) rawSpO2 = 0.0;
      if (rawSpO2 > 100.0) rawSpO2 = 100.0;

      ESpO2_2 = FSpO2 * ESpO2_2 + (1.0 - FSpO2) * rawSpO2;
      if (ESpO2_2 < 0.0) ESpO2_2 = 0.0;
      if (ESpO2_2 > 100.0) ESpO2_2 = 100.0;

      if (ESpO2_2 > 50 && ESpO2_2 <= 100 && acdc_ir > MIN_ACDC_RATIO && acdc_red > MIN_ACDC_RATIO) {
        spo2 = (int)round(ESpO2_2);
        validSPO2 = true;
      } else {
        validSPO2 = false;
      }
    } else {
      validSPO2 = false;
    }

    // store for logging / adapter consumption: always write something (globals)
    last_acdc_ir_2  = (float)acdc_ir;
    last_acdc_red_2 = (float)acdc_red;
    last_rawSpO2_2  = (float)rawSpO2;

    #if SPO2_DEBUG
    Serial.printf("SPO2DBG: window done sampleCounter=%d ESpO2=%.2f raw=%.2f IRacdc=%.3f REDacdc=%.3f valid=%d HR=%d\n",
                  sampleCounter, ESpO2_2, rawSpO2, acdc_ir, acdc_red, validSPO2 ? 1 : 0, displayedHR);
    #endif

    // reset accumulators for next window BUT keep DC running (do not zero avered/aveir)
    sumredrms = sumirrms = 0.0;
    sampleCounter = 0;

    if (millis() - lastPrint >= PRINT_INTERVAL_MS) {
      lastPrint = millis();
      if (validHeartRate) {
        int targetHR = heartRate_2;
        if (displayedHR == 0) displayedHR = targetHR;
        else {
          int delta = targetHR - displayedHR;
          if (abs(delta) > HR_MAX_DELTA) {
            displayedHR += (delta > 0) ? HR_MAX_DELTA : -HR_MAX_DELTA;
          } else displayedHR = targetHR;
        }
      }
    }
  } // end window check
}
