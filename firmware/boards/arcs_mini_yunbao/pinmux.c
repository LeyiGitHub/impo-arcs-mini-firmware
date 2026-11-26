/* SPDX-License-Identifier: MIT */
#include "pinmux.h"
#include "IOMuxManager.h"
void lisa_gpioa_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAMERA_RST_PIN, 1);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, PA_EN_PIN, 1);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_CS_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_CD_PIN, 0);
}
void lisa_gpiob_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, USB_DET_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LED_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, POWER_EN_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, POWER_KEY_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_RST_PIN, 0);
}
void lisa_uart0_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 2, 2);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 3, 2);
}

/* The SDK may initialize adc0; this USB target routes no battery ADC. */
void lisa_adc_pinmux(void) {}
void lisa_pwm_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_PWM_PIN, 12);
}
void lisa_spi0_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_SPI_DATA_PIN, 5);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_SPI_CLK_PIN, 5);
}
void lisa_i2c0_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 6, 8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 7, 8);
}
void lisa_dvp_pinmux(void)
{
    const unsigned pins[] = { CAM_HSYNC_PIN, CAM_VSYNC_PIN, CAM_PCLK_PIN,
        CAM_D0_PIN, CAM_D1_PIN, CAM_D2_PIN, CAM_D3_PIN, CAM_D4_PIN,
        CAM_D5_PIN, CAM_D6_PIN, CAM_D7_PIN, CAM_MCLK_PIN };
    for (unsigned i=0; i<sizeof(pins)/sizeof(pins[0]); ++i)
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, pins[i], 16);
}
