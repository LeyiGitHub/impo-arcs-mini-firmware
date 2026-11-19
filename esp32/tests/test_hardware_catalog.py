# Copyright (c) Impo contributors.
# SPDX-License-Identifier: Apache-2.0
"""hardware/boards.json is the hardware list in the README and on impo.ai:
every ESP32 board in it must flash through tools/flash.sh, have its image,
and the README gallery must match it."""
from pathlib import Path
import json
import re
import subprocess
import sys
import unittest

ESP32 = Path(__file__).resolve().parent.parent
ROOT = ESP32.parent
CATALOG = json.loads((ROOT / "hardware/boards.json").read_text())


def helper_names(path, pattern):
    return set(re.findall(pattern, (ESP32 / path).read_text(), re.M))


class HardwareCatalogTest(unittest.TestCase):
    def test_every_esp32_board_has_a_helper(self):
        impo = helper_names("tools/impo/board.sh", r"^\s+([\w-]+)\)\s+profile=")
        link = set()
        for labels in helper_names("tools/board.sh", r"^  ([\w|-]+)\)$"):
            link.update(labels.split("|"))
        for board in CATALOG["boards"]:
            if board["sdk"] != "esp32":
                continue
            with self.subTest(board=board["id"]):
                self.assertIn(board["flash"], impo | link)

    def test_ids_categories_and_status(self):
        ids = [b["id"] for b in CATALOG["boards"]]
        self.assertEqual(len(ids), len(set(ids)))
        categories = {c["id"] for c in CATALOG["categories"]}
        for board in CATALOG["boards"]:
            with self.subTest(board=board["id"]):
                self.assertIn(board["category"], categories)
                self.assertIn(board["status"], {"ready", "experimental"})
                self.assertIn(board["sdk"], {"esp32", "arcs", "linux"})
                self.assertTrue(board["highlights"])

    def test_readme_gallery_is_current(self):
        result = subprocess.run([sys.executable, str(ROOT / "hardware/render.py"), "--check"],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr or result.stdout)


if __name__ == "__main__":
    unittest.main()
