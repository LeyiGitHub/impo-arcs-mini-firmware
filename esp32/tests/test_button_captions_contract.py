# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Captions name a board's buttons; a board without an aux button must not
read "press the (null) button"."""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMPO = ROOT / "components" / "impo"
BOARD_DEFINITION = re.compile(r"\bimpo_board_t\s+\w+\s*=\s*\{")


def _function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for i in range(brace, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1 : i]
    raise AssertionError(f"could not find end of {signature}")


def _board_sources() -> list[Path]:
    candidates = [*(IMPO / "boards").glob("*.c"), *(ROOT / "devices").glob("*/*.c")]
    return [p for p in candidates if BOARD_DEFINITION.search(p.read_text())]


class ButtonCaptionsContractTest(unittest.TestCase):
    def test_every_board_names_its_talk_button(self) -> None:
        boards = _board_sources()
        self.assertGreater(len(boards), 10)
        for path in boards:
            with self.subTest(board=path.relative_to(ROOT).as_posix()):
                self.assertIn(".talk_button", path.read_text())

    def test_some_boards_have_no_aux_button(self) -> None:
        # The case the captions below guard against is real.
        self.assertTrue(any(".aux_button" not in p.read_text() for p in _board_sources()))

    def test_settings_power_page_names_aux_button_only_when_there_is_one(self) -> None:
        page = _function_body((IMPO / "impo_settings_ui.c").read_text(),
                              "static void build_power_page(")
        guarded = _function_body(page, "if (impo_board->aux_button)")
        self.assertIn("impo_board->aux_button", guarded)
        unguarded = page.replace(guarded, "")
        self.assertNotIn("impo_board->aux_button);", unguarded)
        self.assertIn('"Press the %s button to turn it back on."', unguarded)

    def test_menu_power_view_falls_back_to_the_talk_button(self) -> None:
        menu = (IMPO / "impo_menu.c").read_text()
        self.assertIn(
            'impo_board->aux_button ? impo_board->aux_button : impo_board->talk_button', menu)


if __name__ == "__main__":
    unittest.main()
