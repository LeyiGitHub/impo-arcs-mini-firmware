/*
 * Copyright (c) Impo contributors.
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "cJSON.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

/*
 * The M5Stack StackChan's body under its CoreS3: the head's two servos, the
 * touch strip on top of the head and the 12 RGB LEDs. board_m5stack_cores3.c
 * starts it and lists the commands (CAPABILITIES.md, head.* and lights.*).
 */
esp_err_t stackchan_head_init(i2c_master_bus_handle_t bus);

cJSON *stackchan_head_look(const cJSON *params);
cJSON *stackchan_head_nod(const cJSON *params);
cJSON *stackchan_head_shake(const cJSON *params);
cJSON *stackchan_head_center(const cJSON *params);
cJSON *stackchan_head_status(const cJSON *params);
cJSON *stackchan_lights_set(const cJSON *params);
cJSON *stackchan_lights_off(const cJSON *params);
