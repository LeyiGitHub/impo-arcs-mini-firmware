#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
p=upstream/arcs/platform/arcs
"${CC:-cc}" -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror \
  -include tests/cloud_stubs/critical.h -Itests/cloud_stubs -I"$p/esp_compat/include" \
  -I"$p" -I"$p/esp_compat" -Itests/audio_stubs \
  tests/test_cloud_transport.c "$p/http_client.c" "$p/esp_compat/esp_http_client.c" \
  "$p/esp_compat/esp_compat_url.c" "$p/esp_compat/esp_tls.c" -o "$output/cloud"
"$output/cloud"
python3 upstream/esp32/tests/test_cloud_route.py
