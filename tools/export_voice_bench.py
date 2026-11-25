#!/usr/bin/env python3
"""Export reviewable measurements without copying raw logs, audio or identities."""
import argparse
import json
from pathlib import Path


def export(directory):
    directory = Path(directory)
    analysis = json.loads((directory / "analysis.json").read_text())
    manifest = json.loads((directory / "manifest.json").read_text())
    trials = []
    for row in analysis["trials"]:
        acoustic = row.get("acoustic", {})
        trial = {k: row[k] for k in
                 ("index", "cycle", "mode", "endpoint_ms", "ok", "voice_ok",
                  "failed", "pcm", "queued", "backlog", "dropped",
                  "chat_requests", "tts_requests", "audio_timing_generation_matches")
                 if k in row}
        trial["bench_error"] = bool(row.get("error") or row.get("record_error"))
        trial["reply_diagnostics"] = {k: row.get("reply_diagnostics", {})[k] for k in
                                     ("ready", "bytes", "lines", "complete", "missing_parent",
                                      "not_ready", "duplicate", "rejected_parent", "rejected_state")
                                     if k in row.get("reply_diagnostics", {})}
        trial["acoustic"] = {k: acoustic[k] for k in
                             ("acoustic_valid", "acoustic_gap_ms", "stimulus_tail_seen",
                              "reference_correlation", "answer_peak_to_noise")
                             if k in acoustic}
        trials.append(trial)
    summaries = {}
    for mode, values in analysis["summary"].items():
        summaries[mode] = {k: values[k] for k in
                           ("attempted", "voice_failed", "bench_errors", "acoustic_valid",
                            "acoustic_rejected", "p50_ms", "p95_ms") if k in values}
        summaries[mode]["device_components_p50_ms"] = {
            k: values.get("device_components_p50_ms", {})[k] for k in
            ("last_voice_to_capture_ms", "capture_to_queued_ms", "queued_to_ack_ms",
             "ack_to_text_ms", "tts_to_first_pcm_ms", "first_pcm_to_driver_write_ms")
            if k in values.get("device_components_p50_ms", {})}
    return {
        "schema_version": 1,
        "metric": "fixed source last active frame to detected answer energy, automated acoustic proxy",
        "stimulus_sha256": manifest["stimulus_sha256"],
        "conditions": {k: manifest[k] for k in
                       ("modes", "cycles", "afplay_volume", "acoustic_capture",
                        "speaker_volume_percent", "mac_output_volume_percent") if k in manifest},
        "installed_image": {k: manifest.get("installed_image", {})[k] for k in
                            ("image_sha256", "image_bytes", "product", "listenai_sdk_version", "wake_bundle")
                            if k in manifest.get("installed_image", {})},
        "summary": summaries,
        "paired": {k: analysis.get("paired", {})[k] for k in
                   ("pairs", "median_overlap_minus_batch_ms", "paired_median_bootstrap95_ms")
                   if k in analysis.get("paired", {})},
        "trials": trials,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(export(args.directory), ensure_ascii=False, indent=2) + "\n")
