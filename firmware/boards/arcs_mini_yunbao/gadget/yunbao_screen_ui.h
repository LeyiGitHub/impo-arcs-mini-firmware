/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "gadget_board.h"
#define YUNBAO_SCREEN_WIDTH 240
#define YUNBAO_SCREEN_HEIGHT 240
/* RGB565 host words; the SDK SPI bus sends each word MSB first. */
bool yunbao_screen_render(uint16_t *pixels, unsigned width, unsigned height,
                          gadget_status_t status, uint32_t time_ms, bool color_test);
