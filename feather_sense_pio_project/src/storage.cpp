// src/storage.cpp
#include "storage.h"
#include "ble_manager.h"
#include <semphr.h>
#include "sensor_manager.h"
#include <bluefruit.h>

// ---- Configurable constants ----
static const uint32_t FLASH_LOG_BASE = 0x000000;         // base address to store logs
static const uint32_t FLASH_LOG_MAX_BYTES = 512 * 1024;  // how many bytes reserved for logs (512KB)
static const size_t DEFAULT_RAM_BUF = 4*1024;            // default in-RAM batch buffer
static const uint32_t DEFAULT_FLUSH_MS = 60 * 1000;      // flush interval

// Upload chunk sizes
static const size_t CHUNK_UPLOAD_PAYLOAD = 180; // safe payload to fit typical ATT MTU

// ---- Internal state ----
static uint8_t *ram_buf = NULL;
static size_t ram_len = 0;
static size_t ram_capacity = 0;
static SemaphoreHandle_t ram_mutex = NULL;
static TaskHandle_t flush_task_handle = NULL;
static uint32_t flush_interval_ms = DEFAULT_FLUSH_MS;

// Tracks where next write should go in flash (absolute address)
static uint32_t flash_write_ptr = FLASH_LOG_BASE;
static bool flash_initialized = false;

/* Helper: find current write pointer by scanning flash for first 0xFF byte */
static uint32_t find_write_ptr() {
  // Scan in chunks for first 0xFF byte (free space)
  const size_t SCAN_CHUNK = 256;
  uint8_t buf[SCAN_CHUNK];
  uint32_t max_addr = FLASH_LOG_BASE + FLASH_LOG_MAX_BYTES;
  uint32_t addr = FLASH_LOG_BASE;

  while (addr < max_addr) {
    uint32_t read_len = SCAN_CHUNK;
    if (addr + read_len > max_addr) read_len = max_addr - addr;
    flash_read(addr, buf, read_len);
    // search for 0xFF byte in chunk
    for (size_t i = 0; i < read_len; ++i) {
      if (buf[i] == 0xFF) {
        // return this byte address as first free location
        return addr + (uint32_t)i;
      }
    }
    addr += read_len;
  }
  Serial.printf("[STOR] find_write_ptr => 0x%08X\n", (unsigned)flash_write_ptr);
  // full — wrap to base (we'll erase when writing)
  return FLASH_LOG_BASE;

}

// Erase whole log region (simple; erases sectors from base to base+MAX)
static bool erase_log_region() {
  uint32_t sectors = (FLASH_LOG_MAX_BYTES + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
  for (uint32_t i = 0; i < sectors; ++i) {
    uint32_t sec_idx = (FLASH_LOG_BASE / FLASH_SECTOR_SIZE) + i;
    if (!flash_erase_sector(sec_idx)) {
      // try again? for demo just fail
      return false;
    }
  }
  return true;
}

// Append bytes to RAM buffer (internal, caller should hold mutex)
static void ram_append_locked(const uint8_t *src, size_t len) {
  if (!src || len == 0) return;
  // If not enough space, drop oldest data to make room (simple policy)
  if (ram_len + len > ram_capacity) {
    size_t keep = ram_capacity - len;
    if (keep > 0) {
      memmove(ram_buf, ram_buf + (ram_len - keep), keep);
      ram_len = keep;
    } else {
      // new len larger than capacity -- drop everything and only keep last portion
      ram_len = 0;
    }
  }
  memcpy(ram_buf + ram_len, src, len);
  ram_len += len;
}

// Public API: append record [4-byte ts][1-byte sensor_idx][1-byte len][payload]
void storage_append_record(uint8_t sensor_idx, const sensor_data_t *d) {
  if (!d || d->len == 0) return;
  if (!ram_mutex) return;
  if (!flash_initialized) {
    // try to initialize flash if not ready
    flash_initialized = flash_init();
    if (!flash_initialized) return;
    flash_write_ptr = find_write_ptr();
  }

  xSemaphoreTake(ram_mutex, portMAX_DELAY);

  uint8_t hdr[6];
  uint32_t ts = (uint32_t)millis();
  hdr[0] = (uint8_t)((ts >> 24) & 0xFF);
  hdr[1] = (uint8_t)((ts >> 16) & 0xFF);
  hdr[2] = (uint8_t)((ts >> 8) & 0xFF);
  hdr[3] = (uint8_t)(ts & 0xFF);
  hdr[4] = sensor_idx;
  hdr[5] = (uint8_t)(d->len & 0xFF);

  ram_append_locked(hdr, sizeof(hdr));
  ram_append_locked(d->bytes, d->len);

  xSemaphoreGive(ram_mutex);

  Serial.printf("[STOR] appended rec sensor=%u len=%u ram_len=%u\n",
              (unsigned)sensor_idx, (unsigned)d->len, (unsigned)ram_len);
}

// Flush RAM buffer to flash now
void storage_flush_now(void) {
  if (!flash_initialized) {
    flash_initialized = flash_init();
    if (!flash_initialized) {
      // cannot access flash; drop RAM to avoid unbounded growth
      if (ram_mutex) {
        xSemaphoreTake(ram_mutex, portMAX_DELAY);
        ram_len = 0;
        xSemaphoreGive(ram_mutex);
      }
      return;
    }
  }

  // Quick copy RAM content under lock so we hold lock for minimal time
  if (!ram_mutex) return;
  xSemaphoreTake(ram_mutex, portMAX_DELAY);
  if (ram_len == 0) {
    xSemaphoreGive(ram_mutex);
    return;
  }
  uint8_t *tmp = (uint8_t *)malloc(ram_len);
  if (!tmp) {
    // allocation failed; drop buffer
    ram_len = 0;
    xSemaphoreGive(ram_mutex);
    return;
  }
  memcpy(tmp, ram_buf, ram_len);
  size_t write_len = ram_len;
  ram_len = 0;
  xSemaphoreGive(ram_mutex);

  // Ensure there is space in our reserved log region
  uint32_t max_addr = FLASH_LOG_BASE + FLASH_LOG_MAX_BYTES;
  if (flash_write_ptr + (uint32_t)write_len > max_addr) {
    // Not enough space -> erase region and start at base.
    if (!erase_log_region()) {
      // cannot erase -> drop the data
      free(tmp);
      return;
    }
    flash_write_ptr = FLASH_LOG_BASE;
  }

  // Before writing, must ensure sectors containing the destination are erased.
  // Our simple approach: erase every sector that overlaps write_ptr..write_ptr+write_len-1
  uint32_t start_sector = flash_write_ptr / FLASH_SECTOR_SIZE;
  uint32_t end_sector = (flash_write_ptr + write_len - 1) / FLASH_SECTOR_SIZE;
  for (uint32_t s = start_sector; s <= end_sector; ++s) {
    if (!flash_erase_sector(s)) {
      // erase failed — abort and drop data to avoid repeated attempts
      free(tmp);
      return;
    }
  }

  // Write the data
  bool ok = flash_write(flash_write_ptr, tmp, write_len);
  free(tmp);
  if (!ok) {
    // write failed: we give up for now
    Serial.println("[STOR] flash write failed!");
    return;
  }
  flash_write_ptr += (uint32_t)write_len;
  Serial.printf("[STOR] flushed %u bytes -> new flash_ptr=0x%08X\n",
                  (unsigned)write_len, (unsigned)flash_write_ptr);
}

// Periodic flush task
static void flush_task(void *pv) {
  (void)pv;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(flush_interval_ms));
    storage_flush_now();
  }
}

// Upload logs over BLE by streaming bytes in chunks
void storage_upload_over_ble(void) {
  if (!Bluefruit.connected()) return;
  if (!flash_initialized) {
    flash_initialized = flash_init();
    if (!flash_initialized) return;
  }

  // Flush RAM first so we include latest data
  storage_flush_now();

  // calculate how many bytes stored: find write_ptr by reading pointer (we already track flash_write_ptr)
  uint32_t ptr = flash_write_ptr;
  uint32_t read_addr = FLASH_LOG_BASE;

  // Stream in CHUNK_UPLOAD_PAYLOAD sized blocks
  uint8_t payload[CHUNK_UPLOAD_PAYLOAD];
  uint32_t seq = 0;

  while (read_addr < ptr) {
    size_t to_read = CHUNK_UPLOAD_PAYLOAD;
    if (read_addr + to_read > ptr) to_read = ptr - read_addr;
    flash_read(read_addr, payload, to_read);
    Serial.printf("[STOR] upload chunk seq=%u addr=0x%08X len=%u\n", (unsigned)seq, (unsigned)read_addr, (unsigned)to_read);
    // send small chunk header, then payload
    uint8_t hdr[7];
    hdr[0] = 'B';
    hdr[1] = (uint8_t)((seq >> 24) & 0xFF);
    hdr[2] = (uint8_t)((seq >> 16) & 0xFF);
    hdr[3] = (uint8_t)((seq >> 8) & 0xFF);
    hdr[4] = (uint8_t)(seq & 0xFF);
    hdr[5] = (uint8_t)((to_read >> 8) & 0xFF);
    hdr[6] = (uint8_t)(to_read & 0xFF);

    // write header + payload to BLE UART (existing bleuart.write)
    bleuart.write(hdr, sizeof(hdr));
    // small delay to avoid overflowing BLE buffers — tune if your BLE stack supports bigger queues
    vTaskDelay(pdMS_TO_TICKS(2));
    bleuart.write(payload, to_read);
    vTaskDelay(pdMS_TO_TICKS(2));

    read_addr += to_read;
    seq++;
  }
}

// Erase all logs in the region (useful for app-requested cleanup)
void storage_erase_all_logs(void) {
  if (!flash_initialized) flash_initialized = flash_init();
  erase_log_region();
  flash_write_ptr = FLASH_LOG_BASE;
}

// Initialize storage
bool storage_init(uint32_t flush_interval, size_t ram_buf_size) {
  flush_interval_ms = (flush_interval == 0) ? DEFAULT_FLUSH_MS : flush_interval;
  ram_capacity = (ram_buf_size == 0) ? DEFAULT_RAM_BUF : ram_buf_size;

  // allocate RAM buffer
  ram_buf = (uint8_t *)malloc(ram_capacity);
  if (!ram_buf) return false;
  ram_len = 0;

  ram_mutex = xSemaphoreCreateMutex();
  if (!ram_mutex) {
    free(ram_buf);
    ram_buf = NULL;
    return false;
  }

  // Init flash and determine write pointer
  flash_initialized = flash_init();
  if (!flash_initialized) {
    // still allow operation; we'll drop data or retry when flushing
    flash_initialized = false;
  } else {
    flash_write_ptr = find_write_ptr();
  }

  // Create flush task
  BaseType_t r = xTaskCreate(flush_task, "stor-flush", 4096, NULL, 1, &flush_task_handle);
  if (r != pdPASS) {
    // cleanup
    vSemaphoreDelete(ram_mutex);
    ram_mutex = NULL;
    free(ram_buf);
    ram_buf = NULL;
    return false;
  }
  return true;
}
