// /*
//   Optical SP02 Detection (SPK Algorithm) using the MAX30105 Breakout
//   By: Nathan Seidle @ SparkFun Electronics
//   Date: October 19th, 2016
//   https://github.com/sparkfun/MAX30105_Breakout

//   This demo shows heart rate and SPO2 levels.

//   It is best to attach the sensor to your finger using a rubber band or other tightening 
//   device. Humans are generally bad at applying constant pressure to a thing. When you 
//   press your finger against the sensor it varies enough to cause the blood in your 
//   finger to flow differently which causes the sensor readings to go wonky.


/* SparkFun MAX30105 SPO2 example — adapted for Adafruit Feather nRF52840 Sense
   Changes:
   - Use Adafruit_TinyUSB Serial handling (required for nRF52840)
   - Force Wire clock to 100 kHz (Wire.setClock(100000UL))
   - Use particleSensor.begin(Wire, I2C_SPEED_STANDARD) instead of FAST
   - Avoid blocking "press any key" waits that hang when Serial isn't a tty
*/

#include <Adafruit_TinyUSB.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

MAX30105 particleSensor;

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
uint16_t irBuffer[100];
uint16_t redBuffer[100];
#else
uint32_t irBuffer[100];
uint32_t redBuffer[100];
#endif

int32_t bufferLength; 
int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;

// Use LED_BUILTIN for visible feedback on Feather
byte pulseLED = LED_BUILTIN; 
byte readLED  = LED_BUILTIN;

void setup()
{
  // USB CDC serial on nRF52840: requires TinyUSB to enumerate
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 5000) { delay(10); } // wait up to 5s for host

  pinMode(pulseLED, OUTPUT);
  pinMode(readLED, OUTPUT);

  Serial.println();
  Serial.println(F("MAX30105 SPO2 example (nRF52840-adapted)"));
  Serial.println(F("Initializing I2C and sensor..."));

  Wire.begin();
  Wire.setClock(100000UL); // force 100 kHz for stability

  // Try to init sensor at STANDARD speed (not FAST)
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println(F("MAX30105 was not found. Please check wiring/power."));
    // Blink LED and print status so you can read the error
    for (;;) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(300);
      digitalWrite(LED_BUILTIN, LOW);
      delay(300);
      Serial.println(F("Sensor not found..."));
    }
  }

  Serial.println(F("Attach sensor to finger (rubber band helpful). Starting measurements..."));

  // Config: keep reasonable defaults (tweak later)
  byte ledBrightness = 60; // 0..255
  byte sampleAverage = 4;  // 1,2,4,8,16,32
  byte ledMode = 2;        // 1=Red only, 2=Red+IR, 3=Red+IR+Green
  byte sampleRate = 10;   // 50..3200
  int pulseWidth = 411;    // 69,118,215,411
  int adcRange = 4096;     // 2048..16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  delay(50);
}

void loop()
{
  // fill the buffers with 100 samples (bufferLength = 100)
  bufferLength = 100;

  Serial.println(F("Collecting 100 samples (first run) ..."));
  for (byte i = 0; i < bufferLength; i++) {
    // Wait for new data
    while (particleSensor.available() == false) {
      particleSensor.check();
    }

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i]  = particleSensor.getIR();
    particleSensor.nextSample();

    // feedback
    digitalWrite(readLED, !digitalRead(readLED));
    Serial.print(F("red=")); Serial.print(redBuffer[i]);
    Serial.print(F(", ir="));  Serial.println(irBuffer[i]);
  }

  // calculate HR and SpO2
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

  Serial.println(F("Initial calculation done. Now continuous sampling..."));
  Serial.print(F("HR=")); Serial.print(heartRate);
  Serial.print(F(" (valid=")); Serial.print(validHeartRate);
  Serial.print(F("), SpO2=")); Serial.print(spo2);
  Serial.print(F(" (valid=")); Serial.print(validSPO2);
  Serial.println(F(")"));

  // Continuous running loop (same structure as original)
  for (;;) {
    // shift arrays
    for (byte i = 25; i < 100; i++) {
      redBuffer[i - 25] = redBuffer[i];
      irBuffer[i - 25]  = irBuffer[i];
    }

    // take 25 new samples
    for (byte i = 75; i < 100; i++) {
      while (particleSensor.available() == false) particleSensor.check();

      // blink read LED to show progress
      digitalWrite(readLED, !digitalRead(readLED));

      redBuffer[i] = particleSensor.getRed();
      irBuffer[i]  = particleSensor.getIR();
      particleSensor.nextSample();
    
      // Serial.print(F("red=")); Serial.print(redBuffer[i]);
      // Serial.print(F(", ir=")); Serial.print(irBuffer[i]);
      if (validHeartRate){
         Serial.print(F(", HR=")); Serial.println(heartRate);
      }
     
      //Serial.print(F(", HRvalid=")); Serial.print(validHeartRate);
      if(validSPO2){
        Serial.print(F(", SPO2=")); Serial.println(spo2);
      }
      
      //Serial.print(F(", SPO2Valid=")); Serial.println(validSPO2);
    }

    // recalc
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
  }
}

