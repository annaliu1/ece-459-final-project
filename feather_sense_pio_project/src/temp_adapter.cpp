// src/temp_adapter.cpp
#include <Arduino.h>
#include "sensor_manager.h"
#include <Wire.h>
#include <Adafruit_TinyUSB.h>

// These are implemented in temp_sensor_module.cpp
extern void temp_sensor_init(void);
extern uint16_t readTemp(void);

// Global set by adapter when it finds the device (0 = unknown)
uint8_t temp_i2c_addr = 0;
#define SCL 25
#define SDA 26

void i2c_unstick() {
  Serial.println("Attempting I2C unstick: toggling SCL and sending STOP");

  // Configure pins as open-drain style: drive low actively, release to pull-up for high
  pinMode(SCL, OUTPUT);
  pinMode(SDA, OUTPUT);
  digitalWrite(SCL, HIGH); // release (we'll treat HIGH as released if external pullups)
  digitalWrite(SDA, HIGH);
  delay(5);

  // If SDA is held low, try clocking SCL up to 9 times to make slave release SDA
  if (digitalRead(SDA) == LOW) {
    Serial.println("SDA is LOW; pulsing SCL up to 9 times to recover...");
    // Generate 9 clock pulses
    for (int i = 0; i < 9; ++i) {
      // SCL low
      digitalWrite(SCL, LOW);
      delayMicroseconds(250);
      // SCL high (released)
      pinMode(SCL, INPUT); // release to let pull-up pull it high
      delayMicroseconds(250);
      // read current values
      int sda = digitalRead(SDA);
      int scl = digitalRead(SCL);
      Serial.printf("pulse %d: SCL=%d SDA=%d\n", i+1, scl, sda);
      // re-configure for next pulse
      pinMode(SCL, OUTPUT);
      digitalWrite(SCL, HIGH); // release before next low
      delayMicroseconds(100);
      if (digitalRead(SDA) == HIGH) {
        Serial.println("SDA released during pulses.");
        break;
      }
    }
  } else {
    Serial.println("SDA is HIGH at start; bus looks released.");
  }

  // Try issuing STOP: SDA low, SCL high, then SDA high.
  Serial.println("Issuing STOP sequence...");
  pinMode(SDA, OUTPUT);
  digitalWrite(SDA, LOW);
  delayMicroseconds(100);
  // release SCL
  pinMode(SCL, INPUT);
  delayMicroseconds(200);
  // release SDA (STOP)
  pinMode(SDA, INPUT);
  delayMicroseconds(200);

  Serial.printf("After STOP: SCL=%d SDA=%d\n", digitalRead(SCL), digitalRead(SDA));
  delay(50);
}

void i2c_scan() {
  Serial.println("I2C scan start");
  Wire.begin(); // ensure Wire is started
  delay(50);
  bool found = false;
  for (byte addr = 1; addr < 127; ++addr) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.print("I2C device found at 0x");
      if (addr < 16) Serial.print('0');
      Serial.println(addr, HEX);
      found = true;
    } else {
      // optionally print err code for debugging
      // Serial.printf("addr 0x%02X err=%u\n", addr, err);
    }
    delay(5);
  }
  if (!found) Serial.println("No I2C devices found");
  Serial.println("I2C scan done");
}


// Probe a small set of likely addresses only (non-blocking, with recovery)
bool probe_common_addrs_and_record(void) {
  const uint8_t probe_addrs[] = { 0x48, 0x76, 0x77 };

  for (size_t i = 0; i < sizeof(probe_addrs); ++i) {
    uint8_t a = probe_addrs[i];
    Serial.printf("temp_adapter: probe 0x%02X ...\r\n", a);
    Serial.flush();
    Wire.beginTransmission(a);
    uint8_t err = Wire.endTransmission();
    Serial.printf("temp_adapter: probe 0x%02X result=%u\r\n", a, err);
    Serial.flush();
    if (err == 0) {
      Serial.printf("temp_adapter: device ACK at 0x%02X\r\n", a);
      temp_i2c_addr = a;
      return true;
    }
    // small pause so we don't hammer the bus
    delay(20);
  }
  return false;
}

bool temp_init_adapter(void *ctx) {
  (void)ctx;
  Serial.println("temp_init_adapter: start (with recovery+targeted probes)");
  Wire.begin(); // ensure Wire exists
  delay(10);

  i2c_unstick();

  i2c_scan();

  bool found = probe_common_addrs_and_record();
  if (!found) {
    Serial.println("temp_adapter: no device found at known addresses (0x48/0x76/0x77)");
  } else {
    Serial.printf("temp_adapter: using addr 0x%02X\n", temp_i2c_addr);
  }

  // call minimal module init (should not probe)
  temp_sensor_init();

  Serial.println("temp_init_adapter: done (return true)");
  return true;
}

bool temp_read_adapter(void *ctx, sensor_data_t *out) {
  (void)ctx;
  if (!out) return false;
  memset(out, 0, sizeof(*out));
  if (temp_i2c_addr == 0) return false; // no known device

  uint16_t raw = readTemp();
  if (raw == 0xFFFF) { out->len = 0; return false; }

  float temp_c = (float)raw / 256.0f; // adjust if needed for your sensor
  int16_t t_scaled = (int16_t)roundf(temp_c * 100.0f);
  size_t idx = 0;
  out->bytes[idx++] = (uint8_t)((t_scaled >> 8) & 0xFF);
  out->bytes[idx++] = (uint8_t)(t_scaled & 0xFF);
  out->len = idx;
  return true;
}

void temp_print_adapter(void *ctx, const sensor_data_t *d) {
  (void)ctx;
  if (!d || d->len < 2) {
    Serial.println("  Temp: (no data)");
    return;
  }
  int16_t t = (int16_t)((d->bytes[0] << 8) | d->bytes[1]);
  Serial.printf("  Temp: %.2f C\r\n", t / 100.0f);
}
