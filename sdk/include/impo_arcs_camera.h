/* SPDX-License-Identifier: MIT */
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define IMPO_CAMERA_WIDTH 320
#define IMPO_CAMERA_HEIGHT 240
#define IMPO_CAMERA_JPEG_MAX (48u * 1024u)
/* Encode a native VGA RGB565 byte stream, MSB first, into a QVGA JPEG.
 * Returns a malloc-owned buffer on success. Never uses JPEG/DMA hardware.
 * Calls must run on a worker task, not the link task. */
int impo_arcs_camera_encode(const uint8_t *pixels, size_t bytes,
                           uint8_t **jpeg, size_t *length);
#ifdef __cplusplus
}
#endif
