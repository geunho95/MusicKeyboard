#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/sync.h"

#include "music_keyboard/config.h"
#include "music_keyboard/platform/audio_hal.h"
#include "music_keyboard/platform/rp2350_pinmap.h"

#include "audio_i2s_out.pio.h"

/* Double-buffer: DMA plays one block while CPU fills the other */
#define MK_I2S_BUF_FRAMES MK_AUDIO_BLOCK_FRAMES

static uint32_t g_buf[2][MK_I2S_BUF_FRAMES];
static volatile int    g_play_idx;
static volatile int    g_fill_idx;
static volatile size_t g_fill_pos;
static volatile bool   g_fill_ready;
static int  g_dma_chan;
static uint g_pio_sm;
static PIO  g_pio;
static bool g_initialized;

static void __isr mk_i2s_dma_handler(void) {
    dma_hw->ints0 = (1u << g_dma_chan);

    if (g_fill_ready) {
        g_play_idx    = g_fill_idx;
        g_fill_idx   ^= 1;
        g_fill_pos    = 0;
        g_fill_ready  = false;
    }

    dma_channel_set_read_addr(g_dma_chan, g_buf[g_play_idx], false);
    dma_channel_set_trans_count(g_dma_chan, MK_I2S_BUF_FRAMES, true);
}

bool mk_audio_hal_init(const mk_audio_hal_config_t *config) {
    uint offset;
    pio_sm_config c;
    dma_channel_config dc;

    g_pio    = pio0;
    g_pio_sm = pio_claim_unused_sm(g_pio, true);
    offset   = pio_add_program(g_pio, &audio_i2s_out_program);

    /* GP10=BCLK, GP11=LRCLK, GP12=DIN — all outputs */
    pio_sm_set_consecutive_pindirs(g_pio, g_pio_sm, MK_RP2350_I2S_OUT_BCLK_PIN, 3, true);
    pio_gpio_init(g_pio, MK_RP2350_I2S_OUT_BCLK_PIN);
    pio_gpio_init(g_pio, MK_RP2350_I2S_OUT_LRCK_PIN);
    pio_gpio_init(g_pio, MK_RP2350_I2S_OUT_DIN_PIN);

    c = audio_i2s_out_program_get_default_config(offset);
    sm_config_set_out_pins(&c, MK_RP2350_I2S_OUT_DIN_PIN, 1);
    sm_config_set_sideset_pins(&c, MK_RP2350_I2S_OUT_BCLK_PIN);
    sm_config_set_out_shift(&c, false, true, 32); /* MSB first, autopull at 32 bits */
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    /* PIO clock = sample_rate × 64 (32 BCLK cycles × 2 PIO cycles per BCLK) */
    float clk_div = (float)clock_get_hz(clk_sys) / ((float)config->sample_rate_hz * 64.0f);
    sm_config_set_clkdiv(&c, clk_div);

    pio_sm_init(g_pio, g_pio_sm, offset, &c);
    pio_sm_set_enabled(g_pio, g_pio_sm, true);

    memset(g_buf, 0, sizeof(g_buf));
    g_play_idx   = 0;
    g_fill_idx   = 1;
    g_fill_pos   = 0;
    g_fill_ready = false;

    g_dma_chan = dma_claim_unused_channel(true);
    dc = dma_channel_get_default_config(g_dma_chan);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
    channel_config_set_read_increment(&dc, true);
    channel_config_set_write_increment(&dc, false);
    channel_config_set_dreq(&dc, pio_get_dreq(g_pio, g_pio_sm, true));

    dma_channel_configure(g_dma_chan, &dc,
        &g_pio->txf[g_pio_sm],
        g_buf[g_play_idx],
        MK_I2S_BUF_FRAMES,
        false);

    dma_channel_set_irq0_enabled(g_dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, mk_i2s_dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_channel_start(g_dma_chan);

    g_initialized = true;
    return true;
}

void mk_audio_hal_shutdown(void) {
    if (!g_initialized) return;
    irq_set_enabled(DMA_IRQ_0, false);
    dma_channel_abort(g_dma_chan);
    dma_channel_unclaim(g_dma_chan);
    pio_sm_set_enabled(g_pio, g_pio_sm, false);
    pio_sm_unclaim(g_pio, g_pio_sm);
    g_initialized = false;
}

size_t mk_audio_hal_writable_frames(void) {
    uint32_t saved = save_and_disable_interrupts();
    size_t n = g_fill_ready ? 0u : (size_t)(MK_I2S_BUF_FRAMES - g_fill_pos);
    restore_interrupts(saved);
    return n;
}

bool mk_audio_hal_submit_frames(const int16_t *interleaved_frames, size_t frame_count) {
    uint32_t saved;

    if (frame_count > mk_audio_hal_writable_frames()) return false;

    for (size_t i = 0; i < frame_count; ++i) {
        int16_t left  = interleaved_frames[i * MK_AUDIO_CHANNELS];
        int16_t right = (MK_AUDIO_CHANNELS > 1)
            ? interleaved_frames[i * MK_AUDIO_CHANNELS + 1]
            : left;
        g_buf[g_fill_idx][g_fill_pos + i] =
            ((uint32_t)(uint16_t)left << 16) | (uint16_t)right;
    }

    saved = save_and_disable_interrupts();
    g_fill_pos += frame_count;
    if (g_fill_pos >= (size_t)MK_I2S_BUF_FRAMES) {
        g_fill_ready = true;
    }
    restore_interrupts(saved);

    return true;
}
