/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Observation only. No text, message IDs or credentials leave this snapshot.
 * A matching prefix in a finite sample is not a server stability guarantee. */
typedef struct {
    uint32_t starts, appends, dones, fulls, other_deltas;
    uint32_t matched_finals, append_bytes, sentence_bytes;
    uint32_t duplicate_seq, sequence_gaps, not_ready, errors;
    uint64_t first_append_ms, first_sentence_ms, final_ms;
    bool final_prefix_matches, final_equals_appends;
} impo_arcs_reply_probe_snapshot_t;

/* Internal SDK integration. Caller serializes reset/observe with the voice
 * lock; storage is bounded and allocated in task context, never in an ISR. */
void impo_arcs_reply_probe_reset(void);
void impo_arcs_reply_probe_observe(const char *line, uint64_t now,
    const char *parent_a, const char *parent_b, impo_arcs_reply_probe_snapshot_t *out);

/* Experimental streaming consumer: caller holds the same voice lock, and only
 * consumes a new message after the previous message's sound is idle.
 * 0 waiting, 1 allocated segment, 2 finish-only, -1 mismatch/overflow. */
int impo_arcs_reply_probe_take(const char *parent_a,const char *parent_b,
                             char **text,bool *last,uint64_t now);

void impo_arcs_reply_probe_refresh(const char *parent_a,const char *parent_b,
                                 impo_arcs_reply_probe_snapshot_t *out);
