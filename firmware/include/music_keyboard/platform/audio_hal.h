#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t sample_rate_hz;
    uint16_t block_frames;
    uint8_t channels;
} mk_audio_hal_config_t;

bool mk_audio_hal_init(const mk_audio_hal_config_t *config);
void mk_audio_hal_shutdown(void);
size_t mk_audio_hal_writable_frames(void);
bool mk_audio_hal_submit_frames(const int16_t *interleaved_frames, size_t frame_count);
