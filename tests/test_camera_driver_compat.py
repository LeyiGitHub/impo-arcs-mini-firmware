#!/usr/bin/env python3
"""Compile the SDK DVP bus, checking the frame owner in its real stop paths."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = Path(os.environ["ARCS_BASE"]).resolve()
BUS = SDK / "drivers/lisa_camera/bus/lisa_camera_bus_dvp.c"
COMPAT = ROOT / "firmware/boards/arcs_mini_yunbao/gadget/camera_driver.cmake"
ORIGINAL = BUS.read_bytes()


def run(*args):
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL)


with tempfile.TemporaryDirectory(prefix="yunbao-camera-compat-") as temp:
    work = Path(temp)
    (work / "CMakeLists.txt").write_text(f"""cmake_minimum_required(VERSION 3.20)
project(camera_owner_test C)
set(CONFIG_LISA_CAMERA_DEVICE ON)
set(CONFIG_LISA_CAMERA_BUS_DVP ON)
add_library(lisa_camera_arcs STATIC \"{BUS}\")
include(\"{COMPAT}\")
function(write_selection)
  get_target_property(selected lisa_camera_arcs SOURCES)
  file(WRITE \"${{CMAKE_BINARY_DIR}}/selected.txt\" \"${{selected}}\")
endfunction()
cmake_language(DEFER CALL write_selection)
""")
    build = work / "build"
    run("cmake", "-S", str(work), "-B", str(build))
    patched = build / "yunbao_camera_compat/lisa_camera_bus_dvp.c"
    assert (build / "selected.txt").read_text() == str(patched)
    assert BUS.read_bytes() == ORIGINAL
    for mode, define in (("normal", []), ("pingpong", ["-DDVP_USE_PINGPONG_DMA"])):
        output = work / mode
        run(os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
            *define, f'-DCAMERA_BUS_SOURCE="{patched}"',
            f"-I{ROOT / 'tests/camera_stubs'}", f"-I{BUS.parent}",
            str(ROOT / "tests/test_camera_dvp_owner.c"), "-o", str(output))
        run(str(output))
    # SDK 0.1.8 must retain its original target source unchanged.
    (work / "CMakeLists.txt").write_text(f"""cmake_minimum_required(VERSION 3.20)
project(camera_owner_test C)
set(CONFIG_LISA_CAMERA_DEVICE ON)
set(CONFIG_LISA_CAMERA_BUS_DVP ON)
add_library(lisa_camera_drv STATIC \"{BUS}\")
include(\"{COMPAT}\")
function(write_selection)
  get_target_property(selected lisa_camera_drv SOURCES)
  file(WRITE \"${{CMAKE_BINARY_DIR}}/selected.txt\" \"${{selected}}\")
endfunction()
cmake_language(DEFER CALL write_selection)
""")
    run("cmake", "-S", str(work), "-B", str(work / "sdk18"))
    assert (work / "sdk18/selected.txt").read_text() == str(BUS)
    # The real gadget can be configured before the SDK adds its camera target.
    (work / "CMakeLists.txt").write_text(f"""cmake_minimum_required(VERSION 3.20)
project(camera_owner_test C)
set(CONFIG_LISA_CAMERA_DEVICE ON)
set(CONFIG_LISA_CAMERA_BUS_DVP ON)
include(\"{COMPAT}\")
add_library(lisa_camera_arcs STATIC \"{BUS}\")
function(write_selection)
  get_target_property(selected lisa_camera_arcs SOURCES)
  file(WRITE \"${{CMAKE_BINARY_DIR}}/selected.txt\" \"${{selected}}\")
endfunction()
cmake_language(DEFER CALL write_selection)
""")
    run("cmake", "-S", str(work), "-B", str(work / "late-target"))
    assert (work / "late-target/selected.txt").read_text() == str(
        work / "late-target/yunbao_camera_compat/lisa_camera_bus_dvp.c")
print("camera DVP frame-owner regression passed (normal, Ping/Pong, SDK 0.1.8 passthrough)")
