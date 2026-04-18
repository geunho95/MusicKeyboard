#pragma once

#include <stddef.h>
#include <stdint.h>

#include "music_keyboard/types.h"

void mk_audio_engine_init(mk_audio_engine_t *engine);
void mk_audio_engine_trigger_slot(
    mk_audio_engine_t *engine,
    const mk_sample_bank_t *bank,
    uint8_t slot,
    float rate,
    uint8_t gain_0_127
);
void mk_audio_engine_render(
    mk_audio_engine_t *engine,
    const mk_sample_bank_t *bank,
    int16_t *interleaved_frames,
    size_t frame_count
);
void mk_audio_engine_stop_slot(mk_audio_engine_t *engine, uint8_t slot);
size_t mk_audio_engine_active_voice_count(const mk_audio_engine_t *engine);
