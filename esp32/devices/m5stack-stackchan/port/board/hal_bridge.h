/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-FileCopyrightText: Impo contributors
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * M5Stack's hal_bridge (StackChan firmware/main/hal/board/hal_bridge.h) cut
 * down to the one call vendor/m5stack makes: the CoreS3's main I2C bus, which
 * Impo's CoreS3 board (devices/m5stack-cores3) hands to the body at start-up.
 */
#pragma once
#include <driver/i2c_master.h>

namespace hal_bridge {

i2c_master_bus_handle_t board_get_i2c_bus();

}  // namespace hal_bridge
