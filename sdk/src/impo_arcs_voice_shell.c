/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include "shell.h"
#include "impo_arcs_mini.h"
#include "platform_arcs.h"

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

static int voice_diagnostics_cmd(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) return -1;
    impo_arcs_voice_snapshot_t s;
    impo_arcs_mini_voice_snapshot(&s);
    shellPrint(shellGetCurrent(), "voice_sub ready=%u bytes=%u lines=%u complete=%u\n",
        (unsigned)s.subscription_ready, (unsigned)s.subscription_bytes,
        (unsigned)s.subscription_lines, (unsigned)s.complete_replies);
    shellPrint(shellGetCurrent(), "voice_filter missing_parent=%u not_ready=%u duplicate=%u\n",
        (unsigned)s.missing_parent, (unsigned)s.text_not_ready, (unsigned)s.duplicate_replies);
    shellPrint(shellGetCurrent(), "voice_reject parent=%u state=%u shell_stack_free=%u\n",
        (unsigned)s.rejected_parent, (unsigned)s.rejected_state,
        (unsigned)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)));
    plat_crash_report_previous();
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 voice_diagnostics, voice_diagnostics_cmd, privacy-preserving reply and reset counters);

static int playback_cmd(int argc,char **argv)
{
    if(argc!=1) {
        if(argc!=5 || (strcmp(argv[1],"complete") && strcmp(argv[1],"adaptive")))return -1;
        char *end=NULL;unsigned long ms=strtoul(argv[2],&end,10);
        if(!end||*end||ms<600||ms>2000 || (strcmp(argv[3],"full")&&strcmp(argv[3],"sentence")) || strcmp(argv[4],"apply"))return -1;
        impo_arcs_playback_config_t c={!strcmp(argv[1],"adaptive"),(uint32_t)ms,!strcmp(argv[3],"sentence")};
        if(impo_arcs_mini_configure_playback(&c)) {shellPrint(shellGetCurrent(),"playback busy; unchanged\n");return -1;}
    }
    impo_arcs_playback_snapshot_t s;impo_arcs_mini_playback_snapshot(&s);
    shellPrint(shellGetCurrent(),"playback gen=%u active=%u failed=%u cancelled=%u adaptive=%u sentence=%u\n",s.generation,s.active,s.failed,s.cancelled,s.adaptive,s.sentence_tts);
    shellPrint(shellGetCurrent(),"playback samples played=%u queued=%u underruns=%u gap_ms=%u prefill_ms=%u\n",s.played_samples,s.queued_samples,s.underruns,s.underrun_ms,s.prefill_ms);
    shellPrint(shellGetCurrent(),"playback ingress high=%u overflow=%u bytes=%u stack=%u segments=%u/%u\n",s.ingress_high_water,s.ingress_overflows,s.received_bytes,s.worker_stack_free,s.segments_decoded,s.segments_requested);
    shellPrint(shellGetCurrent(),"playback timing request=%llu body=%llu pcm=%llu first=%llu last=%llu\n",(unsigned long long)s.request_ms,(unsigned long long)s.first_body_ms,(unsigned long long)s.first_pcm_ms,(unsigned long long)s.first_write_ms,(unsigned long long)s.last_write_ms);
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN,
                 playback,playback_cmd,playback configuration and counters);
