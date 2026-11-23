#!/usr/bin/env python3
"""Reject incomplete ARCS link products before a CP image is considered usable."""
import argparse
from pathlib import Path

REQUIRED = (
    "__lisa_device_registry_gpioa", "__lisa_device_registry_gpiob",
    "__lisa_device_registry_audio0", "__lisa_device_registry_pwm0",
    "__lisa_device_registry_spi0", "__lisa_device_registry_display",
    "__lisa_device_registry_st7789p3", "yunbao_wake_start",
    "gadget_voice_wake", "gadget_voice_continuous_active", "preferred_boot_task",
)

def check(build):
    build = Path(build)
    for archive in build.rglob("*.a"):
        with archive.open("rb") as f:
            magic = f.read(8)
        if magic not in (b"!<arch>\n", b"!<thin>\n"):
            raise SystemExit("Incomplete generated archive: " + str(archive))
    symbols = (build / "impo-gadget.symb").read_text()
    missing = [name for name in REQUIRED if name + "|" not in symbols.replace(" ", "")]
    if missing:
        raise SystemExit("Required hardware/feature symbols missing: " + ", ".join(missing))
    size = (build / "impo-gadget.bin").stat().st_size
    if not 512 < size <= 0x400000:
        raise SystemExit("Image does not fit the CP partition")
    print("Image check: archives, required device registry, wake/dialogue and CP size passed")

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("build", type=Path)
    check(ap.parse_args().build)
