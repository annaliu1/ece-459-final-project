#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#pragma once
#include <bluefruit.h>
#include <stdint.h>
#include <stddef.h>

extern BLEUart bleuart;   // define this in ONE .cpp file only

#ifdef __cplusplus
extern "C" {
}
#endif

void my_connect_cb();
void my_disconnect_cb();

void ble_init(void);
void startAdv(void);
void ble_write_bytes_chunked(const uint8_t *data, size_t len);
void print_both(const char *fmt, ...);

#endif