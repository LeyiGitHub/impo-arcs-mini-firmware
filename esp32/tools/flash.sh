#!/usr/bin/env bash
# Copyright (c) Impo contributors.
# SPDX-License-Identifier: Apache-2.0
#
# Builds the Impo firmware for one board and flashes it:
#
#   tools/flash.sh <board> [PORT]
#
# <board> is the board's `flash` name in hardware/boards.json (the README's
# hardware list shows it), for example cores3, sparkbot or reterminal-e1001.
# Boards with the full UI go through tools/impo/board.sh (build, then flash);
# the others through tools/board.sh. Needs ESP-IDF v6.0.1 (see AGENTS.md).
# Read the board's note in the hardware list first: a few boards need their
# vendor firmware backed up or kept.
set -euo pipefail
board=${1:?usage: tools/flash.sh <board> [PORT]}
port=${2:-}
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
if grep -qE "^[[:space:]]+${board}\)" "$here/impo/board.sh"; then
    "$here/impo/board.sh" build "$board"
    "$here/impo/board.sh" flash "$board" ${port:+"$port"}
else
    "$here/board.sh" "$board" flash ${port:+"$port"}
fi
