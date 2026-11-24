#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${ARCS_BASE:?Set ARCS_BASE to the ListenAI ARCS SDK 0.1.6 directory}"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
J=$ARCS_BASE/modules/cjson
G=upstream/arcs/gadget
"${CC:-cc}" -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror \
  -pthread -Isdk/include -Itests/voice_stubs -I"$J" -Itests/audio_stubs -I"$G" -Iupstream/arcs/link \
  tests/test_voice_capture.c "$G/gadget_voice_logic.c" "$G/gadget_pcm.c" "$J/cJSON.c" -lm -o "$output/voice"
"$output/voice"
"${CC:-cc}" -std=c11 -D_POSIX_C_SOURCE=200809L -pthread -Wall -Wextra -Werror \
  -Itests/voice_stubs -Itests/audio_stubs -Itests/cloud_stubs -Iupstream/arcs/link \
  tests/test_link_streams.c -o "$output/streams"
"$output/streams"
B=firmware/boards/arcs_mini_yunbao/gadget
M=$ARCS_BASE/soc/arcs/hal/chip/arcs/mbedtls/mbedtls-2.28.9
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests -DMBEDTLS_CONFIG_FILE='"host_sha256_config.h"' -I"$J" -Isdk/include -I"$M/include" \
  tests/test_wake_resources.c sdk/src/impo_arcs_wake_resources.c "$J/cJSON.c" "$M/library/sha256.c" \
  "$M/library/platform_util.c" -lm -o "$output/wake"
if [ -n "${WAKE_FLASH_BACKUP:-}" ]; then
    "$output/wake" "$WAKE_FLASH_BACKUP"
else
    "$output/wake"
fi
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/audio_stubs \
  tests/test_mic_ownership.c -o "$output/mic"
"$output/mic"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -DCONFIG_LISA_AUDIO_RECORD_INDIVIDUAL_GAIN \
  -Itests/audio_stubs tests/test_mic_ownership.c -o "$output/mic-reference"
"$output/mic-reference"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Isdk/include -I"$B" -I"$G" \
  tests/test_wake_policy.c -o "$output/wake-policy"
"$output/wake-policy"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"$J" -Itests/audio_stubs -I"$G" \
  -Iupstream/arcs/link tests/test_speech_cancel.c "$G/gadget_speech.c" "$G/gadget_pcm.c" \
  "$J/cJSON.c" -lm -o "$output/speech-cancel"
"$output/speech-cancel"
