#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
board=firmware/boards/arcs_mini_yunbao/gadget
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"$board" tests/test_yunbao_button.c "$board/yunbao_button.c" -o "$output/button"
"$output/button"
python3 tools/prepare_sdk.py
gadget=upstream/arcs/gadget
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/audio_stubs -I"$gadget" -I"$board" \
  tests/test_yunbao_screen.c "$board/yunbao_screen_ui.c" -o "$output/screen"
"$output/screen"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/audio_stubs -I"$gadget" \
  tests/test_sound_buffer.c "$gadget/gadget_sound.c" "$gadget/gadget_pcm.c" -lm -o "$output/sound"
"$output/sound"
python3 -m py_compile tools/prepare_sdk.py tools/build.py tools/gen_manifest.py tools/gen_screen_font.py
if [ -n "${ARCS_BASE:-}" ]; then
    sh tests/run_tls.sh
    sh tests/run_voice.sh
fi
