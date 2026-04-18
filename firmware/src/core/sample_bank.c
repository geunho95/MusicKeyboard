#include <stdio.h>
#include <string.h>

#include "music_keyboard/config.h"
#include "music_keyboard/core/sample_bank.h"

void mk_sample_bank_init(mk_sample_bank_t *bank) {
    memset(bank, 0, sizeof(*bank));

    for (uint8_t i = 0; i < MK_SAMPLE_SLOT_COUNT; ++i) {
        if (i < MK_LOADED_MELODIC_SAMPLE_COUNT) {
            snprintf(bank->slots[i].name, sizeof(bank->slots[i].name), "sound-%u", (unsigned)(i + 1u));
            bank->slots[i].root_note = 61;
        } else {
            uint8_t drum_index = (uint8_t)(i - MK_DRUM_SAMPLE_SLOT_BASE);
            uint8_t kit = (uint8_t)(9u + (drum_index / MK_DRUM_SLICES_PER_KIT));
            uint8_t slice = (uint8_t)(1u + (drum_index % MK_DRUM_SLICES_PER_KIT));
            snprintf(bank->slots[i].name, sizeof(bank->slots[i].name), "sound-%u-%u", kit, slice);
            bank->slots[i].root_note = 36;
        }

        bank->slots[i].occupied = false;
        bank->slots[i].loop_enabled = false;
        bank->slots[i].frame_count = 0u;
        bank->slots[i].sample_rate_hz = MK_DEFAULT_AUDIO_SAMPLE_RATE;
        bank->slots[i].pcm_frames = NULL;
        bank->slots[i].owns_pcm_frames = false;
    }
}

const mk_sample_slot_t *mk_sample_bank_get(const mk_sample_bank_t *bank, uint8_t slot) {
    if (slot >= MK_SAMPLE_SLOT_COUNT) {
        return NULL;
    }
    return &bank->slots[slot];
}
