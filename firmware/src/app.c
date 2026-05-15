#include <math.h>
#include <stdio.h>
#include <string.h>

#include "music_keyboard/app.h"
#include "music_keyboard/button_map.h"
#include "music_keyboard/config.h"
#include "music_keyboard/core/audio_engine.h"
#include "music_keyboard/core/sample_bank.h"
#include "music_keyboard/core/sequencer.h"
#include "music_keyboard/core/transport.h"
#include "music_keyboard/platform/audio_hal.h"
#include "music_keyboard/platform/d200_link.h"
#include "music_keyboard/platform/storage_hal.h"

static const float mk_po33_note_rates[MK_STEPS_PER_BAR] = {
    1.49830708f, 1.58740105f, 1.78179744f, 1.88774863f,
    1.00000000f, 1.12246205f, 1.18920712f, 1.33483985f,
    0.74915354f, 0.79370053f, 0.89089872f, 0.94387431f,
    0.50000000f, 0.56123102f, 0.59460356f, 0.66741993f,
};

static float mk_pow_semitones(int8_t semitones) {
    return powf(2.0f, (float)semitones / 12.0f);
}

static bool mk_app_sound_is_drum(uint8_t sound_index) {
    return sound_index >= MK_MELODIC_SOUND_COUNT;
}

static uint8_t mk_app_active_pattern(const mk_app_t *app) {
    if (app->pattern_chain_length == 0u) {
        return app->current_pattern;
    }

    return app->pattern_chain[app->pattern_chain_index % app->pattern_chain_length];
}

static uint8_t mk_app_apply_master_gain(const mk_app_t *app, uint8_t gain_0_127) {
    uint32_t scaled = (uint32_t)gain_0_127 * (uint32_t)app->master_level_0_127;
    return (uint8_t)(scaled / 127u);
}

static uint8_t mk_app_clamp_u8(int value, uint8_t min_value, uint8_t max_value) {
    if (value < (int)min_value) {
        return min_value;
    }
    if (value > (int)max_value) {
        return max_value;
    }
    return (uint8_t)value;
}

static uint8_t mk_app_resolve_melodic_slot(const mk_app_t *app, uint8_t sound_index) {
    uint8_t slot = app->sequencer.sounds[sound_index].sample_slot;

    if (slot >= MK_LOADED_MELODIC_SAMPLE_COUNT) {
        slot = (uint8_t)(slot % MK_LOADED_MELODIC_SAMPLE_COUNT);
    }

    if (!app->sample_bank.slots[slot].occupied) {
        slot = (uint8_t)(sound_index % MK_LOADED_MELODIC_SAMPLE_COUNT);
    }

    return slot;
}

static uint8_t mk_app_resolve_drum_slot(const mk_app_t *app, uint8_t sound_index, uint8_t pad_index) {
    uint8_t slot = app->sequencer.sounds[sound_index].sample_slot;

    if (slot < MK_DRUM_SAMPLE_SLOT_BASE || slot >= MK_SAMPLE_SLOT_COUNT) {
        slot = (uint8_t)(MK_DRUM_SAMPLE_SLOT_BASE +
                         ((sound_index - MK_MELODIC_SOUND_COUNT) % MK_LOADED_DRUM_KIT_COUNT) *
                             MK_DRUM_SLICES_PER_KIT);
    }

    slot = (uint8_t)(slot + (pad_index % MK_DRUM_SLICES_PER_KIT));
    if (slot >= MK_SAMPLE_SLOT_COUNT || !app->sample_bank.slots[slot].occupied) {
        slot = (uint8_t)(MK_DRUM_SAMPLE_SLOT_BASE + (pad_index % MK_DRUM_SLICES_PER_KIT));
    }

    return slot;
}

static bool mk_app_build_sound_event(
    const mk_app_t *app,
    uint8_t sound_index,
    uint8_t pad_index,
    mk_step_event_t *out_event
) {
    const mk_sound_channel_t *sound;
    uint8_t sample_slot;

    if (sound_index >= MK_SOUND_CHANNEL_COUNT || pad_index >= MK_STEPS_PER_BAR || out_event == NULL) {
        return false;
    }

    sound = &app->sequencer.sounds[sound_index];
    sample_slot = mk_app_sound_is_drum(sound_index)
                      ? mk_app_resolve_drum_slot(app, sound_index, pad_index)
                      : mk_app_resolve_melodic_slot(app, sound_index);

    if (sample_slot >= MK_SAMPLE_SLOT_COUNT || !app->sample_bank.slots[sample_slot].occupied) {
        return false;
    }

    memset(out_event, 0, sizeof(*out_event));
    out_event->enabled = true;
    out_event->sample_slot = sample_slot;
    out_event->pad_index = pad_index;
    out_event->gain_0_127 = sound->level_0_127;
    out_event->rate = mk_app_sound_is_drum(sound_index)
                          ? 1.0f
                          : (mk_po33_note_rates[pad_index] * mk_pow_semitones(sound->coarse_tune_semitones));
    return true;
}

static void mk_app_trigger_event(mk_app_t *app, const mk_step_event_t *event) {
    if (event == NULL || !event->enabled) {
        return;
    }

    mk_audio_engine_trigger_slot(
        &app->audio,
        &app->sample_bank,
        event->sample_slot,
        event->rate,
        mk_app_apply_master_gain(app, event->gain_0_127)
    );
}

static void mk_app_publish_status(mk_app_t *app) {
    mk_d200_link_publish_status(app);
    app->dirty = false;
}

static void mk_app_preview_pad(mk_app_t *app, uint8_t pad_index) {
    mk_step_event_t preview = {0};
    mk_sound_channel_t *sound = &app->sequencer.sounds[app->selected_sound];

    if (pad_index >= MK_STEPS_PER_BAR) {
        return;
    }

    sound->last_pad_index = pad_index;
    if (!mk_app_build_sound_event(app, app->selected_sound, pad_index, &preview)) {
        return;
    }

    mk_audio_engine_stop_slot(&app->audio, preview.sample_slot);
    mk_app_trigger_event(app, &preview);
    app->dirty = true;
}

static void mk_app_toggle_step_snapshot(mk_app_t *app, uint8_t beat_index) {
    mk_step_event_t event = {0};

    if (beat_index >= MK_STEPS_PER_BAR) {
        return;
    }

    if (!mk_app_build_sound_event(
            app,
            app->selected_sound,
            app->sequencer.sounds[app->selected_sound].last_pad_index,
            &event
        )) {
        return;
    }

    mk_sequencer_toggle_step(
        &app->sequencer,
        app->selected_sound,
        mk_app_active_pattern(app),
        beat_index,
        &event
    );

    printf(
        "[app] sound=%u pattern=%u beat=%u toggle pad=%u slot=%u\n",
        app->selected_sound,
        mk_app_active_pattern(app),
        beat_index,
        event.pad_index,
        event.sample_slot
    );
    app->dirty = true;
}

static void mk_app_append_pattern_to_chain(mk_app_t *app, uint8_t pattern_index) {
    if (pattern_index >= MK_PATTERN_SLOT_COUNT) {
        return;
    }

    if (app->pattern_chain_length < MK_PATTERN_SLOT_COUNT) {
        app->pattern_chain[app->pattern_chain_length++] = pattern_index;
    }

    if (!app->transport.running) {
        app->pattern_chain_index = 0u;
        app->current_pattern = app->pattern_chain[0];
    }

    app->dirty = true;
}

static void mk_app_toggle_play(mk_app_t *app) {
    if (app->transport.running) {
        mk_transport_stop(&app->transport);
        app->pattern_chain_index = 0u;
        app->current_pattern = mk_app_active_pattern(app);
        puts("[app] transport stopped");
    } else {
        app->transport.current_step = 0u;
        app->transport.running = true;
        if (app->pattern_chain_length == 0u) {
            app->pattern_chain[0] = app->current_pattern;
            app->pattern_chain_length = 1u;
        }
        app->pattern_chain_index = 0u;
        app->current_pattern = mk_app_active_pattern(app);
        puts("[app] transport started");
    }
    app->dirty = true;
}

static void mk_app_toggle_live_record(mk_app_t *app) {
    if (app->record_state == MK_RECORD_ACTIVE) {
        app->record_state = MK_RECORD_IDLE;
        app->view = MK_VIEW_PERFORMANCE;
        mk_transport_stop(&app->transport);
        puts("[app] live record off + transport stopped");
    } else {
        app->record_state = MK_RECORD_ACTIVE;
        app->view = MK_VIEW_LIVE_RECORD;
        app->transport.current_step = 0u;
        app->transport.running = true;
        puts("[app] live record on + transport started");
    }
    app->dirty = true;
}

static void mk_app_set_view_toggle(mk_app_t *app, mk_ui_view_t view) {
    if (app->view == view) {
        app->view = MK_VIEW_PERFORMANCE;
    } else {
        app->view = view;
    }
    app->dirty = true;
}

static void mk_app_cycle_fx_page(mk_app_t *app) {
    if (app->view != MK_VIEW_FX) {
        app->view = MK_VIEW_FX;
    } else {
        app->fx_page = (mk_fx_page_t)((app->fx_page + 1u) % 3u);
    }
    app->dirty = true;
}

static void mk_app_adjust_bpm(mk_app_t *app, int delta) {
    int bpm = (int)app->transport.bpm + delta;
    app->transport.bpm = mk_app_clamp_u8(bpm, 40u, 240u);
    printf("[app] bpm=%u\n", app->transport.bpm);
    app->dirty = true;
}

static void mk_app_adjust_sound_source(mk_app_t *app, int delta) {
    mk_sound_channel_t *sound = &app->sequencer.sounds[app->selected_sound];

    if (sound->is_drum) {
        int kit_index = ((int)(sound->sample_slot - MK_DRUM_SAMPLE_SLOT_BASE) / MK_DRUM_SLICES_PER_KIT) + delta;
        while (kit_index < 0) {
            kit_index += MK_LOADED_DRUM_KIT_COUNT;
        }
        kit_index %= MK_LOADED_DRUM_KIT_COUNT;
        sound->sample_slot = (uint8_t)(MK_DRUM_SAMPLE_SLOT_BASE + kit_index * MK_DRUM_SLICES_PER_KIT);
    } else {
        int melodic_index = (int)sound->sample_slot + delta;
        while (melodic_index < 0) {
            melodic_index += MK_LOADED_MELODIC_SAMPLE_COUNT;
        }
        melodic_index %= MK_LOADED_MELODIC_SAMPLE_COUNT;
        sound->sample_slot = (uint8_t)melodic_index;
    }

    printf("[app] sound=%u source_slot=%u\n", app->selected_sound, sound->sample_slot);
    app->dirty = true;
}

static void mk_app_adjust_selected_sound_parameter(mk_app_t *app, int delta) {
    mk_sound_channel_t *sound = &app->sequencer.sounds[app->selected_sound];

    switch (app->fx_page) {
        case MK_FX_PAGE_PITCH:
            sound->coarse_tune_semitones = (int8_t)mk_app_clamp_u8(
                (int)sound->coarse_tune_semitones + delta + 24,
                0u,
                48u
            ) - 24;
            printf("[app] sound=%u tune=%d\n", app->selected_sound, sound->coarse_tune_semitones);
            break;
        case MK_FX_PAGE_VOLUME:
            sound->level_0_127 = mk_app_clamp_u8((int)sound->level_0_127 + (delta * 8), 0u, 127u);
            printf("[app] sound=%u level=%u\n", app->selected_sound, sound->level_0_127);
            break;
        case MK_FX_PAGE_TRIM:
            mk_app_adjust_sound_source(app, delta);
            return;
    }

    app->dirty = true;
}

/* ─── 옥타브 오프셋 (-2 ~ +2) ─────────────────────────────── */
static int8_t g_octave = 0;

/*
 * 피아노 키 → 오디오 트리거
 *  semitone: 0=C … 11=B
 *  rate = 2^((semitone + octave*12 + coarse_tune) / 12)
 */
static void mk_app_trigger_piano_key(mk_app_t *app, uint8_t semitone) {
    mk_sound_channel_t *sound = &app->sequencer.sounds[app->selected_sound];
    uint8_t slot;

    if (mk_app_sound_is_drum(app->selected_sound)) {
        /* 드럼 사운드: semitone 을 드럼 슬라이스 오프셋으로 사용 */
        slot = mk_app_resolve_drum_slot(app, app->selected_sound, semitone % MK_DRUM_SLICES_PER_KIT);
    } else {
        slot = mk_app_resolve_melodic_slot(app, app->selected_sound);
    }

    if (slot >= MK_SAMPLE_SLOT_COUNT || !app->sample_bank.slots[slot].occupied) {
        return;
    }

    int8_t total_semitones = (int8_t)(semitone + g_octave * 12 + sound->coarse_tune_semitones);
    float  rate = mk_pow_semitones(total_semitones);
    uint8_t gain = mk_app_apply_master_gain(app, sound->level_0_127);

    mk_audio_engine_trigger_slot(&app->audio, &app->sample_bank, slot, rate, gain);

    /* LIVE_RECORD + 트랜스포트 실행 중: 현재 스텝에 기록 */
    if (app->view == MK_VIEW_LIVE_RECORD && app->transport.running) {
        mk_step_event_t rec_event = {
            .enabled     = true,
            .sample_slot = slot,
            .pad_index   = semitone,
            .gain_0_127  = sound->level_0_127,
            .rate        = rate,
        };
        mk_sequencer_toggle_step(
            &app->sequencer,
            app->selected_sound,
            mk_app_active_pattern(app),
            (uint8_t)app->transport.current_step,
            &rec_event
        );
    }

    sound->last_pad_index = semitone;
    app->dirty = true;
}

/* ─── ENC1: 마스터 볼륨 ───────────────────────────────────── */
static void mk_app_enc1_turn(mk_app_t *app, int delta) {
    app->master_level_0_127 = mk_app_clamp_u8(
        (int)app->master_level_0_127 + delta * 4, 0u, 127u);
    printf("[app] master_vol=%u\n", app->master_level_0_127);
    app->dirty = true;
}

/* ─── ENC2: 피치 / 샘플 선택 ─────────────────────────────── */
static void mk_app_enc2_turn(mk_app_t *app, int delta) {
    if (app->view == MK_VIEW_SAMPLE_SELECT) {
        if (app->sample_count == 0) return;
        int next = (int)app->sample_cursor + delta;
        if (next < 0) next = 0;
        if (next >= app->sample_count) next = app->sample_count - 1;
        app->sample_cursor = (uint8_t)next;
        printf("[app] sample_cursor=%u %s\n", app->sample_cursor,
               app->sample_names[app->sample_cursor]);
        mk_storage_hal_load_sample_by_name(app, app->sample_names[app->sample_cursor]);
        app->dirty = true;
    } else if (app->view == MK_VIEW_FX) {
        mk_app_adjust_selected_sound_parameter(app, delta);
    } else {
        mk_sound_channel_t *snd = &app->sequencer.sounds[app->selected_sound];
        snd->coarse_tune_semitones = (int8_t)mk_app_clamp_u8(
            (int)snd->coarse_tune_semitones + delta + 24, 0u, 48u) - 24;
        printf("[app] sound=%u tune=%d\n", app->selected_sound, snd->coarse_tune_semitones);
        app->dirty = true;
    }
}

/* ─── ENC3: 옥타브 ───────────────────────────────────────── */
static void mk_app_enc3_turn(mk_app_t *app, int delta) {
    g_octave = (int8_t)(g_octave + delta);
    if (g_octave > 2)  g_octave = 2;
    if (g_octave < -2) g_octave = -2;
    printf("[app] octave=%d\n", g_octave);
    app->dirty = true;
}

/* ─── 뷰 모드 순환: PERF→SOUND→PATTERN→EDIT→PERF ─────────── */
static void mk_app_cycle_mode(mk_app_t *app) {
    switch (app->view) {
        case MK_VIEW_PERFORMANCE:    app->view = MK_VIEW_SOUND_SELECT;   break;
        case MK_VIEW_SOUND_SELECT:   app->view = MK_VIEW_PATTERN_SELECT; break;
        case MK_VIEW_PATTERN_SELECT: app->view = MK_VIEW_EDIT;           break;
        default:                     app->view = MK_VIEW_PERFORMANCE;    break;
    }
    printf("[app] mode=%u\n", (unsigned)app->view);
    app->dirty = true;
}

static void mk_app_trigger_current_step(mk_app_t *app) {
    mk_step_event_t triggered_events[MK_SOUND_CHANNEL_COUNT];
    size_t trigger_count = mk_sequencer_collect_step_triggers(
        &app->sequencer,
        mk_app_active_pattern(app),
        (uint8_t)app->transport.current_step,
        triggered_events,
        MK_SOUND_CHANNEL_COUNT
    );

    for (size_t i = 0; i < trigger_count; ++i) {
        mk_app_trigger_event(app, &triggered_events[i]);
    }
}

static void mk_app_advance_pattern_chain_if_needed(mk_app_t *app) {
    if (app->transport.current_step != 0u || app->pattern_chain_length == 0u) {
        return;
    }

    app->pattern_chain_index = (uint8_t)((app->pattern_chain_index + 1u) % app->pattern_chain_length);
    app->current_pattern = mk_app_active_pattern(app);
}

void mk_app_init(mk_app_t *app) {
    memset(app, 0, sizeof(*app));

    mk_transport_init(&app->transport, 120, 1);
    mk_sample_bank_init(&app->sample_bank);
    mk_sequencer_init(&app->sequencer);
    mk_audio_engine_init(&app->audio);

    app->view = MK_VIEW_PERFORMANCE;
    app->fx_page = MK_FX_PAGE_PITCH;
    app->record_state = MK_RECORD_IDLE;
    app->selected_sound = 0u;
    app->current_pattern = 0u;
    app->pattern_chain[0] = 0u;
    app->pattern_chain_length = 1u;
    app->pattern_chain_index = 0u;
    app->master_level_0_127 = 127u;
    app->dirty = true;

    mk_storage_hal_init();
    mk_storage_hal_load_project(app);
    mk_d200_link_init();

    mk_audio_hal_config_t audio_config = {
        .sample_rate_hz = MK_DEFAULT_AUDIO_SAMPLE_RATE,
        .block_frames = MK_AUDIO_BLOCK_FRAMES,
        .channels = MK_AUDIO_CHANNELS,
    };
    mk_audio_hal_init(&audio_config);

    mk_app_publish_status(app);
}

void mk_app_handle_button_event(mk_app_t *app, mk_button_event_t event) {
    /* ── 엔코더 회전: PRESS 여부 무관하게 처리 ── */
    if (event.type == MK_BUTTON_EVENT_ENC_CW ||
        event.type == MK_BUTTON_EVENT_ENC_CCW) {
        int delta = (event.type == MK_BUTTON_EVENT_ENC_CW) ? 1 : -1;
        switch (event.id) {
            case MK_ENC1: mk_app_enc1_turn(app, delta); break;
            case MK_ENC2: mk_app_enc2_turn(app, delta); break;
            case MK_ENC3: mk_app_enc3_turn(app, delta); break;
            default: break;
        }
        return;
    }

    /* ── 이하 PRESS 이벤트만 처리 ── */
    if (event.type != MK_BUTTON_EVENT_PRESS) {
        return;
    }

    /* 피아노 건반 */
    if (mk_button_is_key(event.id)) {
        mk_app_trigger_piano_key(app, event.id);  /* id == semitone (0-11) */
        return;
    }

    /* 기능 버튼 */
    switch (event.id) {
        case MK_BTN_PLAY:
            mk_app_toggle_play(app);
            break;

        case MK_BTN_REC:
            mk_app_toggle_live_record(app);
            break;

        case MK_BTN_MODE:
            mk_app_cycle_mode(app);
            break;

        /* 엔코더 푸시 */
        case MK_ENC1_SW:
            /* 탭 템포 — 현재는 BPM 뷰 토글 */
            mk_app_set_view_toggle(app, MK_VIEW_BPM);
            break;

        case MK_ENC2_SW:
            if (app->view == MK_VIEW_SAMPLE_SELECT) {
                /* 선택 확정 → 로드 후 PERF 복귀 */
                if (app->sample_count > 0) {
                    mk_storage_hal_load_sample_by_name(app, app->sample_names[app->sample_cursor]);
                }
                app->view = MK_VIEW_PERFORMANCE;
                app->dirty = true;
            } else {
                /* 샘플 선택 뷰 진입: 파일 목록 열거 */
                app->sample_count = mk_storage_hal_list_samples(app->sample_names, 64);
                app->sample_cursor = 0;
                app->view = MK_VIEW_SAMPLE_SELECT;
                printf("[app] sample_select: %u files\n", app->sample_count);
                app->dirty = true;
            }
            break;

        case MK_ENC3_SW:
            /* 옥타브 리셋 */
            g_octave = 0;
            printf("[app] octave reset\n");
            app->dirty = true;
            break;

        default:
            break;
    }
}

void mk_app_tick(mk_app_t *app) {
    mk_button_event_t event = {0};

    if (mk_d200_link_poll_button_event(&event)) {
        mk_app_handle_button_event(app, event);
    }

    if (app->dirty) {
        mk_app_publish_status(app);
    }

    mk_d200_link_update_leds(app);
}

void mk_app_step_transport(mk_app_t *app) {
    if (!app->transport.running) {
        return;
    }

    mk_app_trigger_current_step(app);
    mk_transport_advance(&app->transport);
    mk_app_advance_pattern_chain_if_needed(app);
    app->dirty = true;

    if (app->dirty) {
        mk_app_publish_status(app);
    }
}

void mk_app_render_audio(mk_app_t *app, int16_t *interleaved_frames, size_t frame_count) {
    mk_audio_engine_render(&app->audio, &app->sample_bank, interleaved_frames, frame_count);
}
