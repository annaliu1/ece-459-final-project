#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <bluefruit.h>

// BLE Service
extern BLEDfu  bledfu;  // OTA DFU service
extern BLEDis  bledis;  // device information
extern BLEUart bleuart; // uart over ble
extern BLEBas  blebas;  // battery 

void ble_init(void);
void startAdv(void);
void ble_write(char *buf, uint16_t size);

#endif