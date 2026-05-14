#pragma once

/*
 * RP2350 GPIO 핀맵 (Pico 2 보드 기준, 외부 핀 GP0~GP22, GP26~GP28 사용)
 *
 * 부품 목록:
 *  - ST7789  : SPI LCD (SPI0 공유, RST→3.3V 직결)
 *  - TF카드  : SPI SD카드 (SPI0 공유)
 *  - MAX98357A : I2S 모노 앰프 (PIO)
 *  - ICS43434  : I2S MEMS 마이크 (PIO)
 *  - 4×4 버튼 매트릭스 (12키 피아노 + 기타버튼3 + 미사용1)
 *  - 로터리 엔코더 2개 (각 A + B + SW = 3 GPIO) ← ENC3 제거 (Pico 2 내부핀 충돌)
 *
 * SPI0 공유:
 *  - SD   : SD_CS  low, LCD_CS high, baudrate 8~25MHz
 *  - LCD  : LCD_CS low, SD_CS  high, baudrate 32~62MHz
 */

/* ── SPI0 공유 버스 (SD + LCD) ─────────────────────────────── */
#define MK_RP2350_SPI_SCK_PIN           2
#define MK_RP2350_SPI_MOSI_PIN          3
#define MK_RP2350_SPI_MISO_PIN          4   /* SD 전용 */

/* ── TF 카드 (SPI0) ────────────────────────────────────────── */
#define MK_RP2350_SD_SCK_PIN            MK_RP2350_SPI_SCK_PIN
#define MK_RP2350_SD_MOSI_PIN           MK_RP2350_SPI_MOSI_PIN
#define MK_RP2350_SD_MISO_PIN           MK_RP2350_SPI_MISO_PIN
#define MK_RP2350_SD_CS_PIN             5

/* ── ST7789 LCD (SPI0 공유) ────────────────────────────────── */
#define MK_RP2350_LCD_DC_PIN            0
#define MK_RP2350_LCD_CS_PIN            1
#define MK_RP2350_LCD_RST_PIN           21  /* 임시: I2S_MIC_SD 자리 — LCD 테스트용 */
#define MK_RP2350_LCD_SCK_PIN           MK_RP2350_SPI_SCK_PIN
#define MK_RP2350_LCD_MOSI_PIN          MK_RP2350_SPI_MOSI_PIN
/* BLK → 3.3V 직결 (항상 켜짐) */

/* ── 로터리 엔코더 (A + B + SW = GPIO 3개씩) ───────────────── */
#define MK_RP2350_ENC1_A_PIN            6   /* 엔코더1 CLK */
#define MK_RP2350_ENC1_B_PIN            7   /* 엔코더1 DT  */
#define MK_RP2350_ENC1_SW_PIN           17  /* 엔코더1 푸시 */

#define MK_RP2350_ENC2_A_PIN            8   /* 엔코더2 CLK */
#define MK_RP2350_ENC2_B_PIN            9   /* 엔코더2 DT  */
#define MK_RP2350_ENC2_SW_PIN           18  /* 엔코더2 푸시 */

/* ENC3 제거 — GP15, GP16, GP19 → 매트릭스 ROW2, ROW3, COL3 로 전환 */

/* ── I2S 출력 — MAX98357A (PIO) ────────────────────────────── */
#define MK_RP2350_I2S_OUT_BCLK_PIN      10
#define MK_RP2350_I2S_OUT_LRCK_PIN      11
#define MK_RP2350_I2S_OUT_DIN_PIN       12

/* ── I2S 마이크 — ICS43434 (PIO) ───────────────────────────── */
#define MK_RP2350_I2S_MIC_SCK_PIN       13
#define MK_RP2350_I2S_MIC_WS_PIN        14
#define MK_RP2350_I2S_MIC_SD_PIN        21

/* ── 4×4 버튼 매트릭스 ──────────────────────────────────────── */
/*
 * 배치 (15개 사용, 1칸 미사용):
 *
 *          COL0    COL1    COL2    COL3
 *         (GP26)  (GP27)  (GP28)  (GP19)
 *
 * ROW0(GP20) [ C  ]  [ C# ]  [ D  ]  [ D# ]
 * ROW1(GP22) [ E  ]  [ F  ]  [ F# ]  [ G  ]
 * ROW2(GP15) [ G# ]  [ A  ]  [ A# ]  [ B  ]
 * ROW3(GP16) [ FN1]  [ FN2]  [ FN3]  [ -- ]  ← 미사용
 *
 * 스캔: COL → OUTPUT (순서대로 LOW 구동)
 *       ROW → INPUT  (내부 풀업, LOW 감지)
 * 다이오드 필수 (1N4148, A→ROW, K(띠)→COL)
 */
#define MK_RP2350_MATRIX_ROW0_PIN       20
#define MK_RP2350_MATRIX_ROW1_PIN       22
#define MK_RP2350_MATRIX_ROW2_PIN       15  /* 구 ENC3_A */
#define MK_RP2350_MATRIX_ROW3_PIN       16  /* 구 ENC3_B */

#define MK_RP2350_MATRIX_COL0_PIN       26
#define MK_RP2350_MATRIX_COL1_PIN       27
#define MK_RP2350_MATRIX_COL2_PIN       28
#define MK_RP2350_MATRIX_COL3_PIN       19  /* 구 ENC3_SW */

/* ── 온보드 LED ─────────────────────────────────────────────── */
#define MK_RP2350_ONBOARD_LED_PIN       25
