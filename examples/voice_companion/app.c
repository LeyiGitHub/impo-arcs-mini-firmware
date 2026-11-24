/* SPDX-License-Identifier: MIT
 * Product JSON selects the upload policy; the SDK owns capture and transport. */
#include "impo_arcs_mini.h"

static cJSON *voice_status(const cJSON *params)
{
    (void)params;
    impo_arcs_voice_snapshot_t snapshot;
    impo_arcs_mini_voice_snapshot(&snapshot);
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "sdk_version", IMPO_ARCS_MINI_SDK_VERSION);
    cJSON_AddStringToObject(result, "default_upload_mode",
        impo_arcs_mini_product()->voice.upload_overlap ? "overlap" : "batch");
    cJSON_AddNumberToObject(result, "phase", snapshot.phase);
    cJSON_AddNumberToObject(result, "turn", snapshot.turn_epoch);
    cJSON_AddNumberToObject(result, "valid_stages", snapshot.valid_stages);
    cJSON_AddNumberToObject(result, "pcm_bytes", snapshot.pcm_bytes);
    cJSON_AddNumberToObject(result, "queued_bytes", snapshot.queued_bytes);
    cJSON_AddNumberToObject(result, "mic_dropped_bytes", snapshot.mic_dropped_bytes);
    cJSON_AddBoolToObject(result, "failed", snapshot.failed);
    return gadget_command_ok_payload(result);
}

static const gadget_command_t commands[] = {
    { "companion.voice_status", "Return voice phase and the latest turn counters.",
      NULL, NULL, voice_status }
};

const impo_arcs_mini_application_t *impo_arcs_mini_application(void)
{
    static const impo_arcs_mini_application_t app = {
        .commands = commands, .command_count = sizeof(commands) / sizeof(commands[0])
    };
    return &app;
}
