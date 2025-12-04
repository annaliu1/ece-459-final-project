#pragma once
#include <bluefruit.h>
#include <stdarg.h>
#include "ble_manager.h"

BLEUart bleuart;   // define this in ONE .cpp file only

void ble_init()
{
  Serial.println("starting ble init");

  // Start BLE stack: 1 peripheral, 0 central
  Bluefruit.begin(1, 0);
  Bluefruit.setName("FeatherSense UART testing");
  Bluefruit.setTxPower(4);

  bleuart.begin();
  

  // Simple connect/disconnect logs (optional)
  Bluefruit.Periph.setConnectCallback([](uint16_t connHandle) {
    Serial.println("[BLE] Connected");
  });
  Bluefruit.Periph.setDisconnectCallback([](uint16_t connHandle, uint8_t reason) {
    Serial.printf("[BLE] Disconnected, reason=%d\r\n", reason);
  });

  // Set up advertising
  Bluefruit.Advertising.stop();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.addService(bleuart);

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);

  Serial.println("ble_init done, advertising");
}

// Writes data in safe BLE chunks
void ble_write_bytes_chunked(const uint8_t *data, size_t len) {
  const size_t CHUNK = 20; // safe default (MTU)
  size_t offset = 0;
  while (offset < len) {
    size_t to_write = ((len - offset) > CHUNK) ? CHUNK : (len - offset);
    bleuart.write(data + offset, to_write);
    offset += to_write;
    delay(2); // small pause avoids BLE TX overflow
  }
}

// Prints to both Serial and BLE
void print_both(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n <= 0) return;

  Serial.print(buf); // debug console

  size_t len = (size_t)((n < (int)sizeof(buf)) ? n : (int)sizeof(buf - 1));
  ble_write_bytes_chunked((const uint8_t*)buf, len);
}
