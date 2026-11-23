/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    bool initialized, last, stable, boot_press;
    unsigned same;
    uint32_t pressed_at;
} yunbao_button_t;
/* 1=MAIN_PRESS, 2=MAIN_RELEASE, 4=AUX_PRESS, 8=AUX_RELEASE. */
unsigned yunbao_button_poll(yunbao_button_t *s, bool down, uint32_t now_ms);
