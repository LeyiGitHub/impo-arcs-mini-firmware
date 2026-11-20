/*
 * Copyright (c) Impo contributors.
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "camera.h"
#include "esp_err.h"

/* The GC0308 backend (camera.c), for camera_register(). */
const camera_driver_t *m5stack_cores3_camera(void);

/* Starts the stream now, while internal RAM is free for its DMA buffer
 * (camera.c). A failed start is tried again on the first photo. */
esp_err_t m5stack_cores3_camera_start(void);
