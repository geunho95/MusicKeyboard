#pragma once

#include <stdint.h>
#include "music_keyboard/types.h"   /* mk_button_event_type_t, mk_button_event_t */

/*
 * 버튼 레이아웃
 *
 * [4×4 매트릭스] — 15개 사용, 1칸 미사용
 *
 *          COL0    COL1    COL2    COL3
 * ROW0  [  C  ]  [ C#  ]  [  D  ]  [ D# ]   id: 0~ 3
 * ROW1  [  E  ]  [  F  ]  [ F#  ]  [  G ]   id: 4~ 7
 * ROW2  [ G#  ]  [  A  ]  [ A#  ]  [  B ]   id: 8~11
 * ROW3  [ FN1 ]  [ FN2 ]  [ FN3 ]  [ -- ]   id:12~14, 15=미사용
 *
 * [엔코더 푸시 — 직결 GPIO]
 *   ENC1_SW  id:16   탭 템포
 *   ENC2_SW  id:17   사운드/FX 선택
 *   ENC3_SW  id:18   옥타브 리셋
 *
 * [엔코더 회전 — ENC_CW / ENC_CCW 이벤트]
 *   ENC1     id:19   선택/BPM
 *   ENC2     id:20   피치/볼륨
 *   ENC3     id:21   옥타브
 *
 * button_id = row × 4 + col  (매트릭스)
 */

typedef enum {
    /* ── 피아노 건반 (12키, 1옥타브 크로매틱) ── */
    MK_KEY_C   = 0,
    MK_KEY_CS  = 1,   /* C# */
    MK_KEY_D   = 2,
    MK_KEY_DS  = 3,   /* D# */
    MK_KEY_E   = 4,
    MK_KEY_F   = 5,
    MK_KEY_FS  = 6,   /* F# */
    MK_KEY_G   = 7,
    MK_KEY_GS  = 8,   /* G# */
    MK_KEY_A   = 9,
    MK_KEY_AS  = 10,  /* A# */
    MK_KEY_B   = 11,

    /* ── 기능 버튼 (3개) ── */
    MK_BTN_PLAY = 12,  /* FN1: 재생 / 정지 */
    MK_BTN_REC  = 13,  /* FN2: 녹음 on/off */
    MK_BTN_MODE = 14,  /* FN3: 모드 순환 (PERF→SOUND→PATTERN→EDIT) */

    /* 매트릭스 미사용 슬롯 */
    MK_BTN_UNUSED = 15,

    /* ── 엔코더 푸시 버튼 (직결 GPIO) ── */
    MK_ENC1_SW  = 16,  /* 탭 템포 */
    MK_ENC2_SW  = 17,  /* 사운드/FX 선택 */
    MK_ENC3_SW  = 18,  /* 옥타브 리셋 */

    /* ── 엔코더 회전 (ENC_CW / ENC_CCW 이벤트에서 사용) ── */
    MK_ENC1     = 19,  /* BPM / 선택 */
    MK_ENC2     = 20,  /* 피치 / 볼륨 */
    MK_ENC3     = 21,  /* 옥타브 */

    MK_BUTTON_COUNT = 22,
} mk_button_id_t;

/*
 * 피아노 키 → 반음 인덱스 (0=C, 1=C#, ..., 11=B)
 * 건반이 아닌 버튼이면 -1 반환
 */
static inline int8_t mk_button_id_to_semitone(uint8_t button_id) {
    if (button_id <= MK_KEY_B) {
        return (int8_t)button_id;  /* 0~11 직접 대응 */
    }
    return -1;
}

/*
 * 피아노 키 여부
 */
static inline int mk_button_is_key(uint8_t button_id) {
    return button_id <= MK_KEY_B;
}
