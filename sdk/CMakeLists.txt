# SPDX-License-Identifier: MIT
if(NOT IMPO_PRODUCT_DIR OR NOT EXISTS "${IMPO_PRODUCT_DIR}/impo_product_config.h")
    message(FATAL_ERROR "Generate a product with tools/build.py --product before building")
endif()
listenai_library_named(impo_arcs_mini)
listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${IMPO_PRODUCT_DIR})
listenai_library_sources(src/impo_arcs_mini.c src/impo_arcs_wake_resources.c
                        src/impo_arcs_firmware_compat.c src/impo_arcs_boot_handoff.c
                        src/impo_arcs_voice_shell.c)
# 0.1.8's recovery API links boot_flash.c into applications. Its flash-critical
# section must join the SDK's startup-copied SRAM text, not execute from XIP.
# The section is absent in 0.1.6; the same relocation is then empty.
listenai_code_relocate(SECTIONS .boot_f.ramcode LOCATION SRAM_TEXT)
if(IMPO_APP_DIR)
    add_subdirectory(${IMPO_APP_DIR} ${CMAKE_BINARY_DIR}/impo_application)
else()
    listenai_library_sources(src/default_app.c)
endif()
