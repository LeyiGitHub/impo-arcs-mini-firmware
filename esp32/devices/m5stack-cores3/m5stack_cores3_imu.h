/*
 * Copyright (c) Impo contributors.
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "cJSON.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "impo_commands.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The BMI270 and its watcher (imu.cpp); ESP_ERR_NOT_FOUND if it is absent. */
esp_err_t m5stack_cores3_imu_start(i2c_master_bus_handle_t bus);

/* Called on the watcher's task on every shake, with the peak in g. */
typedef void (*m5stack_cores3_shake_cb_t)(float peak_g);
void m5stack_cores3_imu_on_shake(m5stack_cores3_shake_cb_t cb);

/* imu.read (CAPABILITIES.md). */
cJSON *m5stack_cores3_imu_read(const cJSON *params);

#ifdef __cplusplus
}
#endif
