import sys
import tempfile
import unittest
import struct
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from arcs_sdk import validate, build_directory
from check_image import check_psram_contract

class SDKProfileTests(unittest.TestCase):
    def test_explicit_profile_rejects_the_other_runtime(self):
        with tempfile.TemporaryDirectory() as name:
            base = Path(name)
            for patch in (6, 8):
                version = f"0.1.{patch}"
                (base / "VERSION").write_text(f"VERSION_MAJOR = 0\nVERSION_MINOR = 1\nPATCHLEVEL = {patch}\n")
                self.assertEqual(validate(base, version), version)
                other = "0.1.8" if patch == 6 else "0.1.6"
                with self.assertRaisesRegex(ValueError, "retained AP"):
                    validate(base, other)
            with self.assertRaises(ValueError):
                validate(base, "0.1.7")
    def test_build_outputs_cannot_reuse_the_other_runtime(self):
        self.assertNotEqual(build_directory("same-product", "0.1.6"), build_directory("same-product", "0.1.8"))
        self.assertEqual(build_directory("yunbao", "0.1.6"), "build-yunbao")
    def test_missing_sdk_is_rejected(self):
        with tempfile.TemporaryDirectory() as base:
            with self.assertRaisesRegex(ValueError, "ARCS_BASE"):
                validate(base, "0.1.8")
    def test_ap_contract_rejects_missing_and_wrong_memory_interval(self):
        image = bytearray(512)
        with self.assertRaisesRegex(SystemExit, "retained AP"):
            check_psram_contract(image, 0x28800000, 0x800000)
        struct.pack_into("<II", image, 0x180, 0x28800000, 0x800000)
        check_psram_contract(image, 0x28800000, 0x800000)
        with self.assertRaisesRegex(SystemExit, "retained AP"):
            check_psram_contract(image, 0x28300000, 0x500000)

if __name__ == "__main__":
    unittest.main()
