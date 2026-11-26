/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define IMPO_RECEIPT_HISTORY 8
typedef enum {
    IMPO_PLAYBACK_FETCHING, IMPO_PLAYBACK_PLAYING, IMPO_PLAYBACK_COMPLETED,
    IMPO_PLAYBACK_INTERRUPTED, IMPO_PLAYBACK_FAILED
} impo_arcs_receipt_state_t;
typedef struct {
    uint32_t playback_id;
    impo_arcs_receipt_state_t state;
    uint64_t accepted_ms, started_ms, ended_ms;
    char kind[16], error[48];
    bool pending_event, notify;
} impo_arcs_receipt_t;
/* Pure bounded history. Caller holds the sound mutex. */
typedef struct {
    impo_arcs_receipt_t entries[IMPO_RECEIPT_HISTORY];
    unsigned next;
    uint32_t evicted_events;
} impo_arcs_receipts_t;
void impo_arcs_receipts_begin(impo_arcs_receipts_t *, uint32_t, const char *, uint64_t);
void impo_arcs_receipts_update(impo_arcs_receipts_t *, uint32_t, impo_arcs_receipt_state_t, const char *, uint64_t);
bool impo_arcs_receipts_get(const impo_arcs_receipts_t *, uint32_t, impo_arcs_receipt_t *);
const char *impo_arcs_receipt_state_name(impo_arcs_receipt_state_t);
#ifdef __cplusplus
}
#endif
