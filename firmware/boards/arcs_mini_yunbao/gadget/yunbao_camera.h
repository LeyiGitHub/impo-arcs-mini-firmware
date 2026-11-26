/* SPDX-License-Identifier: MIT */
#pragma once
#include <stddef.h>
#include <stdint.h>
/* One bounded capture; all frames released and DMA stopped on return.
 * Success transfers a malloc-owned JPEG to the caller. */
int yunbao_camera_capture(uint8_t **jpeg, size_t *length);
