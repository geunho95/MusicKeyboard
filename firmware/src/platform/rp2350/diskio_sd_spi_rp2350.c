#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "ff.h"
#include "diskio.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "music_keyboard/platform/rp2350_pinmap.h"

#define MK_RP2350_SD_SPI spi0
#define MK_RP2350_SD_SECTOR_SIZE 512u
#define MK_RP2350_SD_INIT_BAUD 400000u
#define MK_RP2350_SD_RUN_BAUD 12000000u

#define MK_RP2350_SD_CMD0 0u
#define MK_RP2350_SD_CMD1 1u
#define MK_RP2350_SD_CMD8 8u
#define MK_RP2350_SD_CMD9 9u
#define MK_RP2350_SD_CMD12 12u
#define MK_RP2350_SD_CMD16 16u
#define MK_RP2350_SD_CMD17 17u
#define MK_RP2350_SD_CMD55 55u
#define MK_RP2350_SD_CMD58 58u
#define MK_RP2350_SD_ACMD41 41u

#define MK_RP2350_SD_R1_IDLE 0x01u
#define MK_RP2350_SD_R1_ILLEGAL_COMMAND 0x04u
#define MK_RP2350_SD_TOKEN_START_BLOCK 0xfeu

static DSTATUS g_disk_status = STA_NOINIT;
static bool g_spi_ready = false;
static bool g_block_addressing = false;
static uint32_t g_sector_count = 0u;

static uint8_t mk_sd_spi_transfer(uint8_t tx) {
    uint8_t rx = 0xffu;

    spi_write_read_blocking(MK_RP2350_SD_SPI, &tx, &rx, 1);
    return rx;
}

static void mk_sd_spi_fill(uint8_t *buffer, size_t count) {
    if (count == 0u) {
        return;
    }

    spi_read_blocking(MK_RP2350_SD_SPI, 0xffu, buffer, count);
}

static void mk_sd_spi_deselect(void) {
    gpio_put(MK_RP2350_SD_CS_PIN, 1);
    mk_sd_spi_transfer(0xffu);
}

static bool mk_sd_spi_wait_ready(uint32_t timeout_ms) {
    absolute_time_t deadline = delayed_by_ms(get_absolute_time(), timeout_ms);

    do {
        if (mk_sd_spi_transfer(0xffu) == 0xffu) {
            return true;
        }
    } while (absolute_time_diff_us(get_absolute_time(), deadline) > 0);

    return false;
}

static bool mk_sd_spi_select(void) {
    gpio_put(MK_RP2350_SD_CS_PIN, 0);

    if (mk_sd_spi_wait_ready(500u)) {
        return true;
    }

    mk_sd_spi_deselect();
    return false;
}

static uint8_t mk_sd_spi_command(uint8_t command, uint32_t argument, uint8_t *response, size_t response_size) {
    uint8_t crc = 0x01u;
    uint8_t r1 = 0xffu;

    if (command == MK_RP2350_SD_CMD0) {
        crc = 0x95u;
    } else if (command == MK_RP2350_SD_CMD8) {
        crc = 0x87u;
    }

    if (!mk_sd_spi_select()) {
        return 0xffu;
    }

    mk_sd_spi_transfer((uint8_t)(0x40u | command));
    mk_sd_spi_transfer((uint8_t)(argument >> 24));
    mk_sd_spi_transfer((uint8_t)(argument >> 16));
    mk_sd_spi_transfer((uint8_t)(argument >> 8));
    mk_sd_spi_transfer((uint8_t)argument);
    mk_sd_spi_transfer(crc);

    if (command == MK_RP2350_SD_CMD12) {
        mk_sd_spi_transfer(0xffu);
    }

    for (uint8_t attempt = 0; attempt < 10u; ++attempt) {
        r1 = mk_sd_spi_transfer(0xffu);
        if ((r1 & 0x80u) == 0u) {
            break;
        }
    }

    for (size_t i = 0; i < response_size; ++i) {
        response[i] = mk_sd_spi_transfer(0xffu);
    }

    return r1;
}

static bool mk_sd_spi_wait_data_token(uint8_t expected_token, uint32_t timeout_ms) {
    absolute_time_t deadline = delayed_by_ms(get_absolute_time(), timeout_ms);

    do {
        uint8_t token = mk_sd_spi_transfer(0xffu);

        if (token == expected_token) {
            return true;
        }

        if (token != 0xffu) {
            return false;
        }
    } while (absolute_time_diff_us(get_absolute_time(), deadline) > 0);

    return false;
}

static bool mk_sd_spi_read_data(uint8_t *buffer, size_t count) {
    if (!mk_sd_spi_wait_data_token(MK_RP2350_SD_TOKEN_START_BLOCK, 200u)) {
        return false;
    }

    mk_sd_spi_fill(buffer, count);
    mk_sd_spi_transfer(0xffu);
    mk_sd_spi_transfer(0xffu);
    return true;
}

static bool mk_sd_spi_read_csd(uint8_t csd[16]) {
    uint8_t r1 = mk_sd_spi_command(MK_RP2350_SD_CMD9, 0u, NULL, 0u);
    bool ok = false;

    if (r1 == 0u) {
        ok = mk_sd_spi_read_data(csd, 16u);
    }

    mk_sd_spi_deselect();
    return ok;
}

static uint32_t mk_sd_spi_parse_sector_count(const uint8_t csd[16]) {
    if (((csd[0] >> 6) & 0x03u) == 1u) {
        uint32_t c_size = ((uint32_t)(csd[7] & 0x3fu) << 16) |
                          ((uint32_t)csd[8] << 8) |
                          (uint32_t)csd[9];

        return (c_size + 1u) * 1024u;
    }

    {
        uint32_t c_size = ((uint32_t)(csd[6] & 0x03u) << 10) |
                          ((uint32_t)csd[7] << 2) |
                          ((uint32_t)(csd[8] >> 6) & 0x03u);
        uint32_t c_size_mult = ((uint32_t)(csd[9] & 0x03u) << 1) |
                               ((uint32_t)(csd[10] >> 7) & 0x01u);
        uint32_t read_bl_len = (uint32_t)(csd[5] & 0x0fu);
        uint32_t blocknr = (c_size + 1u) << (c_size_mult + 2u);
        uint32_t block_len = 1u << read_bl_len;
        uint64_t capacity_bytes = (uint64_t)blocknr * (uint64_t)block_len;

        return (uint32_t)(capacity_bytes / MK_RP2350_SD_SECTOR_SIZE);
    }
}

static void mk_sd_spi_bus_init(void) {
    if (g_spi_ready) {
        return;
    }

    spi_init(MK_RP2350_SD_SPI, MK_RP2350_SD_INIT_BAUD);
    spi_set_format(MK_RP2350_SD_SPI, 8u, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(MK_RP2350_SD_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MK_RP2350_SD_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MK_RP2350_SD_MISO_PIN, GPIO_FUNC_SPI);
    gpio_pull_up(MK_RP2350_SD_MISO_PIN);

    gpio_init(MK_RP2350_SD_CS_PIN);
    gpio_set_dir(MK_RP2350_SD_CS_PIN, GPIO_OUT);
    gpio_put(MK_RP2350_SD_CS_PIN, 1);

    for (uint8_t i = 0; i < 12u; ++i) {
        mk_sd_spi_transfer(0xffu);
    }

    g_spi_ready = true;
}

static bool mk_sd_spi_reset_card(void) {
    for (uint8_t attempt = 0; attempt < 10u; ++attempt) {
        uint8_t r1 = mk_sd_spi_command(MK_RP2350_SD_CMD0, 0u, NULL, 0u);

        mk_sd_spi_deselect();
        if (r1 == MK_RP2350_SD_R1_IDLE) {
            return true;
        }
        sleep_ms(10);
    }

    return false;
}

static bool mk_sd_spi_initialize_card(void) {
    uint8_t r7[4] = {0};
    uint8_t ocr[4] = {0};
    uint8_t csd[16] = {0};
    uint8_t r1;
    absolute_time_t deadline;

    mk_sd_spi_bus_init();
    g_block_addressing = false;
    g_sector_count = 0u;

    if (!mk_sd_spi_reset_card()) {
        return false;
    }

    r1 = mk_sd_spi_command(MK_RP2350_SD_CMD8, 0x000001aau, r7, sizeof(r7));
    mk_sd_spi_deselect();

    if (r1 == MK_RP2350_SD_R1_IDLE) {
        deadline = delayed_by_ms(get_absolute_time(), 1500u);
        do {
            r1 = mk_sd_spi_command(MK_RP2350_SD_CMD55, 0u, NULL, 0u);
            mk_sd_spi_deselect();

            if (r1 <= 1u) {
                r1 = mk_sd_spi_command(MK_RP2350_SD_ACMD41, 0x40000000u, NULL, 0u);
                mk_sd_spi_deselect();
                if (r1 == 0u) {
                    break;
                }
            }

            sleep_ms(20);
        } while (absolute_time_diff_us(get_absolute_time(), deadline) > 0);

        if (r1 != 0u) {
            return false;
        }

        r1 = mk_sd_spi_command(MK_RP2350_SD_CMD58, 0u, ocr, sizeof(ocr));
        mk_sd_spi_deselect();
        if (r1 != 0u) {
            return false;
        }

        g_block_addressing = (ocr[0] & 0x40u) != 0u;
    } else if ((r1 & MK_RP2350_SD_R1_ILLEGAL_COMMAND) != 0u) {
        deadline = delayed_by_ms(get_absolute_time(), 1500u);
        do {
            r1 = mk_sd_spi_command(MK_RP2350_SD_CMD55, 0u, NULL, 0u);
            mk_sd_spi_deselect();

            if (r1 <= 1u) {
                r1 = mk_sd_spi_command(MK_RP2350_SD_ACMD41, 0u, NULL, 0u);
                mk_sd_spi_deselect();
            } else {
                r1 = mk_sd_spi_command(MK_RP2350_SD_CMD1, 0u, NULL, 0u);
                mk_sd_spi_deselect();
            }

            if (r1 == 0u) {
                break;
            }

            sleep_ms(20);
        } while (absolute_time_diff_us(get_absolute_time(), deadline) > 0);

        if (r1 != 0u) {
            return false;
        }

        r1 = mk_sd_spi_command(MK_RP2350_SD_CMD16, MK_RP2350_SD_SECTOR_SIZE, NULL, 0u);
        mk_sd_spi_deselect();
        if (r1 != 0u) {
            return false;
        }
    } else {
        return false;
    }

    spi_set_baudrate(MK_RP2350_SD_SPI, MK_RP2350_SD_RUN_BAUD);

    if (mk_sd_spi_read_csd(csd)) {
        g_sector_count = mk_sd_spi_parse_sector_count(csd);
    }

    g_disk_status = 0u;
    return true;
}

DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0u) {
        return STA_NOINIT;
    }

    if (mk_sd_spi_initialize_card()) {
        return g_disk_status;
    }

    g_disk_status = STA_NOINIT;
    return g_disk_status;
}

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0u) {
        return STA_NOINIT;
    }

    return g_disk_status;
}

DRESULT disk_read(BYTE pdrv, BYTE *buffer, LBA_t sector, UINT count) {
    if (pdrv != 0u || count == 0u || buffer == NULL) {
        return RES_PARERR;
    }

    if ((g_disk_status & STA_NOINIT) != 0u && disk_initialize(pdrv) != 0u) {
        return RES_NOTRDY;
    }

    for (UINT index = 0; index < count; ++index) {
        uint32_t address = g_block_addressing ? (uint32_t)(sector + index)
                                              : (uint32_t)((sector + index) * MK_RP2350_SD_SECTOR_SIZE);
        uint8_t r1 = mk_sd_spi_command(MK_RP2350_SD_CMD17, address, NULL, 0u);
        bool ok = false;

        if (r1 == 0u) {
            ok = mk_sd_spi_read_data(
                &buffer[index * MK_RP2350_SD_SECTOR_SIZE],
                MK_RP2350_SD_SECTOR_SIZE
            );
        }

        mk_sd_spi_deselect();
        if (!ok) {
            return RES_ERROR;
        }
    }

    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buffer, LBA_t sector, UINT count) {
    (void)pdrv;
    (void)buffer;
    (void)sector;
    (void)count;
    return RES_WRPRT;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE command, void *buffer) {
    if (pdrv != 0u) {
        return RES_PARERR;
    }

    if ((g_disk_status & STA_NOINIT) != 0u && command != CTRL_SYNC) {
        return RES_NOTRDY;
    }

    switch (command) {
        case CTRL_SYNC:
            return RES_OK;
        case GET_SECTOR_COUNT:
            if (buffer == NULL || g_sector_count == 0u) {
                return RES_PARERR;
            }
            *(LBA_t *)buffer = (LBA_t)g_sector_count;
            return RES_OK;
        case GET_SECTOR_SIZE:
            if (buffer == NULL) {
                return RES_PARERR;
            }
            *(WORD *)buffer = MK_RP2350_SD_SECTOR_SIZE;
            return RES_OK;
        case GET_BLOCK_SIZE:
            if (buffer == NULL) {
                return RES_PARERR;
            }
            *(DWORD *)buffer = 1u;
            return RES_OK;
        default:
            return RES_PARERR;
    }
}
