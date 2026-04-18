#pragma once

/*
 * Current hardware profile:
 *
 * Phase A bring-up:
 * - direct GPIO buttons
 * - SPI microSD module
 * - temporary PWM audio output
 *
 * Phase B target:
 * - D200 over UART
 * - I2S mic
 * - I2S mono amp
 */

#define MK_RP2350_SD_SCK_PIN 2
#define MK_RP2350_SD_MOSI_PIN 3
#define MK_RP2350_SD_MISO_PIN 4
#define MK_RP2350_SD_CS_PIN 5

#define MK_RP2350_BUTTON_BLUE_0_PIN 16
#define MK_RP2350_BUTTON_BLUE_1_PIN 17
#define MK_RP2350_BUTTON_BLUE_2_PIN 18
#define MK_RP2350_BUTTON_BLUE_3_PIN 19
#define MK_RP2350_BUTTON_RED_PIN 20

#define MK_RP2350_TEMP_AUDIO_PWM_PIN 15

#define MK_RP2350_D200_UART_TX_PIN 0
#define MK_RP2350_D200_UART_RX_PIN 1

#define MK_RP2350_I2S_OUT_BCLK_PIN 10
#define MK_RP2350_I2S_OUT_LRCK_PIN 11
#define MK_RP2350_I2S_OUT_DIN_PIN 12

#define MK_RP2350_I2S_MIC_SCK_PIN 13
#define MK_RP2350_I2S_MIC_WS_PIN 14
#define MK_RP2350_I2S_MIC_SD_PIN 21

#define MK_RP2350_BUTTON_SPARE_0_PIN 6
#define MK_RP2350_BUTTON_SPARE_1_PIN 7

#define MK_RP2350_ONBOARD_LED_PIN 25
