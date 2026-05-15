#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "music_keyboard/config.h"

typedef enum {
    MK_RECORD_IDLE = 0,
    MK_RECORD_ARMED,
    MK_RECORD_ACTIVE,
} mk_record_state_t;

typedef enum {
    MK_VIEW_PERFORMANCE = 0,
    MK_VIEW_EDIT,
    MK_VIEW_LIVE_RECORD,
    MK_VIEW_SOUND_SELECT,
    MK_VIEW_PATTERN_SELECT,
    MK_VIEW_BPM,
    MK_VIEW_FX,
    MK_VIEW_SAMPLE_SELECT,
} mk_ui_view_t;

typedef enum {
    MK_FX_PAGE_PITCH = 0,
    MK_FX_PAGE_VOLUME,
    MK_FX_PAGE_TRIM,
} mk_fx_page_t;

typedef enum {
    MK_BUTTON_EVENT_NONE = 0,
    MK_BUTTON_EVENT_PRESS,
    MK_BUTTON_EVENT_RELEASE,
    MK_BUTTON_EVENT_ENC_CW,   /* 엔코더 시계방향 (1클릭) */
    MK_BUTTON_EVENT_ENC_CCW,  /* 엔코더 반시계방향 (1클릭) */
} mk_button_event_type_t;

typedef struct {
    mk_button_event_type_t type;
    uint8_t id;
} mk_button_event_t;

typedef struct {
    uint16_t bpm;
    uint8_t bars;
    uint16_t current_step;
    bool running;
} mk_transport_t;

typedef struct {
    char name[24];
    bool occupied;
    bool loop_enabled;
    uint32_t frame_count;
    uint32_t sample_rate_hz;
    int8_t root_note;
    const int16_t *pcm_frames;
    bool owns_pcm_frames;
} mk_sample_slot_t;

typedef struct {
    mk_sample_slot_t slots[MK_SAMPLE_SLOT_COUNT];
} mk_sample_bank_t;

typedef struct {
    bool enabled;
    bool muted;
    bool is_drum;
    uint8_t sample_slot;
    uint8_t last_pad_index;
    uint8_t level_0_127;
    int8_t coarse_tune_semitones;
} mk_sound_channel_t;

typedef struct {
    bool enabled;
    uint8_t sample_slot;
    uint8_t pad_index;
    uint8_t gain_0_127;
    float rate;
} mk_step_event_t;

typedef struct {
    mk_sound_channel_t sounds[MK_SOUND_CHANNEL_COUNT];
    mk_step_event_t steps[MK_SOUND_CHANNEL_COUNT][MK_PATTERN_SLOT_COUNT][MK_STEPS_PER_BAR];
} mk_sequencer_t;

typedef struct {
    bool active;
    uint8_t sample_slot;
    uint8_t gain_0_127;
    float rate;
    float position_frames;
} mk_voice_t;

typedef struct {
    mk_voice_t voices[MK_MAX_VOICES];
} mk_audio_engine_t;

typedef struct {
    mk_transport_t transport;
    mk_sample_bank_t sample_bank;
    mk_sequencer_t sequencer;
    mk_audio_engine_t audio;
    mk_ui_view_t view;
    mk_fx_page_t fx_page;
    mk_record_state_t record_state;
    uint8_t selected_sound;
    uint8_t current_pattern;
    uint8_t pattern_chain[MK_PATTERN_SLOT_COUNT];
    uint8_t pattern_chain_length;
    uint8_t pattern_chain_index;
    uint8_t master_level_0_127;
    bool dirty;
    /* 샘플 선택 뷰 */
    char sample_names[64][32];
    uint8_t sample_count;
    uint8_t sample_cursor;
} mk_app_t;
