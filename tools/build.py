#!/usr/bin/env python3
"""Build the board with the official SDK, optionally in the pinned builder."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
from prepare_sdk import ROOT, prepare
from check_image import check, incomplete_archives
from product import DEFAULT_PRODUCT, load_product
from arcs_sdk import SUPPORTED, validate, build_directory
ap = argparse.ArgumentParser()
ap.add_argument("--docker", action="store_true")
ap.add_argument("--arcs-sdk-version", choices=SUPPORTED, default="0.1.6",
                help="ListenAI SDK profile matching the retained AP (default: 0.1.6)")
ap.add_argument("--sdk", type=Path, default=ROOT / "upstream")
ap.add_argument("--product", type=Path, default=DEFAULT_PRODUCT)
ap.add_argument("--font", type=Path)
ap.add_argument("--app-dir", type=Path, help="Application component with CMakeLists.txt and impo_arcs_mini_application()")
a = ap.parse_args()
profile = load_product(a.product)
app = a.app_dir.resolve() if a.app_dir else None
if app and not (app / "CMakeLists.txt").is_file():
    raise SystemExit("--app-dir requires a component CMakeLists.txt")
base = Path(os.environ.get("ARCS_BASE", ""))
try:
    validate(base if os.environ.get("ARCS_BASE") else "", a.arcs_sdk_version)
except ValueError as error:
    raise SystemExit(str(error))
sdk = prepare(a.sdk, a.product, a.font)
build_dir = build_directory(profile["id"], a.arcs_sdk_version)
product_dir = sdk / "arcs/products" / profile["id"]
sdk_conf = "components/impo_arcs_mini/profiles/listenai-" + a.arcs_sdk_version + ".conf"
args = ["bash", "build.sh", "-S", "app", "-B", build_dir, "-t", "mkhdr", "-DBOARD=arcs_mini_yunbao",
        "-DCONFIG_FILES=" + str(product_dir / "product.conf") + ";" + str(sdk / "arcs" / sdk_conf),
        "-DIMPO_PRODUCT_DIR=" + str(product_dir), "-DIMPO_APP_DIR=" + (str(app) if app else "")]
if a.docker:
    image = json.loads((ROOT / "dependencies.lock.json").read_text())["builder_image"]
    # Container paths must not accidentally point at host-only product/app paths.
    args[-3:] = ["-DCONFIG_FILES=/sdk/arcs/products/" + profile["id"] + "/product.conf;/sdk/arcs/" + sdk_conf,
                 "-DIMPO_PRODUCT_DIR=/sdk/arcs/products/" + profile["id"], "-DIMPO_APP_DIR=" + ("/impo-app" if app else "")]
    mounts = ["-v", str(app) + ":/impo-app:ro"] if app else []
    args = ["docker", "run", "--rm", "--platform", "linux/amd64", "-v", str(sdk) + ":/sdk", "-v", str(base.resolve()) + ":/arcs-sdk:ro", *mounts, "-e", "ARCS_BASE=/arcs-sdk", "-w", "/sdk/arcs", image, *args]
def discard_incomplete():
    bad = incomplete_archives(sdk / "arcs" / build_dir)
    for archive in bad:
        print("Rebuilding incomplete generated archive:", archive.relative_to(sdk / "arcs" / build_dir), flush=True)
        archive.unlink()  # Generated outputs only, never SDK source or prebuilt vendor libraries.
    return bool(bad)

discard_incomplete()  # A previous interrupted build must not reuse a zero-byte archive.
for attempt in range(2):
    result = subprocess.run(args, cwd=None if a.docker else sdk / "arcs")
    bad = incomplete_archives(sdk / "arcs" / build_dir)
    if bad and attempt == 0:
        discard_incomplete()
        continue
    result.check_returncode()
    break
check(sdk / "arcs" / build_dir, wake_enabled=profile["wake"]["enabled"], arcs_sdk_version=a.arcs_sdk_version)
firmware = sdk / "arcs" / build_dir / "impo-gadget.bin"
metadata = {"product": profile["id"], "voice": profile["voice"], "listenai_sdk_version": a.arcs_sdk_version,
            "image_bytes": firmware.stat().st_size, "image_sha256": hashlib.sha256(firmware.read_bytes()).hexdigest(),
            "wake_bundle": profile["wake"]["id"], "retained_ap_required": True}
(firmware.parent / "impo-sdk-build.json").write_text(json.dumps(metadata, indent=2) + "\n")
print("Firmware:", sdk / "arcs" / build_dir / "impo-gadget.bin")
