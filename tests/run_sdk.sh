#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 tests/test_product.py
python3 tests/test_arcs_sdk.py
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
python3 tools/prepare_sdk.py --product examples/button_companion/product.json
python3 tools/prepare_sdk.py --product products/arcs-mini2-xiaoling.json
for profile in yunbao mini-companion arcs-mini2-xiaoling; do
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/audio_stubs -Isdk/include \
        -Iupstream/arcs/gadget -Iupstream/arcs/link -Ifirmware/boards/arcs_mini_yunbao/gadget \
        -I"upstream/arcs/products/$profile" tests/test_sdk_api.c sdk/src/impo_arcs_mini.c -o "$output/sdk"
    wake=off
    if [ "$profile" != mini-companion ]; then wake=on; fi
    "$output/sdk" "$profile" "$wake"
done
