/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include "shell.h"
#include "impo_arcs_mini.h"

static int voice_latency_cmd(int argc, char **argv)
{
    if (argc != 1) {
        char *end = NULL;
        unsigned long pause = argc == 3 ? strtoul(argv[2], &end, 10) : 0;
        if (argc != 3 || (strcmp(argv[1], "batch") && strcmp(argv[1], "overlap")) ||
            !end || *end || pause < 600 || pause > 2000) return -1;
        impo_arcs_voice_config_t config = { !strcmp(argv[1], "overlap"), (uint32_t)pause };
        if (impo_arcs_mini_configure_voice(&config) != 0) {
            shellPrint(shellGetCurrent(), "voice busy; configuration unchanged\n");
            return -1;
        }
        shellPrint(shellGetCurrent(), "voice configured mode=%s endpoint_ms=%lu\n", argv[1], pause);
    }
    impo_arcs_voice_snapshot_t s;
    impo_arcs_mini_voice_snapshot(&s);
    /* The SDK shell has a bounded formatting buffer. Keep each line short so
     * the byte counters and stack watermark are not silently truncated. */
    shellPrint(shellGetCurrent(), "voice phase=%u boot=%08x epoch=%u session=%u overlap=%u valid=%x failed=%u\n",
        (unsigned)s.phase, (unsigned)s.boot_id, (unsigned)s.turn_epoch, (unsigned)s.session_epoch,
        (unsigned)s.upload_overlap, (unsigned)s.valid_stages, (unsigned)s.failed);
    shellPrint(shellGetCurrent(), "voice bytes pcm=%u queued=%u backlog=%u dropped=%u upload_stack_free=%u\n",
        (unsigned)s.pcm_bytes,
        (unsigned)s.queued_bytes, (unsigned)s.capture_end_backlog_bytes,
        (unsigned)s.mic_dropped_bytes, (unsigned)s.upload_stack_free_bytes);
    for (unsigned i = 0; i < IMPO_VOICE_STAGE_COUNT; i++)
        if (s.valid_stages & (1u << i))
            shellPrint(shellGetCurrent(), "voice stage=%u mono_ms=%llu\n", i, (unsigned long long)s.stage_ms[i]);
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 voice_latency, voice_latency_cmd, voice latency or idle configuration);
