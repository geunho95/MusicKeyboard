#include <stdio.h>

#include "music_keyboard/config.h"
#include "music_keyboard/platform/audio_hal.h"

bool mk_audio_hal_init(const mk_audio_hal_config_t *config) {
    printf(
        "[host/audio] init sample_rate=%u block_frames=%u channels=%u\n",
        config->sample_rate_hz,
        config->block_frames,
        config->channels
    );
    return true;
}

void mk_audio_hal_shutdown(void) {
    puts("[host/audio] shutdown");
}

size_t mk_audio_hal_writable_frames(void) {
    return (size_t)(MK_AUDIO_BLOCK_FRAMES * 32u);
}

bool mk_audio_hal_submit_frames(const int16_t *interleaved_frames, size_t frame_count) {
    (void)interleaved_frames;
    (void)frame_count;
    return true;
}
