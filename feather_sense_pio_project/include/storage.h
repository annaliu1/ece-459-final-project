#ifndef STORAGE_H
#define STORAGE_H

#include <Adafruit_SPIFlash.h>
#include "sensor_manager.h" 

// Declare global variables (not define)
extern Adafruit_FlashTransport_QSPI flashTransport;
extern Adafruit_SPIFlash flash;
extern const uint32_t FLASH_SECTOR_SIZE;

// Declare functions
bool flash_init();
bool flash_erase_sector(uint32_t sector_index);
bool flash_read(uint32_t addr, uint8_t *buf, size_t len);
bool flash_write(uint32_t addr, const uint8_t *buf, size_t len);
uint32_t flash_sector_size();


// Public API from storage.cpp
bool storage_init(uint32_t flush_interval, size_t ram_buf_size);
void storage_append_record(uint8_t sensor_idx, const sensor_data_t *d);
void storage_flush_now(void);
void storage_upload_over_ble(void);
void storage_erase_all_logs(void);

#endif