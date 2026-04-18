#include <stdbool.h>
#include <string.h>

#include "music_keyboard/core/sequencer.h"

static uint8_t mk_default_sample_slot_for_sound(uint8_t sound_index) {
    if (sound_index < MK_MELODIC_SOUND_COUNT) {
        return (uint8_t)(MK_MELODIC_SAMPLE_SLOT_BASE +
                         (sound_index % MK_LOADED_MELODIC_SAMPLE_COUNT));
    }

    return (uint8_t)(MK_DRUM_SAMPLE_SLOT_BASE +
                     ((sound_index - MK_MELODIC_SOUND_COUNT) % MK_LOADED_DRUM_KIT_COUNT) *
                         MK_DRUM_SLICES_PER_KIT);
}

void mk_sequencer_init(mk_sequencer_t *sequencer) {
    memset(sequencer, 0, sizeof(*sequencer));

    for (uint8_t i = 0; i < MK_SOUND_CHANNEL_COUNT; ++i) {
        sequencer->sounds[i].enabled = true;
        sequencer->sounds[i].muted = false;
        sequencer->sounds[i].is_drum = (i >= MK_MELODIC_SOUND_COUNT);
        sequencer->sounds[i].sample_slot = mk_default_sample_slot_for_sound(i);
        sequencer->sounds[i].last_pad_index = sequencer->sounds[i].is_drum ? 0u : 4u;
        sequencer->sounds[i].level_0_127 = 100;
        sequencer->sounds[i].coarse_tune_semitones = 0;
    }
}

void mk_sequencer_clear_all_patterns(mk_sequencer_t *sequencer) {
    memset(sequencer->steps, 0, sizeof(sequencer->steps));
}

void mk_sequencer_clear_pattern(mk_sequencer_t *sequencer, uint8_t pattern) {
    if (pattern >= MK_PATTERN_SLOT_COUNT) {
        return;
    }

    for (uint8_t sound = 0; sound < MK_SOUND_CHANNEL_COUNT; ++sound) {
        memset(sequencer->steps[sound][pattern], 0, sizeof(sequencer->steps[sound][pattern]));
    }
}

void mk_sequencer_toggle_step(
    mk_sequencer_t *sequencer,
    uint8_t sound,
    uint8_t pattern,
    uint8_t step,
    const mk_step_event_t *event
) {
    mk_step_event_t *cell;

    if (sound >= MK_SOUND_CHANNEL_COUNT || pattern >= MK_PATTERN_SLOT_COUNT || step >= MK_STEPS_PER_BAR) {
        return;
    }

    cell = &sequencer->steps[sound][pattern][step];
    if (cell->enabled) {
        memset(cell, 0, sizeof(*cell));
    } else if (event != NULL) {
        *cell = *event;
        cell->enabled = true;
    }
}

void mk_sequencer_set_step(
    mk_sequencer_t *sequencer,
    uint8_t sound,
    uint8_t pattern,
    uint8_t step,
    const mk_step_event_t *event
) {
    if (sound >= MK_SOUND_CHANNEL_COUNT || pattern >= MK_PATTERN_SLOT_COUNT || step >= MK_STEPS_PER_BAR) {
        return;
    }

    if (event == NULL) {
        memset(&sequencer->steps[sound][pattern][step], 0, sizeof(sequencer->steps[sound][pattern][step]));
        return;
    }

    sequencer->steps[sound][pattern][step] = *event;
    sequencer->steps[sound][pattern][step].enabled = true;
}

const mk_step_event_t *mk_sequencer_get_step(
    const mk_sequencer_t *sequencer,
    uint8_t sound,
    uint8_t pattern,
    uint8_t step
) {
    if (sound >= MK_SOUND_CHANNEL_COUNT || pattern >= MK_PATTERN_SLOT_COUNT || step >= MK_STEPS_PER_BAR) {
        return NULL;
    }

    return &sequencer->steps[sound][pattern][step];
}

size_t mk_sequencer_collect_step_triggers(
    const mk_sequencer_t *sequencer,
    uint8_t pattern,
    uint8_t step,
    mk_step_event_t *out_events,
    size_t out_capacity
) {
    size_t count = 0;

    if (pattern >= MK_PATTERN_SLOT_COUNT || step >= MK_STEPS_PER_BAR) {
        return 0;
    }

    for (uint8_t i = 0; i < MK_SOUND_CHANNEL_COUNT; ++i) {
        const mk_sound_channel_t *sound = &sequencer->sounds[i];
        const mk_step_event_t *event = &sequencer->steps[i][pattern][step];

        if (!sound->enabled || sound->muted || !event->enabled) {
            continue;
        }
        if (count < out_capacity) {
            out_events[count++] = *event;
        }
    }

    return count;
}
