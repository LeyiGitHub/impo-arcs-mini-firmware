#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${ARCS_BASE:?Set ARCS_BASE to the ListenAI ARCS SDK 0.1.6 directory}"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
M=$ARCS_BASE/modules/mbedtls/mbedtls
set -- test_tls_reads.c
for source in "$M"/library/*.c; do
    case "$source" in */net_sockets.c) continue ;; esac
    set -- "$@" "$source"
done
"${CC:-cc}" -std=c11 -w -include stdbool.h -I. -DMBEDTLS_CONFIG_FILE=\"host_tls_config.h\" \
  -Iruntime_stubs -I"$M/include" "$@" -lm -o "$output/tls"
"$output/tls"
