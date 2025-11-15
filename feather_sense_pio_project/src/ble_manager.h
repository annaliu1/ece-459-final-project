#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#pragma once
#include <bluefruit.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
}
#endif


// BLE Service
extern BLEDfu  bledfu;  // OTA DFU service
extern BLEDis  bledis;  // device information
extern BLEUart bleuart; // uart over ble
extern BLEBas  blebas;  // battery 

void ble_init(void);
void startAdv(void);
void ble_write(char *buf, uint16_t size);
void ble_write_bytes_chunked(const uint8_t *data, size_t len);
void print_both(const char *fmt, ...);

#endif