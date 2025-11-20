#pragma once
#include <Arduino.h>
#include "sensor_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

bool spo2_init_fusion_adapter(void *ctx);
bool spo2_read_fusion_adapter(void *ctx, sensor_data_t *out);
void spo2_print_fusion_adapter(void *ctx, const sensor_data_t *d);

#ifdef __cplusplus
}
#endif