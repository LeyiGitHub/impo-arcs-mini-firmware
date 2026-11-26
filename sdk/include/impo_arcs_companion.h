/* SPDX-License-Identifier: MIT */
#pragma once
#include "impo_arcs_mini.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Default companion commands/events, reusable by applications without a main(). */
const impo_arcs_mini_application_t *impo_arcs_companion_application(void);
/* Board display snapshot: copies, never exposes mutable UI storage. */
typedef struct {
    char expression[12], text[193];
    uint32_t revision, image_revision;
    bool timer_running;
    unsigned timer_remaining;
} impo_arcs_companion_view_t;
void impo_arcs_companion_view(impo_arcs_companion_view_t *out);
void impo_arcs_companion_restore(void);
/* True for valid supported UTF-8 which fits three 216-pixel lines. */
bool impo_arcs_companion_text_supported(const char *text);
#ifdef __cplusplus
}
#endif
