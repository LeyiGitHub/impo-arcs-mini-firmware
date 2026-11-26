/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define IMPO_IMAGE_WIDTH 240
#define IMPO_IMAGE_HEIGHT 240
#define IMPO_IMAGE_MAX_BYTES (256u * 1024u)
#define IMPO_IMAGE_MAX_DIMENSION 512
typedef struct {
    uint32_t image_id, revision;
    unsigned width, height, remaining_seconds;
    bool busy;
    char state[12], error[96];
} impo_arcs_image_snapshot_t;
int impo_arcs_image_init(void);
/* Asynchronous, single download; URL is public HTTPS, duration starts at ready. */
int impo_arcs_image_start(const char *url, unsigned seconds, uint32_t *id, const char **why);
/* Invalidates a pending result as well as the current image. */
void impo_arcs_image_clear(void);
void impo_arcs_image_snapshot(impo_arcs_image_snapshot_t *out);
/* Copies RGB565 host words while locked; caller never retains an owned pointer. */
bool impo_arcs_image_copy(uint16_t *pixels, unsigned width, unsigned height);
/* Internal decoder entry point. Calls must be serialized; worker owns that rule. */
uint16_t *impo_arcs_image_decode(const uint8_t *data, size_t bytes,
                               unsigned *width, unsigned *height, const char **why);
#ifdef __cplusplus
}
#endif
