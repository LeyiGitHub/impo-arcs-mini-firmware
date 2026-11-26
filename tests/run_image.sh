#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
 -Isdk/include -Itests/audio_stubs -Iupstream/arcs/platform/arcs \
 tests/test_image.c sdk/src/impo_arcs_image.c sdk/src/impo_arcs_image_decode.c \
 -o "$output/image"
"$output/image" tests/fixtures/image-alpha.png tests/fixtures/image-fit.jpg \
 tests/fixtures/image-512.png tests/fixtures/image-513.png
