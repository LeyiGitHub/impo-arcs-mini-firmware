#!/bin/sh
set -eu
cd "$(dirname "$0")"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
B=../../boards/arcs_mini_voice/gadget
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"$B" \
  test_arcs_mini_button.c "$B/arcs_mini_button.c" -o "$output/button"
"$output/button"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Iruntime_stubs -I../../gadget -I"$B" \
  test_arcs_mini_screen.c "$B/arcs_mini_screen_ui.c" -o "$output/screen"
"$output/screen" "$@"
