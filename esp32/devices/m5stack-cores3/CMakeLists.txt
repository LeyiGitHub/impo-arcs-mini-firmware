# SPDX-License-Identifier: Apache-2.0
# M5Stack CoreS3 (device.json): the board under Impo's UI, its camera and its
# motion sensor. A body it sits in (a device with "base": "m5stack-cores3")
# adds its own component and selects M5STACK_CORES3_BODY.
# vendor/m5stack is M5Stack's code, unchanged (vendor/README.md).
set(m5 "vendor/m5stack/hal")
idf_component_register(
    SRCS "board.c" "camera.c" "imu.cpp"
         "${m5}/drivers/bmi270/bmi270.cpp"
         "${m5}/drivers/bmi270/BMI270_SensorAPI/bmi2.c"
         "${m5}/drivers/bmi270/BMI270_SensorAPI/bmi270.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "${m5}"
    REQUIRES impo esp_driver_i2c
    PRIV_REQUIRES camera esp_timer
    WHOLE_ARCHIVE   # impo_board_get() and the camera's registration
)

# M5Stack's code is built as M5Stack builds it, not under this project's
# warnings-as-errors.
file(GLOB_RECURSE vendored CONFIGURE_DEPENDS "${COMPONENT_DIR}/vendor/*.c" "${COMPONENT_DIR}/vendor/*.cc" "${COMPONENT_DIR}/vendor/*.cpp")
set_source_files_properties(${vendored} PROPERTIES COMPILE_OPTIONS "-Wno-error")
