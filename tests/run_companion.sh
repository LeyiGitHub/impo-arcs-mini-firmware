#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${ARCS_BASE:?Set ARCS_BASE for the real cJSON implementation}"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
J=$ARCS_BASE/modules/cjson
B=firmware/boards/arcs_mini_yunbao/gadget
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Isdk/include tests/test_receipt.c sdk/src/impo_arcs_receipt.c -o "$output/receipt"
"$output/receipt"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"$J" -Isdk/include -Itests/audio_stubs -Iupstream/arcs/gadget -Iupstream/arcs/link -I"$B" -Iupstream/arcs/products/yunbao \
 tests/test_companion.c sdk/src/impo_arcs_companion.c sdk/src/impo_arcs_receipt.c "$B/yunbao_screen_ui.c" "$J/cJSON.c" -lm -o "$output/companion"
"$output/companion"
