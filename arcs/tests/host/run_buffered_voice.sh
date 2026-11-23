#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${ARCS_BASE:?Set ARCS_BASE to the ListenAI ARCS SDK 0.1.6 directory}"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
G=../../gadget
J=$ARCS_BASE/modules/cjson
# glibc exposes the SDK's M_PI constant with this host-only feature macro.
"${CC:-cc}" -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -Iruntime_stubs -I"$G" \
  test_sound_buffer.c "$G/gadget_sound.c" "$G/gadget_pcm.c" -lm -o "$output/sound"
"$output/sound"
"${CC:-cc}" -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror \
  -I"$J" -Iruntime_stubs -I"$G" -I../../link \
  test_voice_capture.c "$G/gadget_voice_logic.c" "$G/gadget_pcm.c" "$J/cJSON.c" -lm -o "$output/voice"
"$output/voice"
