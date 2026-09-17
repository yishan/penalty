#pragma once

#include "penalty_model.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PENALTY_AUDIO_VOLUME_PERCENT 60
#define PENALTY_AUDIO_CONTACT_MS 100
#define PENALTY_SFX_MAX_SEGMENTS 4

typedef enum {
    PENALTY_SFX_NONE = 0,
    PENALTY_SFX_UI_CONFIRM,
    PENALTY_SFX_DIFFICULTY_EASY,
    PENALTY_SFX_DIFFICULTY_NORMAL,
    PENALTY_SFX_DIFFICULTY_HARD,
    PENALTY_SFX_SOUND_ON,
    PENALTY_SFX_SOUND_OFF,
    PENALTY_SFX_CHARGE,
    PENALTY_SFX_KICK,
    PENALTY_SFX_GOAL,
    PENALTY_SFX_PERFECT,
    PENALTY_SFX_SAVE,
    PENALTY_SFX_MISS,
    PENALTY_SFX_FULL_TIME,
    PENALTY_SFX_FLAWLESS,
} penalty_sfx_event_t;

typedef enum {
    PENALTY_SFX_PRIORITY_UI = 1,
    PENALTY_SFX_PRIORITY_ACTION,
    PENALTY_SFX_PRIORITY_RESULT,
} penalty_sfx_priority_t;

typedef enum {
    PENALTY_SFX_SEGMENT_TONE,
    PENALTY_SFX_SEGMENT_SILENCE,
    PENALTY_SFX_SEGMENT_KICK_PCM,
    PENALTY_SFX_SEGMENT_SAVE_PCM,
    PENALTY_SFX_SEGMENT_GOAL_PCM,
} penalty_sfx_segment_kind_t;

typedef struct {
    penalty_sfx_segment_kind_t kind;
    uint16_t hz;
    uint16_t ms;
} penalty_sfx_segment_t;

typedef struct {
    penalty_sfx_priority_t priority;
    uint16_t stale_ms;
    bool audible_when_muted;
    uint8_t segment_count;
    penalty_sfx_segment_t segments[PENALTY_SFX_MAX_SEGMENTS];
} penalty_sfx_cue_t;

typedef struct {
    bool kick_emitted;
} penalty_audio_tracker_t;

/* Converts accepted model changes to semantic audio events. The model remains
 * audio-independent, and visual contact is the sole kick timing authority. */
size_t penalty_audio_events(penalty_audio_tracker_t *tracker,
                            const penalty_model_t *before,
                            const penalty_model_t *after,
                            uint64_t now_ms,
                            penalty_sfx_event_t *events,
                            size_t capacity);

/* Maps a semantic event to a short tone/PCM sequence. */
bool penalty_audio_cue(penalty_sfx_event_t event, penalty_sfx_cue_t *cue);
