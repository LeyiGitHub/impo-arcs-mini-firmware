#!/usr/bin/env python3
"""Reject incomplete ARCS link products before a CP image is considered usable."""
import argparse, struct
from pathlib import Path

REQUIRED = (
    "__lisa_device_registry_gpioa", "__lisa_device_registry_gpiob",
    "__lisa_device_registry_audio0", "__lisa_device_registry_pwm0",
    "__lisa_device_registry_spi0", "__lisa_device_registry_display",
    "__lisa_device_registry_st7789p3", "yunbao_wake_start",
    "gadget_voice_wake", "gadget_voice_continuous_active", "preferred_boot_task",
    "impo_cloud_route_endpoint", "impo_cloud_route_url", "impo_cloud_route_tls_name",
    "impo_cloud_route_report", "impo_cloud_route_reset", "plat_tls_connect_named",
    "impo_arcs_mini_product", "impo_arcs_mini_application",
)

def incomplete_archives(build):
    bad = []
    for archive in Path(build).rglob("*.a"):
        with archive.open("rb") as f:
            magic = f.read(8)
        if magic not in (b"!<arch>\n", b"!<thin>\n"):
            bad.append(archive)
    return bad

def check_psram_contract(image, base, size):
    if len(image) < 0x188 or struct.unpack_from("<II", image, 0x180) != (base, size):
        raise SystemExit("CP PSRAM declaration is missing or differs from the linked memory range; retained AP will refuse startup")

def check(build, wake_enabled=True, arcs_sdk_version=None):
    build = Path(build)
    bad = incomplete_archives(build)
    if bad:
        raise SystemExit("Incomplete generated archive: " + str(bad[0]))
    symbols = (build / "impo-gadget.symb").read_text()
    required = REQUIRED + (("impo_arcs_wake_resources", "impo_arcs_wake_result") if wake_enabled else ())
    if arcs_sdk_version == "0.1.8":
        required += ("ipc_auto_init", "ipc_mem_validate", "impo_arcs_boot_watchdog_handoff")
    elif arcs_sdk_version == "0.1.6":
        required += ("plat_ipc_early_init",)
    missing = [name for name in required if name + "|" not in symbols.replace(" ", "")]
    if missing:
        raise SystemExit("Required hardware/feature symbols missing: " + ", ".join(missing))
    image = (build / "impo-gadget.bin").read_bytes()
    size = len(image)
    if not 512 < size <= 0x400000:
        raise SystemExit("Image does not fit the CP partition")
    if image[336:340] != b"ARCS" or struct.unpack_from("<I", image, 324)[0] != size:
        raise SystemExit("Missing or inconsistent ARCS boot header")
    if arcs_sdk_version == "0.1.8":
        config = dict(line.split("=", 1) for line in (build / ".config").read_text().splitlines()
                      if line.startswith("CONFIG_"))
        check_psram_contract(image, int(config["CONFIG_MEM_PSRAM_BASE"], 0),
                             int(config["CONFIG_MEM_PSRAM_SIZE"], 0))
    print("Image check: archives, device registry, SDK hooks, wake/dialogue and CP size passed")

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("build", type=Path)
    ap.add_argument("--no-wake", action="store_true")
    ap.add_argument("--arcs-sdk-version", choices=("0.1.6", "0.1.8"))
    a = ap.parse_args()
    check(a.build, wake_enabled=not a.no_wake, arcs_sdk_version=a.arcs_sdk_version)
