"""Publishing keeps failed attempts, but removes private and unknown fields."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from export_voice_bench import export


class ExportTests(unittest.TestCase):
    def test_private_fields_removed_and_failed_attempt_kept(self):
        secret = "PRIVATE-DO-NOT-PUBLISH"
        analysis = {"metric": secret,
                    "summary": {"overlap:1200": {"attempted": 1, "voice_failed": 1,
                        "private": secret, "device_components_p50_ms": {"raw_log": secret}}},
                    "paired": {"private": secret}, "trials": [
                        {"index": 1, "mode": "overlap", "ok": False, "failed": 1,
                         "tail_keep_ms": 300, "captured_pcm_bytes": 76800, "trimmed_pcm_bytes": 28800,
                         "error": secret, "boot": secret, "raw_log": secret,
                         "reply_diagnostics": {"bytes": 77, "raw_log": secret},
                         "acoustic": {"acoustic_valid": False, "error": secret}}]}
        manifest = {"stimulus_sha256": "0" * 64, "device_id": secret,
                    "installed_image": {"image_sha256": "1" * 64, "token": secret}}
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            (directory / "analysis.json").write_text(json.dumps(analysis))
            (directory / "manifest.json").write_text(json.dumps(manifest))
            result = export(directory)
        self.assertNotIn(secret, json.dumps(result))
        self.assertEqual(len(result["trials"]), 1)
        self.assertFalse(result["trials"][0]["ok"])
        self.assertTrue(result["trials"][0]["bench_error"])
        self.assertEqual(result["summary"]["overlap:1200"]["voice_failed"], 1)
        self.assertEqual(result["trials"][0]["reply_diagnostics"], {"bytes": 77})
        self.assertEqual(result["trials"][0]["tail_keep_ms"],300)
        self.assertEqual(result["trials"][0]["trimmed_pcm_bytes"],28800)


if __name__ == "__main__":
    unittest.main()
