#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <Adafruit_TinyUSB.h>

#define SENSOR_NAME_MAX    24
#define SENSOR_DATA_BYTES  64

typedef struct {
    uint8_t bytes[SENSOR_DATA_BYTES];
    size_t len;
    TickType_t timestamp;
} sensor_data_t;

/* sensor callbacks expected by manager */
typedef bool (*sensor_init_cb)(void *ctx);
typedef bool (*sensor_read_cb)(void *ctx, sensor_data_t *out);
typedef void (*sensor_print_cb)(void *ctx, const sensor_data_t *d);

/* Register a sensor. Returns index or -1 on error. */
int sensor_register(const char *name,
                    sensor_init_cb init_cb,
                    sensor_read_cb read_cb,
                    sensor_print_cb print_cb,
                    void *ctx,
                    float initial_freq_hz,
                    bool start_enabled);

/* Control */
void sensor_enable(int idx);
void sensor_disable(int idx);
void sensor_set_freq(int idx, float freq_hz);

/* Query / print */
void print_all_sensors(void);
bool sensor_get_last(int idx, sensor_data_t *out);

/* Initialization */
bool sensor_manager_init(void);

/* Create a periodic printer task (optional convenience) */
BaseType_t create_sensor_printer_task(UBaseType_t priority, uint16_t stack_words, TickType_t period_ms);

#endif /* SENSOR_MANAGER_H */
