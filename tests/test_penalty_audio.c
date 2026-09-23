#include "penalty_audio.h"

#include <assert.h>
#include <stddef.h>

static penalty_model_t model(penalty_state_t state, uint64_t since) {
    penalty_model_t value = {0};
    value.state = state;
    value.since_ms = since;
    return value;
}

static void test_cue_vocabulary(void) {
    penalty_sfx_cue_t cue;
    assert(penalty_audio_cue(PENALTY_SFX_KICK, &cue));
    assert(cue.priority == PENALTY_SFX_PRIORITY_ACTION);
    assert(cue.segment_count == 1);
    assert(cue.segments[0].kind == PENALTY_SFX_SEGMENT_KICK_PCM);
    assert(cue.stale_ms == 250);

    assert(penalty_audio_cue(PENALTY_SFX_PERFECT, &cue));
    assert(cue.priority == PENALTY_SFX_PRIORITY_RESULT);
    assert(cue.segment_count >= 3);
    assert(cue.segments[0].kind == PENALTY_SFX_SEGMENT_GOAL_PCM);

    assert(penalty_audio_cue(PENALTY_SFX_SOUND_OFF, &cue));
    assert(cue.audible_when_muted);
    assert(cue.priority == PENALTY_SFX_PRIORITY_ACTION);
    assert(!penalty_audio_cue(PENALTY_SFX_NONE, &cue));
}

static void test_kick_is_aligned_to_contact(void) {
    penalty_audio_tracker_t tracker = {0};
    penalty_model_t before = model(PENALTY_CHARGE, 900);
    penalty_model_t flight = model(PENALTY_FLIGHT, 1000);
    penalty_sfx_event_t events[3];

    assert(penalty_audio_events(&tracker, &before, &flight, 1000, events, 3) == 0);
    before = flight;
    assert(penalty_audio_events(&tracker, &before, &flight, 1099, events, 3) == 0);
    assert(penalty_audio_events(&tracker, &before, &flight, 1100, events, 3) == 1);
    assert(events[0] == PENALTY_SFX_KICK);
    assert(penalty_audio_events(&tracker, &before, &flight, 1150, events, 3) == 0);
}

static void test_timeout_has_result_but_no_kick(void) {
    penalty_audio_tracker_t tracker = {0};
    penalty_model_t before = model(PENALTY_CHARGE, 1000);
    penalty_model_t after = model(PENALTY_RESULT, 5000);
    penalty_sfx_event_t events[3];
    before.completed = 0;
    after.completed = 1;
    after.current.outcome = PENALTY_TIMEOUT;

    size_t count = penalty_audio_events(&tracker, &before, &after, 5000, events, 3);
    assert(count == 1);
    assert(events[0] == PENALTY_SFX_MISS);
}

static void test_result_families(void) {
    struct {
        penalty_outcome_t outcome;
        penalty_sfx_event_t event;
    } cases[] = {
        {PENALTY_GOAL, PENALTY_SFX_GOAL},
        {PENALTY_GREEN_GOAL, PENALTY_SFX_GOAL},
        {PENALTY_PERFECT, PENALTY_SFX_PERFECT},
        {PENALTY_SAVE, PENALTY_SFX_SAVE},
        {PENALTY_WEAK, PENALTY_SFX_SAVE},
        {PENALTY_GREEN_SAVE, PENALTY_SFX_SAVE},
        {PENALTY_PERFECT_SAVE, PENALTY_SFX_SAVE},
        {PENALTY_HIGH, PENALTY_SFX_MISS},
        {PENALTY_TIMEOUT, PENALTY_SFX_MISS},
        {PENALTY_KEEPER_CATCH, PENALTY_SFX_SAVE},
        {PENALTY_KEEPER_PARRY, PENALTY_SFX_SAVE},
        {PENALTY_KEEPER_WRONG_WAY, PENALTY_SFX_GOAL},
        {PENALTY_KEEPER_READ_MISS, PENALTY_SFX_GOAL},
        {PENALTY_KEEPER_EARLY, PENALTY_SFX_GOAL},
        {PENALTY_KEEPER_LATE, PENALTY_SFX_GOAL},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        penalty_audio_tracker_t tracker = {0};
        penalty_model_t before = model(PENALTY_FLIGHT, 1000);
        penalty_model_t after = model(PENALTY_RESULT, 1600);
        penalty_sfx_event_t events[3];
        before.completed = 0;
        after.completed = 1;
        after.current.outcome = cases[i].outcome;
        assert(penalty_audio_events(&tracker, &before, &after, 1600, events, 3) == 1);
        assert(events[0] == cases[i].event);
    }
}

static void test_ui_and_full_time_events(void) {
    penalty_audio_tracker_t tracker = {0};
    penalty_model_t before = model(PENALTY_TITLE, 10);
    penalty_model_t after = before;
    penalty_sfx_event_t events[3];

    after.difficulty = PENALTY_HARD;
    assert(penalty_audio_events(&tracker, &before, &after, 20, events, 3) == 1);
    assert(events[0] == PENALTY_SFX_DIFFICULTY_HARD);

    before = after;
    after.muted = true;
    assert(penalty_audio_events(&tracker, &before, &after, 30, events, 3) == 1);
    assert(events[0] == PENALTY_SFX_SOUND_OFF);

    before = model(PENALTY_TITLE, 40);
    after = model(PENALTY_SETTINGS, 50);
    assert(penalty_audio_events(&tracker, &before, &after, 50, events, 3) == 1);
    assert(events[0] == PENALTY_SFX_UI_CONFIRM);

    before = after;
    after.language = PENALTY_LANGUAGE_ZH_CN;
    assert(penalty_audio_events(&tracker, &before, &after, 60, events, 3) == 1);
    assert(events[0] == PENALTY_SFX_UI_CONFIRM);

    before = model(PENALTY_RESULT, 100);
    after = model(PENALTY_SUMMARY, 1000);
    after.goals = 5;
    assert(penalty_audio_events(&tracker, &before, &after, 1000, events, 3) == 1);
    assert(events[0] == PENALTY_SFX_FLAWLESS);

    before = model(PENALTY_RESULT, 100);
    after = model(PENALTY_SUMMARY, 1000);
    after.mode = PENALTY_MODE_KEEPER;
    after.saves = 5;
    assert(penalty_audio_events(&tracker, &before, &after, 1000, events, 3) == 1);
    assert(events[0] == PENALTY_SFX_FLAWLESS);
}

int main(void) {
    test_cue_vocabulary();
    test_kick_is_aligned_to_contact();
    test_timeout_has_result_but_no_kick();
    test_result_families();
    test_ui_and_full_time_events();
    return 0;
}
