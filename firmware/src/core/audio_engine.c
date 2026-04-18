#include <stddef.h>
#include <string.h>

#include "music_keyboard/core/audio_engine.h"
#include "music_keyboard/config.h"
#include "music_keyboard/core/sample_bank.h"

static int16_t mk_clamp_int16(int32_t value) {
    if (value > 32767) {
        return 32767;
    }
    if (value < -32768) {
        return -32768;
    }
    return (int16_t)value;
}

void mk_audio_engine_init(mk_audio_engine_t *engine) {
    memset(engine, 0, sizeof(*engine));
}

static mk_voice_t *mk_audio_engine_claim_voice(mk_audio_engine_t *engine) {
    for (uint8_t i = 0; i < MK_MAX_VOICES; ++i) {
        if (!engine->voices[i].active) {
            return &engine->voices[i];
        }
    }
    return &engine->voices[0];
}

void mk_audio_engine_trigger_slot(
    mk_audio_engine_t *engine,
    const mk_sample_bank_t *bank,
    uint8_t slot,
    float rate,
    uint8_t gain_0_127
) {
    const mk_sample_slot_t *sample = mk_sample_bank_get(bank, slot);
    mk_voice_t *voice;

    if (sample == NULL || !sample->occupied || sample->pcm_frames == NULL || sample->frame_count == 0u) {
        return;
    }

    voice = mk_audio_engine_claim_voice(engine);
    voice->active = true;
    voice->sample_slot = slot;
    voice->gain_0_127 = gain_0_127;
    voice->rate = rate;
    voice->position_frames = 0.0f;
}

void mk_audio_engine_render(
    mk_audio_engine_t *engine,
    const mk_sample_bank_t *bank,
    int16_t *interleaved_frames,
    size_t frame_count
) {
    memset(interleaved_frames, 0, sizeof(int16_t) * frame_count * MK_AUDIO_CHANNELS);

    for (size_t frame = 0; frame < frame_count; ++frame) {
        int32_t mixed_mono = 0;

        for (uint8_t voice_index = 0; voice_index < MK_MAX_VOICES; ++voice_index) {
            mk_voice_t *voice = &engine->voices[voice_index];
            const mk_sample_slot_t *sample;
            uint32_t frame_index;
            uint32_t next_frame_index;
            float frac;
            float step;
            int32_t s0;
            int32_t s1;
            float interpolated;

            if (!voice->active) {
                continue;
            }

            sample = mk_sample_bank_get(bank, voice->sample_slot);
            if (sample == NULL || sample->pcm_frames == NULL || sample->frame_count == 0u) {
                voice->active = false;
                continue;
            }

            if (voice->position_frames >= (float)sample->frame_count) {
                voice->active = false;
                continue;
            }

            frame_index = (uint32_t)voice->position_frames;
            next_frame_index = (frame_index + 1u < sample->frame_count) ? (frame_index + 1u) : frame_index;
            frac = voice->position_frames - (float)frame_index;
            s0 = sample->pcm_frames[frame_index];
            s1 = sample->pcm_frames[next_frame_index];
            interpolated = (float)s0 + ((float)(s1 - s0) * frac);

            mixed_mono += (int32_t)((interpolated * (float)voice->gain_0_127) / 127.0f);

            step = ((float)sample->sample_rate_hz / (float)MK_DEFAULT_AUDIO_SAMPLE_RATE) * voice->rate;
            voice->position_frames += step;
            if (voice->position_frames >= (float)sample->frame_count) {
                voice->active = false;
            }
        }

        interleaved_frames[(frame * MK_AUDIO_CHANNELS)] = mk_clamp_int16(mixed_mono);
        interleaved_frames[(frame * MK_AUDIO_CHANNELS) + 1u] = mk_clamp_int16(mixed_mono);
    }
}

void mk_audio_engine_stop_slot(mk_audio_engine_t *engine, uint8_t slot) {
    for (uint8_t i = 0; i < MK_MAX_VOICES; ++i) {
        if (engine->voices[i].active && engine->voices[i].sample_slot == slot) {
            engine->voices[i].active = false;
        }
    }
}

size_t mk_audio_engine_active_voice_count(const mk_audio_engine_t *engine) {
    size_t count = 0;

    for (uint8_t i = 0; i < MK_MAX_VOICES; ++i) {
        if (engine->voices[i].active) {
            ++count;
        }
    }

    return count;
}
