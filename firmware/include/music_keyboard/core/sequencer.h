#pragma once

#include <stddef.h>
#include <stdint.h>

#include "music_keyboard/types.h"

void mk_sequencer_init(mk_sequencer_t *sequencer);
void mk_sequencer_clear_all_patterns(mk_sequencer_t *sequencer);
void mk_sequencer_clear_pattern(mk_sequencer_t *sequencer, uint8_t pattern);
void mk_sequencer_toggle_step(
    mk_sequencer_t *sequencer,
    uint8_t sound,
    uint8_t pattern,
    uint8_t step,
    const mk_step_event_t *event
);
void mk_sequencer_set_step(
    mk_sequencer_t *sequencer,
    uint8_t sound,
    uint8_t pattern,
    uint8_t step,
    const mk_step_event_t *event
);
const mk_step_event_t *mk_sequencer_get_step(
    const mk_sequencer_t *sequencer,
    uint8_t sound,
    uint8_t pattern,
    uint8_t step
);
size_t mk_sequencer_collect_step_triggers(
    const mk_sequencer_t *sequencer,
    uint8_t pattern,
    uint8_t step,
    mk_step_event_t *out_events,
    size_t out_capacity
);
