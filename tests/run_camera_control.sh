#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${ARCS_BASE:?Set ARCS_BASE to a ListenAI SDK (0.1.6 or 0.1.8)}"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
    -DCONFIG_LISA_AUDIO_RECORD_DMA_CHN_LEFT=1 \
    -DCONFIG_LISA_AUDIO_RECORD_DMA_CHN_RIGHT=3 \
    -DCONFIG_LISA_AUDIO_PLAY_DMA_CHN=2 \
    -Isdk/include -Itests/camera_control_stubs -I"$ARCS_BASE/drivers/lisa_camera" \
    -I"$ARCS_BASE/drivers/lisa_device" -Ifirmware/boards/arcs_mini_yunbao \
    tests/test_yunbao_camera.c -o "$output/camera"
"$output/camera"
