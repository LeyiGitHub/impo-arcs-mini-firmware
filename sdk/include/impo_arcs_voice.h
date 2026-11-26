/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "impo_arcs_reply_probe.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IMPO_VOICE_IDLE, IMPO_VOICE_LISTENING, IMPO_VOICE_UPLOADING,
    IMPO_VOICE_WAITING, IMPO_VOICE_PREPARING, IMPO_VOICE_PLAYING,
    IMPO_VOICE_CANCELLING, IMPO_VOICE_FAILED
} impo_arcs_voice_phase_t;

typedef struct {
    bool upload_overlap; /* Experimental: unknown-length WAV requires gateway acceptance. */
    uint32_t endpoint_pause_ms; /* 600..2000; conservative default 1200. */
} impo_arcs_voice_config_t;

typedef enum {
    IMPO_VOICE_LAST_VOICE, IMPO_VOICE_CAPTURE_END, IMPO_VOICE_UPLOAD_START,
    IMPO_VOICE_UPLOAD_QUEUED, IMPO_VOICE_SERVER_ACK, IMPO_VOICE_MESSAGE_DONE,
    IMPO_VOICE_TTS_REQUEST, IMPO_VOICE_CANCELLED, IMPO_VOICE_STAGE_COUNT
} impo_arcs_voice_stage_t;

typedef struct {
    uint32_t boot_id, turn_epoch, session_epoch;
    impo_arcs_voice_phase_t phase;
    uint32_t valid_stages;
    uint64_t stage_ms[IMPO_VOICE_STAGE_COUNT]; /* Device monotonic clock only. */
    uint32_t pcm_bytes, queued_bytes, capture_end_backlog_bytes, mic_dropped_bytes;
    uint32_t upload_stack_free_bytes;
    bool upload_overlap, failed;
    /* Per-turn subscription counters; no answer text or message identifiers. */
    uint32_t subscription_bytes, subscription_lines, complete_replies;
    uint32_t missing_parent, text_not_ready, duplicate_replies;
    uint32_t rejected_parent, rejected_state;
    bool subscription_ready;
    impo_arcs_reply_probe_snapshot_t reply_probe;
    bool reply_incremental;
    uint32_t reply_segments, reply_mismatches;
    uint32_t captured_pcm_bytes, trimmed_pcm_bytes, tail_keep_ms;
} impo_arcs_voice_snapshot_t;

/* Task context. Configuration is accepted only between turns. No automatic
 * retry or batch fallback after any part of an overlap request is submitted. */
int impo_arcs_mini_configure_voice(const impo_arcs_voice_config_t *config);
void impo_arcs_mini_voice_snapshot(impo_arcs_voice_snapshot_t *out);
/* Experimental append-only reply playback. Only between conversations. The
 * gateway's live deltas must represent the stable user-visible answer. Default
 * false; on a final-prefix mismatch the current utterance fails explicitly. */
int impo_arcs_mini_configure_reply(bool incremental);
/* Experimental batch-only trailing-silence trimming. 0 disables (default),
 * 200..1000 preserves that many ms after the last VAD voice frame. Does not
 * shorten the endpoint. Refused during a session or with overlap enabled. */
int impo_arcs_mini_configure_voice_tail(uint32_t keep_ms);

#ifdef __cplusplus
}
#endif
