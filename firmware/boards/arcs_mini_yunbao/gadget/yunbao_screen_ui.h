/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "gadget_board.h"
#include "impo_arcs_voice.h"
#include "impo_arcs_companion.h"
#define YUNBAO_SCREEN_WIDTH 240
#define YUNBAO_SCREEN_HEIGHT 240
/* RGB565 host words; the SDK SPI bus sends each word MSB first. */
bool yunbao_screen_render(uint16_t *pixels, unsigned width, unsigned height,
                          gadget_status_t status, uint32_t time_ms, bool color_test, bool wake_available, bool dialogue_active);
bool yunbao_screen_render_voice(uint16_t *pixels, unsigned width, unsigned height,
    gadget_status_t status, uint32_t time_ms, bool color_test, bool wake_available,
    bool dialogue_active, impo_arcs_voice_phase_t voice_phase);
bool yunbao_screen_render_companion(uint16_t *, unsigned, unsigned, gadget_status_t,
    uint32_t, bool, bool, bool, impo_arcs_voice_phase_t, const impo_arcs_companion_view_t *);
