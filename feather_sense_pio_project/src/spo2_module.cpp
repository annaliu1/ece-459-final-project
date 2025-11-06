/* Converted SPO2 module: provides create_spo2_task() which starts a FreeRTOS task
   running the original sketch loop body. Also exposes globals used by spo2_adapter.cpp.
   - Call create_spo2_task() from spo2_init_adapter() or from main setup.
*/
#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

// Globals exported for adapter (volatile to avoid torn reads/writes across tasks)
volatile float last_acdc_ir = 0.0f;
volatile float last_acdc_red = 0.0f;
volatile float last_rawSpO2 = 0.0f;
volatile float ESpO2 = 0.0f;
volatile float heartRate = 0.0f;

// MAX30105 - RMS SpO2 + robust HR (MIN_ACDC_RATIO reverted to 0.02)
MAX30105 particleSensor;

// ---------- TUNED PARAMETERS ----------
const int Num = 100;                  // samples/window for SpO2
const double frate = 0.95;            // DC IIR factor (keep running between windows)
const double FSpO2 = 0.80;            // SpO2 smoothing (higher => more smoothing)
const unsigned long PRINT_INTERVAL_MS = 1000; // print every 1s

// HR filtering/smoothing thresholds
const int HR_AVG_BEATS = 8;           // average across last N beats for stability
const float HR_STDEV_THRESHOLD = 5.0; // allowed stdev (bpm) among stored beats to accept HR (kept as reference)
const float HR_ALPHA = 0.80;          // exponential smoothing factor for HR
const int HR_MIN = 35;                // plausible min HR
const int HR_MAX = 220;               // plausible max HR
const int HR_MAX_DELTA = 15;          // max allowed jump (bpm) between displayed values per second

// Signal quality gating & safety
const double MIN_ACDC_RATIO = 0.02;   // accept AC/DC >= 2% (more permissive)
const double DC_MIN = 10.0;           // require DC baseline > this to accept window
const double MAX_ACDC_CLAMP = 2.0;    // clamp AC/DC to avoid spikes

// Sensor hardware setup (tweak if needed)
const byte ledBrightness = 50;        // 0..255 - increase if signal weak
const byte sampleAverage = 4;         // 1,2,4,8,16,32
const byte ledMode = 2;               // 1=Red only, 2=Red+IR, 3=Red+IR+Green
const int sampleRate = 200;           // 50..3200
const int pulseWidth = 411;           // 69,118,215,411
const int adcRange = 16384;           // 2048..16384
// ------------------------------------------------

// runtime accumulators & state
double avered = 0.0, aveir = 0.0;      // running DC estimates (keep between windows)
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

// ----------------- Utility (median + MAD) -----------------
// compute median of int array of length n (n>0)
static float compute_median_int(const int *arr, uint8_t n) {
  if (n == 0) return 0.0f;
  // copy to temp buffer
  int tmp[HR_AVG_BEATS];
  for (uint8_t i = 0; i < n; ++i) tmp[i] = arr[i];
  // simple insertion sort (n <= 8)
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

// compute MAD (median absolute deviation) from median
static float compute_mad_int(const int *arr, uint8_t n, float median) {
  if (n == 0) return 0.0f;
  float absdev[HR_AVG_BEATS];
  for (uint8_t i = 0; i < n; ++i) absdev[i] = fabsf((float)arr[i] - median);
  // sort small array
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
// convert MAD to approximate standard deviation: std ≈ 1.4826 * MAD (for normal dist)
static float mad_to_std(float mad) {
  return mad * 1.4826f;
}
// ----------------------------------------------------------

void spo2_original_setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 5000) delay(10);

  Serial.println();
  Serial.println("MAX30105 RMS SpO2 + Robust HR (MIN_ACDC_RATIO = 0.02)");

  Wire.begin();
  Wire.setClock(100000UL); // stable 100 kHz
  delay(50);

  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("ERROR: MAX30105 not found. Check wiring & VCC.");
    for (;; ) {
      digitalWrite(LED_BUILTIN, HIGH); delay(300);
      digitalWrite(LED_BUILTIN, LOW); delay(300);
    }
  }

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  particleSensor.enableDIETEMPRDY();

  // initialize accumulators
  avered = aveir = 0.0;
  sumredrms = sumirrms = 0.0;
  sampleCounter = 0;

  Serial.println("Columns: HR, SpO2, AC_IR, AC_Red, rawSpO2, ESpO2");
  Serial.println("Place finger on sensor and keep steady. Collect lines to verify.");
}

void spo2_original_loop_body() {
  // Read FIFO
  particleSensor.check();
  while (particleSensor.available()) {
    uint32_t red = particleSensor.getFIFORed();
    uint32_t ir  = particleSensor.getFIFOIR();

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

    // Heartbeat detection on IR
    if (checkForBeat((long)ir)) {
      unsigned long now = millis();
      if (lastBeatMillis > 0) {
        unsigned long delta = now - lastBeatMillis;
        float instBPM = 60.0f / (delta / 1000.0f);
        if (instBPM >= HR_MIN && instBPM <= HR_MAX) {
          // store in circular buffer
          storedBeats[storedIdx] = (int)round(instBPM);
          storedIdx = (storedIdx + 1) % HR_AVG_BEATS;
          if (storedCount < HR_AVG_BEATS) storedCount++;

          // compute robust median & MAD
          float median = compute_median_int(storedBeats, storedCount);
          float mad = compute_mad_int(storedBeats, storedCount, median);
          float approx_std = mad_to_std(mad);

          // gating: require several beats and small robust deviation
          if (storedCount >= 3 && approx_std < HR_STDEV_THRESHOLD) {
            // use median as robust center (less sensitive to outliers)
            if (hr_smooth == 0.0f) hr_smooth = median;
            else hr_smooth = HR_ALPHA * hr_smooth + (1.0f - HR_ALPHA) * median;
            heartRate = (int)round(hr_smooth);
            validHeartRate = true;
          } else {
            validHeartRate = false;
          }
        }
      } else {
        // first beat notification (no delta yet) - keep lastBeatMillis updated
        // no action required here
      }
      lastBeatMillis = now;
    }

    particleSensor.nextSample();
  } // end FIFO processing

  // compute SpO2 when enough samples collected
  if (sampleCounter >= Num) {
    // temporaries for window computation (local)
    double acdc_red = -1.0;
    double acdc_ir  = -1.0;
    double rawSpO2  = -1.0;

    if (avered > DC_MIN && aveir > DC_MIN && sumirrms > 0.0) {
      double ac_red_rms = sqrt(sumredrms / (double)sampleCounter);
      double ac_ir_rms  = sqrt(sumirrms  / (double)sampleCounter);

      acdc_red = ac_red_rms / avered;
      acdc_ir  = ac_ir_rms  / aveir;

      // clamp AC/DC to avoid spikes
      if (acdc_red > MAX_ACDC_CLAMP) acdc_red = MAX_ACDC_CLAMP;
      if (acdc_ir  > MAX_ACDC_CLAMP) acdc_ir  = MAX_ACDC_CLAMP;

      double R = acdc_red / (acdc_ir + 1e-12);
      rawSpO2 = -23.3 * (R - 0.4) + 100.0;

      // clamp raw and estimated to 0..100
      if (rawSpO2 < 0.0) rawSpO2 = 0.0;
      if (rawSpO2 > 100.0) rawSpO2 = 100.0;

      // smoother SpO2
      ESpO2 = FSpO2 * ESpO2 + (1.0 - FSpO2) * rawSpO2;
      if (ESpO2 < 0.0) ESpO2 = 0.0;
      if (ESpO2 > 100.0) ESpO2 = 100.0;

      // validity: numeric range and signal quality (AC/DC)
      if (ESpO2 > 50 && ESpO2 <= 100 && acdc_ir > MIN_ACDC_RATIO && acdc_red > MIN_ACDC_RATIO) {
        spo2 = (int)round(ESpO2);
        validSPO2 = true;
      } else {
        validSPO2 = false;
      }
    } else {
      validSPO2 = false;
    }

    // store for logging / adapter consumption: always write something (globals)
    // writes to volatile globals
    last_acdc_ir  = (float)acdc_ir;
    last_acdc_red = (float)acdc_red;
    last_rawSpO2  = (float)rawSpO2;

    // reset accumulators for next window BUT keep DC running (do not zero avered/aveir)
    sumredrms = sumirrms = 0.0;
    sampleCounter = 0;

    // printing block (once per second)
    if (millis() - lastPrint >= PRINT_INTERVAL_MS) {
      lastPrint = millis();

      // clamp displayed HR change
      if (validHeartRate) {
        int targetHR = heartRate;
        if (displayedHR == 0) displayedHR = targetHR;
        else {
          int delta = targetHR - displayedHR;
          if (abs(delta) > HR_MAX_DELTA) {
            displayedHR += (delta > 0) ? HR_MAX_DELTA : -HR_MAX_DELTA;
          } else displayedHR = targetHR;
        }
      }

      // Print columns: HR, SpO2, AC_IR, AC_Red, rawSpO2, ESpO2
      Serial.print("HR=");
      if (validHeartRate) Serial.print(displayedHR); else Serial.print("invalid");
      Serial.print(", SpO2=");
      if (validSPO2) Serial.print(spo2); else Serial.print("invalid");

      Serial.print(", AC_IR=");
      Serial.print(last_acdc_ir, 4);
      Serial.print(", AC_Red=");
      Serial.print(last_acdc_red, 4);

      Serial.print(", rawSpO2=");
      Serial.print(last_rawSpO2, 2);
      Serial.print(", ESpO2=");
      Serial.println(ESpO2, 2);
    }
  } // end window check

} // end loop


static void spo2_task_wrapper(void *pv) {
    (void)pv;
    // Call any original setup once
    spo2_original_setup();
    // Then run the loop body repeatedly (original loop likely contains its own timing)
    while (1) {
        spo2_original_loop_body();
        vTaskDelay(pdMS_TO_TICKS(10)); // small yield so other tasks aren't starved
    }
}

// Create the FreeRTOS task that runs the SPO2 processing
void create_spo2_task(UBaseType_t priority, uint16_t stack_words) {
    xTaskCreate(spo2_task_wrapper, "spo2", stack_words ? stack_words : 4096, NULL, priority ? priority : 2, NULL);
}
