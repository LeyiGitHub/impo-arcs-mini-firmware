#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
"${CC:-cc}" -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror \
  -Itests/wifi_stubs -Itests/audio_stubs tests/test_wifi_preference.c -o "$output/wifi"
"$output/wifi"
