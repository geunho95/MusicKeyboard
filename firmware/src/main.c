#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "music_keyboard/app.h"
#include "music_keyboard/config.h"

static void mk_write_le_u16(FILE *file, uint16_t value) {
    uint8_t bytes[2];
    bytes[0] = (uint8_t)(value & 0xffu);
    bytes[1] = (uint8_t)((value >> 8) & 0xffu);
    fwrite(bytes, 1, sizeof(bytes), file);
}

static void mk_write_le_u32(FILE *file, uint32_t value) {
    uint8_t bytes[4];
    bytes[0] = (uint8_t)(value & 0xffu);
    bytes[1] = (uint8_t)((value >> 8) & 0xffu);
    bytes[2] = (uint8_t)((value >> 16) & 0xffu);
    bytes[3] = (uint8_t)((value >> 24) & 0xffu);
    fwrite(bytes, 1, sizeof(bytes), file);
}

static bool mk_write_preview_wav(
    const char *path,
    const int16_t *interleaved_frames,
    uint32_t frame_count
) {
    FILE *file = fopen(path, "wb");
    uint32_t data_size_bytes = frame_count * MK_AUDIO_CHANNELS * (uint32_t)sizeof(int16_t);
    uint32_t riff_size = 36u + data_size_bytes;
    uint32_t byte_rate = MK_DEFAULT_AUDIO_SAMPLE_RATE * MK_AUDIO_CHANNELS * (uint32_t)sizeof(int16_t);
    uint16_t block_align = (uint16_t)(MK_AUDIO_CHANNELS * sizeof(int16_t));

    if (file == NULL) {
        return false;
    }

    fwrite("RIFF", 1, 4, file);
    mk_write_le_u32(file, riff_size);
    fwrite("WAVE", 1, 4, file);

    fwrite("fmt ", 1, 4, file);
    mk_write_le_u32(file, 16u);
    mk_write_le_u16(file, 1u);
    mk_write_le_u16(file, MK_AUDIO_CHANNELS);
    mk_write_le_u32(file, MK_DEFAULT_AUDIO_SAMPLE_RATE);
    mk_write_le_u32(file, byte_rate);
    mk_write_le_u16(file, block_align);
    mk_write_le_u16(file, 16u);

    fwrite("data", 1, 4, file);
    mk_write_le_u32(file, data_size_bytes);
    fwrite(interleaved_frames, sizeof(int16_t), frame_count * MK_AUDIO_CHANNELS, file);

    fclose(file);
    return true;
}

int main(void) {
    mk_app_t app;
    const uint32_t preview_seconds = 10u;
    const uint32_t total_frames = MK_DEFAULT_AUDIO_SAMPLE_RATE * preview_seconds;
    const uint32_t total_blocks =
        (total_frames + (uint32_t)MK_AUDIO_BLOCK_FRAMES - 1u) / (uint32_t)MK_AUDIO_BLOCK_FRAMES;
    int16_t *preview_frames =
        (int16_t *)calloc(total_blocks * MK_AUDIO_BLOCK_FRAMES * MK_AUDIO_CHANNELS, sizeof(int16_t));
    double blocks_per_step;
    double next_tick_block = 0.0;

    if (preview_frames == NULL) {
        fputs("[host] failed to allocate preview buffer\n", stderr);
        return 1;
    }

    mk_app_init(&app);

    for (uint32_t block = 0; block < total_blocks; ++block) {
        int16_t *block_frames =
            &preview_frames[(size_t)block * MK_AUDIO_BLOCK_FRAMES * MK_AUDIO_CHANNELS];

        mk_app_tick(&app);
        blocks_per_step = ((double)MK_DEFAULT_AUDIO_SAMPLE_RATE * 60.0) /
                          ((double)app.transport.bpm * 4.0 * (double)MK_AUDIO_BLOCK_FRAMES);

        if ((double)block >= next_tick_block) {
            mk_app_step_transport(&app);
            next_tick_block += blocks_per_step;
        }

        mk_app_render_audio(&app, block_frames, MK_AUDIO_BLOCK_FRAMES);
    }

    if (mk_write_preview_wav("host_preview.wav", preview_frames, total_blocks * MK_AUDIO_BLOCK_FRAMES)) {
        puts("[host] wrote host_preview.wav");
    } else {
        puts("[host] failed to write host_preview.wav");
    }

    free(preview_frames);
    puts("[host] sampler skeleton run complete");
    return 0;
}
