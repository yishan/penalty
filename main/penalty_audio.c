#include "penalty_audio.h"

#include <string.h>

static void append(penalty_sfx_event_t event, penalty_sfx_event_t *events,
                   size_t capacity, size_t *count) {
    if (event == PENALTY_SFX_NONE) return;
    if (*count < capacity) events[*count] = event;
    if (*count < capacity) ++*count;
}

static penalty_sfx_event_t result_event(penalty_outcome_t outcome) {
    switch (outcome) {
    case PENALTY_GOAL:
    case PENALTY_GREEN_GOAL:
        return PENALTY_SFX_GOAL;
    case PENALTY_PERFECT:
        return PENALTY_SFX_PERFECT;
    case PENALTY_SAVE:
    case PENALTY_WEAK:
    case PENALTY_GREEN_SAVE:
    case PENALTY_PERFECT_SAVE:
    case PENALTY_KEEPER_CATCH:
    case PENALTY_KEEPER_PARRY:
        return PENALTY_SFX_SAVE;
    case PENALTY_HIGH:
    case PENALTY_TIMEOUT:
        return PENALTY_SFX_MISS;
    case PENALTY_KEEPER_WRONG_WAY:
    case PENALTY_KEEPER_READ_MISS:
    case PENALTY_KEEPER_EARLY:
    case PENALTY_KEEPER_LATE:
        return PENALTY_SFX_GOAL;
    default:
        return PENALTY_SFX_NONE;
    }
}

static penalty_sfx_event_t difficulty_event(penalty_difficulty_t difficulty) {
    static const penalty_sfx_event_t events[] = {
        PENALTY_SFX_DIFFICULTY_EASY,
        PENALTY_SFX_DIFFICULTY_NORMAL,
        PENALTY_SFX_DIFFICULTY_HARD,
    };
    return (unsigned)difficulty < sizeof(events) / sizeof(events[0])
        ? events[difficulty] : PENALTY_SFX_NONE;
}

static bool is_confirm_transition(penalty_state_t before, penalty_state_t after) {
    return (before == PENALTY_COVER && after == PENALTY_TITLE) ||
           (before == PENALTY_TITLE &&
            (after == PENALTY_SETTINGS || after == PENALTY_AIM || after == PENALTY_HELP)) ||
           (before == PENALTY_SETTINGS && after == PENALTY_TITLE) ||
           (before == PENALTY_HELP && after == PENALTY_TITLE) ||
           (before == PENALTY_SUMMARY && (after == PENALTY_AIM || after == PENALTY_TITLE));
}

size_t penalty_audio_events(penalty_audio_tracker_t *tracker,
                            const penalty_model_t *before,
                            const penalty_model_t *after,
                            uint64_t now,
                            penalty_sfx_event_t *events,
                            size_t capacity) {
    if (!tracker || !before || !after || !events || capacity == 0) return 0;
    size_t count = 0;

    if (before->difficulty != after->difficulty)
        append(difficulty_event(after->difficulty), events, capacity, &count);
    if (before->muted != after->muted)
        append(after->muted ? PENALTY_SFX_SOUND_OFF : PENALTY_SFX_SOUND_ON,
               events, capacity, &count);
    if (before->language != after->language)
        append(PENALTY_SFX_UI_CONFIRM, events, capacity, &count);

    if (before->state != after->state && after->state == PENALTY_FLIGHT)
        tracker->kick_emitted = false;

    if (before->completed != after->completed)
        append(result_event(after->current.outcome), events, capacity, &count);
    else if (before->state == PENALTY_RESULT && after->state == PENALTY_SUMMARY)
        append((after->mode == PENALTY_MODE_KEEPER ? after->saves : after->goals) == PENALTY_SHOTS
                   ? PENALTY_SFX_FLAWLESS : PENALTY_SFX_FULL_TIME,
               events, capacity, &count);
    else if (before->state == PENALTY_AIM && after->state == PENALTY_CHARGE)
        append(PENALTY_SFX_CHARGE, events, capacity, &count);
    else if (is_confirm_transition(before->state, after->state))
        append(PENALTY_SFX_UI_CONFIRM, events, capacity, &count);

    if (after->state == PENALTY_FLIGHT && !tracker->kick_emitted && now >= after->since_ms) {
        uint64_t elapsed = now - after->since_ms;
        if (elapsed >= PENALTY_AUDIO_CONTACT_MS) {
            /* A late render must not produce a detached kick long after contact. */
            if (elapsed <= PENALTY_AUDIO_CONTACT_MS + 250)
                append(PENALTY_SFX_KICK, events, capacity, &count);
            tracker->kick_emitted = true;
        }
    }
    return count;
}

static void segment(penalty_sfx_cue_t *cue, penalty_sfx_segment_kind_t kind,
                    uint16_t hz, uint16_t ms) {
    if (cue->segment_count >= PENALTY_SFX_MAX_SEGMENTS) return;
    cue->segments[cue->segment_count++] = (penalty_sfx_segment_t){kind, hz, ms};
}

bool penalty_audio_cue(penalty_sfx_event_t event, penalty_sfx_cue_t *cue) {
    if (!cue || event == PENALTY_SFX_NONE) return false;
    memset(cue, 0, sizeof(*cue));
    cue->priority = PENALTY_SFX_PRIORITY_UI;
    cue->stale_ms = 300;

    switch (event) {
    case PENALTY_SFX_UI_CONFIRM:
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 720, 35);
        break;
    case PENALTY_SFX_DIFFICULTY_EASY:
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 520, 55);
        break;
    case PENALTY_SFX_DIFFICULTY_NORMAL:
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 660, 55);
        break;
    case PENALTY_SFX_DIFFICULTY_HARD:
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 820, 55);
        break;
    case PENALTY_SFX_SOUND_ON:
        cue->priority = PENALTY_SFX_PRIORITY_ACTION;
        cue->audible_when_muted = true;
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 600, 45);
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 900, 65);
        break;
    case PENALTY_SFX_SOUND_OFF:
        cue->priority = PENALTY_SFX_PRIORITY_ACTION;
        cue->audible_when_muted = true;
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 800, 40);
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 420, 70);
        break;
    case PENALTY_SFX_CHARGE:
        cue->priority = PENALTY_SFX_PRIORITY_ACTION;
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 260, 45);
        break;
    case PENALTY_SFX_KICK:
        cue->priority = PENALTY_SFX_PRIORITY_ACTION;
        cue->stale_ms = 250;
        segment(cue, PENALTY_SFX_SEGMENT_KICK_PCM, 0, 80);
        break;
    case PENALTY_SFX_GOAL:
        cue->priority = PENALTY_SFX_PRIORITY_RESULT;
        cue->stale_ms = 500;
        segment(cue, PENALTY_SFX_SEGMENT_GOAL_PCM, 0, 110);
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 660, 65);
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 880, 100);
        break;
    case PENALTY_SFX_PERFECT:
        cue->priority = PENALTY_SFX_PRIORITY_RESULT;
        cue->stale_ms = 500;
        segment(cue, PENALTY_SFX_SEGMENT_GOAL_PCM, 0, 110);
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 660, 55);
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 880, 65);
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 1100, 100);
        break;
    case PENALTY_SFX_SAVE:
        cue->priority = PENALTY_SFX_PRIORITY_RESULT;
        cue->stale_ms = 500;
        segment(cue, PENALTY_SFX_SEGMENT_SAVE_PCM, 0, 100);
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 260, 85);
        break;
    case PENALTY_SFX_MISS:
        cue->priority = PENALTY_SFX_PRIORITY_RESULT;
        cue->stale_ms = 500;
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 480, 70);
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 240, 110);
        break;
    case PENALTY_SFX_FULL_TIME:
        cue->priority = PENALTY_SFX_PRIORITY_RESULT;
        cue->stale_ms = 700;
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 520, 80);
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 390, 80);
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 520, 120);
        break;
    case PENALTY_SFX_FLAWLESS:
        cue->priority = PENALTY_SFX_PRIORITY_RESULT;
        cue->stale_ms = 700;
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 660, 70);
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 880, 70);
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 1100, 80);
        segment(cue, PENALTY_SFX_SEGMENT_TONE, 1320, 120);
        break;
    default:
        return false;
    }
    return cue->segment_count != 0;
}
