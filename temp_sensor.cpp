#include <Adafruit_TinyUSB.h>

// #include "Adafruit_SHT31.h"

// Adafruit_SHT31 sht31 = Adafruit_SHT31();

// void setup() {
//   Serial.begin(115200);
//   while (!Serial);
//   Serial.println("Feather Sense onboard SHT31-D sensor");

//   if (!sht31.begin(0x44)) {   // 0x44 from your I2C scan
//     Serial.println("Couldn't find SHT31-D sensor!");
//     while (1);
//   }
// }

// void loop() {
//   float tempC = sht31.readTemperature();
//   float hum   = sht31.readHumidity();

//   Serial.printf("Temperature: %.2f °C / %.2f °F   Humidity: %.2f %%\n",
//                 tempC, tempC * 1.8f + 32.0f, hum);

//   delay(1000);
// }

#include <Wire.h>

#define MAX30205_TEMP_SENSOR_I2C_ADDR 0x48
uint16_t readTemp(void);

void setup() 
{
  Serial.begin(115200);
  while (!Serial) delay(10);

  Wire.begin();
  Serial.println("init i2c");
  // iterate through possible I2C addresses, find the temp sensor
  // default address is 0x48
  for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        Serial.print("Found device at 0x");
        Serial.println(addr, HEX);
      }
    }
}

void loop()
{
  uint16_t temp = readTemp();
  float temp_celsius = temp / 256.0f;
  Serial.print("Temp = ");
  Serial.print(temp_celsius, 2);
  Serial.println(" °C");
  delay(500);
}

uint16_t readTemp(void)
{
  Wire.beginTransmission(MAX30205_TEMP_SENSOR_I2C_ADDR);
  Wire.write(0x00);
  Wire.endTransmission(false);

  Wire.requestFrom(MAX30205_TEMP_SENSOR_I2C_ADDR, (uint8_t)2); //request 2 bytes
  if (Wire.available() < 2) return 0xFFFF;

  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();
  // Serial.print("msb="); Serial.print(msb, BIN);
  // Serial.print("  lsb="); Serial.println(lsb, BIN);
  // int16_t raw = (msb << 8) | lsb;

  uint16_t raw_u = ((uint16_t)msb << 8) | lsb;   // unsigned
  int16_t  raw_s = ((int16_t)msb << 8) | lsb;    // signed

  Serial.printf("msb=0x%02X  lsb=0x%02X  raw_u=%u  raw_s=%d\n",
                msb, lsb, raw_u, raw_s);

  return raw_u;
}