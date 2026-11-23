#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${ARCS_BASE:?Set ARCS_BASE to the ListenAI ARCS SDK 0.1.6 directory}"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
J=$ARCS_BASE/modules/cjson
G=upstream/arcs/gadget
"${CC:-cc}" -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror \
  -I"$J" -Itests/audio_stubs -I"$G" -Iupstream/arcs/link \
  tests/test_voice_capture.c "$G/gadget_voice_logic.c" "$G/gadget_pcm.c" "$J/cJSON.c" -lm -o "$output/voice"
"$output/voice"
