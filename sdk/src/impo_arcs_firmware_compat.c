/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include "sdk_version.h"

#if SDK_VERSION_NUMBER == SDK_VERSION(0, 1, 8)
#if !CONFIG_CP_FIRMWARE_COMPAT
#error "The reviewed ListenAI 0.1.8 AP requires the CP PSRAM declaration"
#endif
_Static_assert(CONFIG_CP_FIRMWARE_COMPAT_OFFSET == 0x180,
               "CP PSRAM declaration must match the retained AP contract");
/* The SDK linker pins this read-only declaration after the ARCS header.
 * The AP validates it before starting CP; report the actual linked interval. */
__attribute__((section(".cp_firmware_compat"), used, aligned(4)))
const uint32_t impo_arcs_cp_psram_range[2] = {
    CONFIG_MEM_PSRAM_BASE, CONFIG_MEM_PSRAM_SIZE,
};
#endif
