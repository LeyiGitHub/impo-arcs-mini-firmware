/* SPDX-License-Identifier: MIT
 * Yunbao voice box. No actuators are initialized or advertised.
 * PB04 short click starts/stops an SDK voice turn; hold retains SDK power-off.
 */
#include <stddef.h>
#include "lisa_gpio.h"
#include "pinmux.h"
#include "power_manager.h"
#include "platform_arcs.h"
#include "gadget_board.h"
#include "yunbao_button.h"

static lisa_device_t *s_amp, *s_gpio;
static yunbao_button_t s_button;
static void set_amp(bool on)
{
    if (s_amp) lisa_gpio_write_pin(s_amp, PA_EN_PIN, on ? LISA_GPIO_HIGH : LISA_GPIO_LOW);
}
static int init(void)
{
    s_amp = lisa_device_get("gpioa");
    s_gpio = lisa_device_get("gpiob");
    if (!lisa_device_ready(s_amp) || !lisa_device_ready(s_gpio)) return -1;
    lisa_gpio_configure(s_amp, PA_EN_PIN, LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_LOW);
    lisa_gpio_configure(s_gpio, POWER_KEY_PIN, LISA_GPIO_CONFIG_INPUT_PULLUP);
    return 0;
}
static unsigned poll_buttons(void)
{
    return yunbao_button_poll(&s_button,
        lisa_gpio_read_pin(s_gpio, POWER_KEY_PIN) == LISA_GPIO_LOW,
        (uint32_t)plat_uptime_ms());
}
static int read_power(gadget_power_t *out)
{
    *out = (gadget_power_t){ .usb_power = power_is_usb_plugged() };
    return 0; /* No unverified battery curve is reported. */
}
static int power_off(void) { power_shutdown(); return -1; }
static const char s_instructions[] =
    "This gadget is Yunbao, a stationary ARCS Mini voice companion. "
    "A short click of the power button starts or stops a voice turn. "
    "Replies are spoken by its own speaker. Reply in the user's language, "
    "in one to three short sentences without Markdown. "
    "It has no advertised camera, screen, robot movement or motion sensor.";
static const gadget_board_t s_board = {
    .name = "云宝",
    .model_id = "arcs-mini-yunbao",
    .ble_prefix = "ImpoGadget",
    .product_id = NULL,
    .init = init,
    .audio = { .speaker = true, .microphone = true, .mic_channel = 0,
               .mic_gain_db = 30, .set_amp = set_amp },
    .poll_buttons = poll_buttons,
    .read_power = read_power,
    .power_off = power_off,
    .instructions = s_instructions,
};
const gadget_board_t *gadget_board_get(void) { return &s_board; }
