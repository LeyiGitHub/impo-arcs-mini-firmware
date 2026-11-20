# Copyright (c) Impo contributors.
# SPDX-License-Identifier: Apache-2.0
"""Device folders (devices/README.md): each devices/<id>/device.json names its
folder, a chip target and a settings profile that exist, and a base that is
another device folder; each folder builds as an IDF component; code copied
from elsewhere (vendor/) carries its licence."""
from pathlib import Path
import json
import unittest

ESP32 = Path(__file__).resolve().parent.parent
DEVICES = ESP32 / "devices"
FOLDERS = sorted(p.parent for p in DEVICES.glob("*/device.json"))
TARGETS = {"esp32", "esp32s3", "esp32c3", "esp32c6"}
REQUIRED = ("id", "name", "vendor", "category", "chip", "status", "sdk", "highlights", "flash")


def load(folder):
    return json.loads((folder / "device.json").read_text())


def chain(folder):
    """The folder and its bases, root first."""
    seen = []
    while folder is not None:
        if folder in seen:
            raise AssertionError(f"base loop at {folder.name}")
        seen.insert(0, folder)
        base = load(folder).get("base")
        folder = DEVICES / base if base else None
    return seen


class DeviceFoldersTest(unittest.TestCase):
    def test_there_are_device_folders(self):
        self.assertIn(DEVICES / "m5stack-cores3", FOLDERS)
        self.assertIn(DEVICES / "m5stack-stackchan", FOLDERS)

    def test_device_json(self):
        for folder in FOLDERS:
            with self.subTest(device=folder.name):
                device = load(folder)
                self.assertEqual(device["id"], folder.name)
                for key in REQUIRED:
                    self.assertIn(key, device)
                if "base" in device:
                    self.assertTrue((DEVICES / device["base"] / "device.json").is_file(), device["base"])
                    self.assertNotIn("target", device, "the chain's root names the target")
                    self.assertNotIn("profile", device, "the chain's root names the profile")
                root = load(chain(folder)[0])
                self.assertIn(root["target"], TARGETS)
                self.assertTrue((DEVICES / f"sdkconfig.{root['profile']}").is_file(), root["profile"])

    def test_each_folder_is_a_component_with_settings(self):
        for folder in FOLDERS:
            with self.subTest(device=folder.name):
                self.assertIn("idf_component_register", (folder / "CMakeLists.txt").read_text())
                self.assertTrue((folder / "sdkconfig").is_file())
                self.assertTrue((folder / "README.md").is_file())

    def test_only_a_chain_root_picks_the_board(self):
        for folder in FOLDERS:
            with self.subTest(device=folder.name):
                says = "CONFIG_IMPO_BOARD_DEVICE=y" in (folder / "sdkconfig").read_text()
                self.assertEqual(says, "base" not in load(folder))

    def test_vendored_code_carries_its_licence(self):
        for folder in FOLDERS:
            vendor = folder / "vendor"
            if not vendor.is_dir():
                continue
            for source in sorted(p for p in vendor.iterdir() if p.is_dir()):
                with self.subTest(source=f"{folder.name}/vendor/{source.name}"):
                    self.assertTrue((source / "LICENSE").is_file())
            with self.subTest(readme=folder.name):
                self.assertTrue((vendor / "README.md").is_file())


if __name__ == "__main__":
    unittest.main()
