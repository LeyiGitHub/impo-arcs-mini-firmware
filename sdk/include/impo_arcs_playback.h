/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    bool adaptive;          /* false: complete/25 second legacy buffering */
    uint32_t prefill_ms;    /* 600..2000, initial adaptive target */
    bool sentence_tts;      /* prefer sentence boundaries; every request is capped at 120 codepoints */
} impo_arcs_playback_config_t;
typedef struct {
    uint32_t generation, ingress_high_water, ingress_overflows, received_bytes;
    uint32_t segments_requested, segments_decoded, worker_stack_free;
    uint32_t queued_samples, played_samples, underruns, underrun_ms, prefill_ms;
    uint64_t request_ms, first_body_ms, first_pcm_ms, first_write_ms, last_write_ms;
    bool active, failed, cancelled, rebuffering, adaptive, sentence_tts;
    uint64_t first_submit_ms, first_status_ms; /* local stream open and response-header callback; not wire/ASR timing */
} impo_arcs_playback_snapshot_t;
/* Task context. Changes are accepted only when speech and sound are idle. */
int impo_arcs_mini_configure_playback(const impo_arcs_playback_config_t *config);
void impo_arcs_mini_playback_snapshot(impo_arcs_playback_snapshot_t *out);
#ifdef __cplusplus
}
#endif
