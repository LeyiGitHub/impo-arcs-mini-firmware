/* SPDX-License-Identifier: MIT */
#include <string.h>
#include "impo_arcs_mini.h"
#include "impo_product_config.h"
#include "impo_link.h"
#include "gadget_voice.h"
#include "gadget_speech.h"
#include "yunbao_wake.h"

static const char *const keywords[] = IMPO_WAKE_KEYWORDS;
static const impo_arcs_mini_product_t product = {
    .id = IMPO_PRODUCT_ID, .display_name = IMPO_DISPLAY_NAME,
    .model_id = IMPO_MODEL_ID, .product_id = IMPO_PLATFORM_PRODUCT_ID,
    .instructions = IMPO_INSTRUCTIONS,
    .continuous_dialogue = IMPO_CONTINUOUS_DIALOGUE,
    .wake_interrupt = IMPO_WAKE_INTERRUPT,
    .voice = { .upload_overlap = IMPO_VOICE_UPLOAD_OVERLAP,
               .endpoint_pause_ms = IMPO_VOICE_ENDPOINT_PAUSE_MS },
    .playback = { IMPO_PLAYBACK_ADAPTIVE, IMPO_PLAYBACK_PREFILL_MS, IMPO_PLAYBACK_SENTENCE_TTS },
    .wake = { .enabled = IMPO_WAKE_ENABLED, .label = IMPO_WAKE_LABEL,
        .keywords = keywords, .keyword_count = IMPO_WAKE_KEYWORD_COUNT,
        .model_file = IMPO_WAKE_MODEL_FILE, .wrap_file = IMPO_WAKE_WRAP_FILE,
        .model_sha256 = IMPO_WAKE_MODEL_SHA256, .wrap_sha256 = IMPO_WAKE_WRAP_SHA256 }
};
const impo_arcs_mini_product_t *impo_arcs_mini_product(void) { return &product; }
int impo_arcs_mini_begin_conversation(void)
{
    if (impo_link_state() != IMPO_LINK_ONLINE ||
        gadget_voice_state() != GADGET_VOICE_IDLE || gadget_voice_continuous_active()) return -1;
    gadget_voice_wake();
    return 0;
}
int impo_arcs_mini_say(const char *text)
{
    if (!text || !*text || strlen(text) > 1000) return -1;
    return gadget_speech_say(text);
}
int impo_arcs_mini_interrupt_conversation(void)
{
    return product.wake_interrupt && gadget_voice_interrupt() ? 0 : -1;
}
int impo_arcs_mini_set_wake_enabled(bool enabled) { return yunbao_wake_set_enabled(enabled); }
bool impo_arcs_mini_wake_available(void) { return gadget_voice_wakeup_available(); }
int impo_arcs_mini_send_event(const char *name, cJSON *data) { return gadget_send_event(name, data); }
int impo_arcs_mini_configure_voice(const impo_arcs_voice_config_t *config)
{ return gadget_voice_configure(config); }
void impo_arcs_mini_voice_snapshot(impo_arcs_voice_snapshot_t *out)
{ gadget_voice_snapshot(out); }
int impo_arcs_mini_configure_reply(bool incremental)
{ return gadget_voice_reply_configure(incremental); }
int impo_arcs_mini_configure_voice_tail(uint32_t keep_ms)
{ return gadget_voice_tail_configure(keep_ms); }

int impo_arcs_mini_configure_playback(const impo_arcs_playback_config_t *c) { return gadget_speech_configure(c); }
void impo_arcs_mini_playback_snapshot(impo_arcs_playback_snapshot_t *out) { gadget_speech_snapshot(out); }
