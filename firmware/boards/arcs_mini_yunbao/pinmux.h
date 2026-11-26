/* SPDX-License-Identifier: MIT
 * Pin aliases match ListenAI arcs_mini's board, not the V2.1 robot dog.
 */
#pragma once
#define CAMERA_RST_PIN 0
#define CAM_HSYNC_PIN 10
#define CAM_VSYNC_PIN 11
#define CAM_PCLK_PIN 12
#define CAM_D0_PIN 13
#define CAM_D1_PIN 14
#define CAM_D2_PIN 15
#define CAM_D3_PIN 16
#define CAM_D4_PIN 17
#define CAM_D5_PIN 18
#define CAM_D6_PIN 19
#define CAM_D7_PIN 20
#define CAM_MCLK_PIN 26
#define PA_EN_PIN 1
#define USB_DET_PIN 0
#define LED_PIN 1
#define POWER_EN_PIN 3
#define POWER_KEY_PIN 4
#define LCD_PWM_PIN 21
#define LCD_CS_PIN 22
#define LCD_CD_PIN 23
#define LCD_SPI_DATA_PIN 24
#define LCD_SPI_CLK_PIN 25
#define LCD_RST_PIN 9
void lisa_gpioa_pinmux(void);
void lisa_gpiob_pinmux(void);
void lisa_uart0_pinmux(void);
void lisa_pwm_pinmux(void);
void lisa_spi0_pinmux(void);
void lisa_i2c0_pinmux(void);
void lisa_dvp_pinmux(void);
