#!/usr/bin/env python3
"""Build the board with the official SDK, optionally in the pinned builder."""
import argparse, json, os, subprocess
from pathlib import Path
from prepare_sdk import ROOT, prepare
from check_image import check
ap = argparse.ArgumentParser()
ap.add_argument("--docker", action="store_true")
ap.add_argument("--sdk", type=Path, default=ROOT / "upstream")
a = ap.parse_args()
base = Path(os.environ.get("ARCS_BASE", ""))
if not os.environ.get("ARCS_BASE") or not (base / "VERSION").is_file():
    raise SystemExit("Set ARCS_BASE to your ListenAI ARCS SDK 0.1.6 directory")
values = dict(line.replace(" ", "").split("=", 1) for line in (base / "VERSION").read_text().splitlines() if "=" in line)
if [values.get(k) for k in ("VERSION_MAJOR", "VERSION_MINOR", "PATCHLEVEL")] != ["0", "1", "6"]:
    raise SystemExit("This firmware targets ARCS SDK 0.1.6; do not silently use 0.1.8")
sdk = prepare(a.sdk)
args = ["bash", "build.sh", "-S", "app", "-B", "build-yunbao", "-t", "mkhdr", "-DBOARD=arcs_mini_yunbao", "-DCONFIG_FILES=yunbao.conf"]
if a.docker:
    image = json.loads((ROOT / "dependencies.lock.json").read_text())["builder_image"]
    args = ["docker", "run", "--rm", "--platform", "linux/amd64", "-v", str(sdk) + ":/sdk", "-v", str(base.resolve()) + ":/arcs-sdk:ro", "-e", "ARCS_BASE=/arcs-sdk", "-w", "/sdk/arcs", image, *args]
    subprocess.run(args, check=True)
else:
    subprocess.run(args, cwd=sdk / "arcs", check=True)
check(sdk / "arcs/build-yunbao")
print("Firmware:", sdk / "arcs/build-yunbao/impo-gadget.bin")
