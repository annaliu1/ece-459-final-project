#include <Adafruit_SPIFlash.h>
#include <Adafruit_TinyUSB.h>
#include "storage.h"

Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);
const uint32_t FLASH_SECTOR_SIZE = 4096;

// Helper wrappers (used by storage module)
bool flash_init() {
  if (!flash.begin()) {
    return false;
  }
  return true;
}

bool flash_erase_sector(uint32_t sector_index) {
  bool r = flash.eraseSector(sector_index);
  flash.waitUntilReady();
  return r;
}

bool flash_read(uint32_t addr, uint8_t *buf, size_t len) {
  if (!buf || len == 0) return false;
  flash.readBuffer(addr, buf, len);
  return true;
}

bool flash_write(uint32_t addr, const uint8_t *buf, size_t len) {
  if (!buf || len == 0) return false;
  flash.writeBuffer(addr, (uint8_t*)buf, len);
  flash.waitUntilReady();
  return true;
}

// Expose useful info (approx)
uint32_t flash_sector_size() { return FLASH_SECTOR_SIZE; }
// Optionally we could add flash.totalSize() if supported by library; skip for now.
