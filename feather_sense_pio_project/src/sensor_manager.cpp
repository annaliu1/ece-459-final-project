#include "sensor_manager.h"
#include <string.h>
#include <stdio.h>
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

/* Config */
#define MAX_SENSORS        20
#define DEFAULT_STACK_SIZE 2048
#define DEFAULT_TASK_PRIO  2

typedef struct {
    char name[SENSOR_NAME_MAX];
    sensor_init_cb init;
    sensor_read_cb read;
    sensor_print_cb print;
    void *ctx;
    float freq_hz;
    bool enabled;
    TaskHandle_t task_handle;
    sensor_data_t last_data;
} sensor_t;

static sensor_t sensors[MAX_SENSORS];
static int sensor_count = 0;
static SemaphoreHandle_t bus_mutex = NULL;

/* Bus locking (for shared I2C/SPI resources) */
static inline bool bus_lock(TickType_t timeout_ms) {
    if (!bus_mutex) return true;
    return (xSemaphoreTake(bus_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE);
}
static inline void bus_unlock(void) {
    if (bus_mutex) xSemaphoreGive(bus_mutex);
}

// Exported wrappers so other modules can use the same bus mutex
bool sensor_bus_lock(TickType_t timeout_ms) {
    // bus_lock is static inline above in this file; call it
    return bus_lock(timeout_ms);
}

void sensor_bus_unlock(void) {
    bus_unlock();
}

/* Per-sensor task */
static void sensor_task(void *pvParameters)
{
    sensor_t *s = (sensor_t *)pvParameters;
    TickType_t last_wake = xTaskGetTickCount();
    TickType_t period_ticks = (s->freq_hz > 0.0f) ? pdMS_TO_TICKS((int)(1000.0f / s->freq_hz)) : portMAX_DELAY;

    for (;;) {
        if (!s->enabled || s->freq_hz <= 0.0f) {
            vTaskSuspend(NULL);
            last_wake = xTaskGetTickCount();
            period_ticks = (s->freq_hz > 0.0f) ? pdMS_TO_TICKS((int)(1000.0f / s->freq_hz)) : portMAX_DELAY;
            continue;
        }

        if (bus_lock(100)) {
            sensor_data_t tmp;
            memset(&tmp, 0, sizeof(tmp));
            bool ok = false;
            if (s->read) ok = s->read(s->ctx, &tmp);
            if (ok) {
                taskENTER_CRITICAL();
                s->last_data = tmp;
                s->last_data.timestamp = xTaskGetTickCount();
                taskEXIT_CRITICAL();
            } else {
                taskENTER_CRITICAL();
                s->last_data.len = 0;
                s->last_data.timestamp = xTaskGetTickCount();
                taskEXIT_CRITICAL();
            }
            bus_unlock();
        }

        if (period_ticks == portMAX_DELAY) {
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            vTaskDelayUntil(&last_wake, period_ticks);
        }
    }
}

bool sensor_manager_init(void)
{
    if (bus_mutex == NULL) {
        bus_mutex = xSemaphoreCreateMutex();
        if (!bus_mutex) return false;
    }
    sensor_count = 0;
    memset(sensors, 0, sizeof(sensors));
    return true;
}

int sensor_register(const char *name,
                    sensor_init_cb init_cb,
                    sensor_read_cb read_cb,
                    sensor_print_cb print_cb,
                    void *ctx,
                    float initial_freq_hz,
                    bool start_enabled)
{
    if (sensor_count >= MAX_SENSORS) return -1;
    int idx = sensor_count++;
    sensor_t *s = &sensors[idx];
    strncpy(s->name, name, SENSOR_NAME_MAX-1);
    s->init = init_cb;
    s->read = read_cb;
    s->print = print_cb;
    s->ctx = ctx;
    s->freq_hz = initial_freq_hz;
    s->enabled = start_enabled;
    s->task_handle = NULL;
    s->last_data.len = 0;
    s->last_data.timestamp = 0;

    if (s->init) {
        s->init(s->ctx);
    }

    char tname[16];
    snprintf(tname, sizeof(tname), "sens-%d", idx);
    BaseType_t r = xTaskCreate(sensor_task, tname, DEFAULT_STACK_SIZE, (void*)s, DEFAULT_TASK_PRIO, &s->task_handle);
    if (r != pdPASS) {
        s->task_handle = NULL;
        Serial.printf("Failed to create task %s idx=%d\r\n", tname, idx);
        return -1;
    }
    else {
        Serial.printf("Created task %s idx=%d\r\n", tname, idx);
    }

    if (!start_enabled && s->task_handle) {
        vTaskSuspend(s->task_handle);
    }

    return idx;
}

void sensor_enable(int idx)
{
    if (idx < 0 || idx >= sensor_count) return;
    sensor_t *s = &sensors[idx];
    s->enabled = true;
    if (s->task_handle) vTaskResume(s->task_handle);
}

void sensor_disable(int idx)
{
    if (idx < 0 || idx >= sensor_count) return;
    sensor_t *s = &sensors[idx];
    s->enabled = false;
    if (s->task_handle) vTaskSuspend(s->task_handle);
}

void sensor_set_freq(int idx, float freq_hz)
{
    if (idx < 0 || idx >= sensor_count) return;
    sensor_t *s = &sensors[idx];
    s->freq_hz = freq_hz;
    if (s->enabled && s->task_handle) vTaskResume(s->task_handle);
}

void print_all_sensors(void)
{
    Serial.println("==== SENSORS SNAPSHOT ====");
    for (int i = 0; i < sensor_count; ++i) {
        sensor_t *s = &sensors[i];
        Serial.printf("[%d] %s : enabled=%d freq=%.2fHz last_len=%u ts=%lu\r\n",
               i, s->name, s->enabled ? 1 : 0, s->freq_hz, (unsigned)s->last_data.len, (unsigned long)s->last_data.timestamp);
        if (s->print) s->print(s->ctx, &s->last_data);
        else {
            if (s->last_data.len > 0) {
                Serial.print("  data (hex): ");
                for (size_t b = 0; b < s->last_data.len && b < SENSOR_DATA_BYTES; ++b) {
                    Serial.printf("%02X ", s->last_data.bytes[b]);
                }
                Serial.println();
            } else Serial.println("  (no data)");
        }
    }
    Serial.println("==========================");
}

bool sensor_get_last(int idx, sensor_data_t *out)
{
    if (!out) return false;
    if (idx < 0 || idx >= sensor_count) return false;
    sensor_t *s = &sensors[idx];
    taskENTER_CRITICAL();
    memcpy(out, &s->last_data, sizeof(sensor_data_t));
    taskEXIT_CRITICAL();
    return (out->len > 0);
}

/* Convenience: create periodic printer task */
static TickType_t g_print_period = pdMS_TO_TICKS(2000);
static void sensor_printer_task(void *pv) {
    (void)pv;
    for (;;) {
        print_all_sensors();
        vTaskDelay(g_print_period);
    }
}
BaseType_t create_sensor_printer_task(UBaseType_t priority, uint16_t stack_words, TickType_t period_ms)
{
    g_print_period = pdMS_TO_TICKS(period_ms);
    return xTaskCreate(sensor_printer_task, "sens-pr", stack_words ? stack_words : 4096, NULL, priority ? priority : 1, NULL);
}
