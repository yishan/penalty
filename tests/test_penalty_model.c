#include "penalty_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void begin(penalty_model_t *m) {
    penalty_model_init(m, 42, 0);
    penalty_model_input(m, PENALTY_INPUT_OK, 1);
    assert(m->state == PENALTY_TITLE);
    penalty_model_input(m, PENALTY_INPUT_OK, 1); /* duplicate entry */
    assert(m->state == PENALTY_TITLE);
    penalty_model_input(m, PENALTY_INPUT_OK, 2);
    assert(m->state == PENALTY_AIM);
}

static uint64_t perfect_time(const penalty_shot_t *s) {
    return s->green_low * 8u + (s->perfect_low - s->green_low) * 40u + 20u;
}

static unsigned target_column(penalty_direction_t target) {
    static const unsigned columns[PENALTY_TARGET_COUNT] = {0, 1, 2, 2, 1, 0};
    assert((unsigned)target < PENALTY_TARGET_COUNT);
    return columns[target];
}

static unsigned goal_chance(const penalty_shot_t *s) {
    bool exact = s->direction == s->keeper;
    bool same_column = target_column(s->direction) == target_column(s->keeper);
    bool perfect = s->power >= s->perfect_low && s->power <= s->perfect_low + 1;
    bool green = s->power >= s->green_low && s->power <= s->green_high;
    if (perfect) return exact ? 75 : same_column ? 85 : 95;
    if (green) return exact ? 30 : same_column ? 40 : 80;
    return exact ? 0 : same_column ? 10 : 35;
}

static void boundaries(void) {
    for (unsigned d = 0; d < PENALTY_TARGET_COUNT; ++d) {
        for (unsigned k = 0; k < PENALTY_TARGET_COUNT; ++k) {
            for (unsigned p = 0; p <= 100; ++p) {
              for (unsigned roll = 0; roll < 100; ++roll) {
                penalty_shot_t s = {.direction=d, .keeper=k, .power=p,
                    .green_low=61, .green_high=80, .perfect_low=73, .outcome_roll=roll};
                penalty_outcome_t expected = p < 30 ? PENALTY_WEAK : p > 90 ? PENALTY_HIGH :
                    p >= 73 && p <= 74 ? (roll < goal_chance(&s) ? PENALTY_PERFECT : PENALTY_PERFECT_SAVE) :
                    p >= 61 && p <= 80 ? (roll < goal_chance(&s) ? PENALTY_GREEN_GOAL : PENALTY_GREEN_SAVE) :
                    roll < goal_chance(&s) ? PENALTY_GOAL : PENALTY_SAVE;
                assert(penalty_judge(&s) == expected);
              }
            }
        }
    }
    assert(penalty_is_goal(PENALTY_GREEN_GOAL));
    assert(!penalty_is_goal(PENALTY_GREEN_SAVE));
    assert(!penalty_is_goal(PENALTY_PERFECT_SAVE));
    assert(strcmp(penalty_outcome_text(PENALTY_GREEN_SAVE), "GREEN - SAVE") == 0);
}

static void calibrated_average_goal_rates(void) {
    for (unsigned direction = 0; direction < PENALTY_TARGET_COUNT; ++direction) {
        unsigned perfect_goals = 0, green_goals = 0, outside_goals = 0;
        for (unsigned keeper = 0; keeper < PENALTY_TARGET_COUNT; ++keeper) {
            for (unsigned roll = 0; roll < 100; ++roll) {
                penalty_shot_t shot = {
                    .direction = direction, .keeper = keeper,
                    .green_low = 60, .green_high = 75, .perfect_low = 68,
                    .outcome_roll = roll,
                };
                shot.power = 68;
                perfect_goals += penalty_is_goal(penalty_judge(&shot));
                shot.power = 65;
                green_goals += penalty_is_goal(penalty_judge(&shot));
                shot.power = 50;
                outside_goals += penalty_is_goal(penalty_judge(&shot));
            }
        }
        assert(perfect_goals == 540);
        assert(green_goals == 390);
        assert(outside_goals == 150);
    }
}

static void meter_windows(void) {
    const unsigned widths[] = {20, 11, 6};
    for (unsigned d = 0; d < 3; ++d) {
      for (unsigned low = 30; low + widths[d] - 1 <= 90; ++low) {
        penalty_shot_t s = {.green_low=low, .green_high=low+widths[d]-1};
        unsigned half = 800 + widths[d] * 32;
        assert(penalty_meter_half_ms(&s) == half);
        assert(penalty_power_at(&s, half) == 100);
        assert(penalty_power_at(&s, 2 * half) == 0);
        unsigned up[101] = {0}, down[101] = {0};
        for (unsigned t = 0; t < half; ++t) {
            unsigned p = penalty_power_at(&s, t);
            ++up[p];
            ++down[penalty_power_at(&s, 2 * half - t)];
            assert(penalty_power_at(&s, t + 1000000ULL * 2 * half) == p);
            assert(penalty_power_at(&s, t + 1) >= p);
        }
        for (unsigned p = 0; p < 100; ++p) {
            unsigned duration = p >= low && p <= s.green_high ? 40 : 8;
            assert(up[p] == duration && down[p] == duration);
        }
        for (unsigned line = low; line < s.green_high; ++line) {
            s.perfect_low = line;
            assert(up[line] + up[line + 1] == 80);
            assert(penalty_power_at(&s, perfect_time(&s)) == line);
            assert(perfect_time(&s) < PENALTY_TIMEOUT_MS);
            for (unsigned p = 0; p <= 100; ++p) {
                s.power = p;
                penalty_outcome_t outcome = penalty_judge(&s);
                bool on_line = p >= line && p <= line + 1;
                assert((outcome == PENALTY_PERFECT || outcome == PENALTY_PERFECT_SAVE) == on_line);
            }
        }
      }
    }
}

static void session(void) {
    penalty_model_t m;
    begin(&m);
    uint64_t t = 100;
    for (unsigned shot = 0; shot < PENALTY_SHOTS; ++shot) {
        assert(m.current.direction == PENALTY_BOTTOM_CENTER);
        for (unsigned target = 0; target < PENALTY_TARGET_COUNT; ++target)
            penalty_model_input(&m, PENALTY_INPUT_RIGHT, t++);
        assert(m.current.direction == PENALTY_BOTTOM_CENTER);
        penalty_model_input(&m, PENALTY_INPUT_LEFT, t++);
        assert(m.current.direction == PENALTY_BOTTOM_RIGHT);
        m.current.outcome_roll = 0;
        penalty_model_input(&m, PENALTY_INPUT_OK, t);
        assert(m.state == PENALTY_CHARGE);
        penalty_direction_t keeper = m.current.keeper;
        penalty_model_input(&m, PENALTY_INPUT_LEFT, t + 10);
        assert(m.current.direction == PENALTY_BOTTOM_RIGHT);
        penalty_model_tick(&m, t + 200); /* rendering doesn't set locked power */
        uint64_t fired = t + perfect_time(&m.current);
        penalty_model_tick(&m, fired + 50); /* queued press uses its source timestamp */
        penalty_model_input(&m, PENALTY_INPUT_OK, fired);
        assert(m.state == PENALTY_FLIGHT && m.current.power == m.current.perfect_low);
        assert(m.current.keeper == keeper && m.completed == shot);
        penalty_model_input(&m, PENALTY_INPUT_OK, fired + 40);
        assert(m.state == PENALTY_FLIGHT);
        penalty_model_tick(&m, fired + 599);
        assert(m.completed == shot);
        penalty_model_tick(&m, fired + 600);
        penalty_model_tick(&m, fired + 600);
        assert(m.completed == shot + 1 && m.goals == shot + 1);
        penalty_model_input(&m, PENALTY_INPUT_OK, fired + 640);
        assert(m.state == PENALTY_RESULT);
        unsigned previous = m.current.green_low;
        penalty_model_tick(&m, fired + 1500);
        if (shot + 1 < PENALTY_SHOTS) assert(m.current.green_low != previous);
        t = fired + 1600;
    }
    assert(m.state == PENALTY_SUMMARY && m.perfect == 5);
    penalty_model_tick(&m, t + 99999);
    assert(m.state == PENALTY_SUMMARY && m.completed == 5);
    penalty_model_input(&m, PENALTY_INPUT_OK, t + 100000);
    assert(m.state == PENALTY_AIM && m.completed == 0 && m.goals == 0 && m.perfect == 0);
}

static void timing_and_recovery(void) {
    penalty_model_t m;
    begin(&m);
    penalty_model_input(&m, PENALTY_INPUT_OK, 100);
    penalty_model_tick(&m, 4099);
    assert(m.state == PENALTY_CHARGE);
    penalty_model_input(&m, PENALTY_INPUT_OK, 4100);
    assert(m.state == PENALTY_RESULT && m.current.outcome == PENALTY_TIMEOUT);
    assert(m.completed == 1 && m.goals == 0);
    penalty_model_input(&m, PENALTY_INPUT_OK, 5000);
    assert(m.state == PENALTY_AIM); /* not CHARGE across result boundary */
    penalty_model_input(&m, PENALTY_INPUT_OK, 4900);
    assert(m.state == PENALTY_AIM); /* stale timestamp */
    penalty_model_input(&m, PENALTY_INPUT_OK, 5100);
    penalty_model_resync(&m, 5200);
    assert(m.state == PENALTY_AIM && m.completed == 1);
    penalty_model_input(&m, PENALTY_INPUT_OK, 5300);
    penalty_model_tick(&m, 90000); /* delayed frame: one timeout, visible result */
    assert(m.state == PENALTY_RESULT && m.completed == 2);
    penalty_model_tick(&m, 90900);
    penalty_model_input(&m, PENALTY_INPUT_OK, 91000);
    uint64_t fired = 91000 + perfect_time(&m.current);
    penalty_model_input(&m, PENALTY_INPUT_OK, fired);
    penalty_model_resync(&m, fired + 40);
    assert(m.state == PENALTY_FLIGHT);
    penalty_model_tick(&m, fired + 600);
    assert(m.completed == 3 && m.goals == 1);
}

static void menus_and_exit(void) {
    penalty_model_t m;
    penalty_model_init(&m, 0, 0);
    penalty_model_input(&m, PENALTY_INPUT_OK, 1);
    penalty_model_input(&m, PENALTY_INPUT_RIGHT, 2);
    penalty_model_input(&m, PENALTY_INPUT_OK, 3);
    assert(m.state == PENALTY_SETTINGS && m.selection == 0);
    penalty_model_input(&m, PENALTY_INPUT_OK, 4);
    assert(m.difficulty == PENALTY_NORMAL && !m.muted);
    penalty_model_input(&m, PENALTY_INPUT_OK, 5);
    assert(m.difficulty == PENALTY_HARD);
    penalty_model_input(&m, PENALTY_INPUT_OK, 6);
    assert(m.difficulty == PENALTY_EASY);
    penalty_model_input(&m, PENALTY_INPUT_RIGHT, 7);
    penalty_model_input(&m, PENALTY_INPUT_OK, 8);
    assert(m.muted);
    penalty_model_input(&m, PENALTY_INPUT_RIGHT, 9);
    penalty_model_input(&m, PENALTY_INPUT_OK, 10);
    assert(m.language == PENALTY_LANGUAGE_ZH_CN);
    penalty_model_input(&m, PENALTY_INPUT_OK, 11);
    assert(m.language == PENALTY_LANGUAGE_EN);
    penalty_model_set_language(&m, PENALTY_LANGUAGE_ZH_CN);
    assert(m.language == PENALTY_LANGUAGE_ZH_CN);
    penalty_model_set_language(&m, (penalty_language_t)99);
    assert(m.language == PENALTY_LANGUAGE_EN);

    penalty_model_input(&m, PENALTY_INPUT_RIGHT, 12);
    penalty_model_input(&m, PENALTY_INPUT_OK, 13);
    assert(m.state == PENALTY_TITLE && m.selection == 1);
    penalty_model_input(&m, PENALTY_INPUT_LEFT, 14);
    penalty_model_input(&m, PENALTY_INPUT_OK, 15);
    assert(m.muted && m.language == PENALTY_LANGUAGE_EN && m.state == PENALTY_AIM);
    for (unsigned s = PENALTY_ORIENT; s <= PENALTY_HELP; ++s) {
        m.state = (penalty_state_t)s;
        penalty_model_input(&m, PENALTY_INPUT_EXIT, 100 + s);
        assert(m.state == PENALTY_EXITING);
    }
    penalty_model_t a, b;
    begin(&a); begin(&b);
    for (unsigned i = 0; i < 1000; ++i) {
        uint64_t t = 100 + i * 10;
        penalty_model_input(&a, PENALTY_INPUT_OK, t);
        penalty_model_input(&b, PENALTY_INPUT_OK, t);
        assert(a.current.keeper == b.current.keeper);
        assert(a.current.keeper < PENALTY_TARGET_COUNT);
        penalty_shot_t before = a.current;
        uint32_t random_before = a.random;
        penalty_model_resync(&a, t + 1);
        penalty_model_resync(&b, t + 1);
        assert(a.current.green_low == before.green_low && a.current.green_high == before.green_high);
        assert(a.current.perfect_low == before.perfect_low && a.current.keeper == before.keeper);
        assert(a.current.outcome_roll == before.outcome_roll && a.random == random_before);
    }
}

static void five_misses(void) {
    penalty_model_t m;
    begin(&m);
    uint64_t t = 100;
    for (unsigned i = 0; i < 5; ++i) {
        penalty_model_input(&m, PENALTY_INPUT_OK, t);
        unsigned elapsed = i % 2 ? penalty_meter_half_ms(&m.current) : 8;
        penalty_model_input(&m, PENALTY_INPUT_OK, t + elapsed);
        penalty_model_tick(&m, t + elapsed + 600);
        penalty_model_tick(&m, t + elapsed + 1500);
        t += elapsed + 1600;
    }
    assert(m.state == PENALTY_SUMMARY && m.goals == 0 && m.completed == 5);
    penalty_model_input(&m, PENALTY_INPUT_RIGHT, t++);
    penalty_model_input(&m, PENALTY_INPUT_OK, t++);
    assert(m.state == PENALTY_TITLE);
    penalty_model_input(&m, PENALTY_INPUT_RIGHT, t++);
    penalty_model_input(&m, PENALTY_INPUT_RIGHT, t++);
    penalty_model_input(&m, PENALTY_INPUT_OK, t++);
    assert(m.state != PENALTY_EXITING); /* Third title item opens instructions. */
    assert(m.state == PENALTY_HELP);
    penalty_model_input(&m, PENALTY_INPUT_OK, t++);
    assert(m.state == PENALTY_TITLE && m.selection == 0);
}

static void generated_zones(void) {
    const unsigned widths[] = {20, 11, 6};
    for (unsigned d = 0; d < 3; ++d) {
        penalty_model_t m;
        begin(&m);
        m.difficulty = d;
        bool starts[91] = {false}, left_edge = false, right_edge = false;
        for (unsigned i = 0; i < 10000; ++i) {
            unsigned old = m.current.green_low;
            bool same_width = m.current.green_high - old + 1 == widths[d];
            m.state = PENALTY_SUMMARY; m.selection = 0;
            penalty_model_input(&m, PENALTY_INPUT_OK, 100 + i);
            assert(m.state == PENALTY_AIM && m.difficulty == (penalty_difficulty_t)d);
            const penalty_shot_t *s = &m.current;
            assert(s->green_low >= 30 && s->green_high <= 90);
            assert(s->green_high - s->green_low + 1 == (int)widths[d]);
            assert(s->perfect_low >= s->green_low && s->perfect_low + 1 <= s->green_high);
            assert(s->outcome_roll < 100 && s->keeper < PENALTY_TARGET_COUNT);
            if (same_width) assert(old != s->green_low);
            starts[s->green_low] = true;
            left_edge |= s->perfect_low == s->green_low;
            right_edge |= s->perfect_low + 1 == s->green_high;
        }
        for (unsigned low = 30; low + widths[d] - 1 <= 90; ++low) assert(starts[low]);
        assert(left_edge && right_edge);
    }
}

static void event_sequences(void) {
    penalty_model_t m;
    uint32_t random = 17;
    uint64_t time = 0;
    penalty_model_init(&m, random, time);
    for (unsigned i = 0; i < 100000; ++i) {
        random = random * 1664525u + 1013904223u;
        time += (random >> 24) + 1;
        if (m.state == PENALTY_EXITING) penalty_model_init(&m, random, time);
        if ((random & 15) == 0) penalty_model_resync(&m, time);
        else if (random & 1) penalty_model_tick(&m, time);
        else penalty_model_input(&m, (penalty_input_t)((random >> 8) % 4), time);
        unsigned goals = 0, perfect = 0;
        assert(m.completed <= 5);
        for (unsigned j = 0; j < m.completed; ++j) {
            goals += penalty_is_goal(m.shots[j].outcome);
            perfect += m.shots[j].outcome == PENALTY_PERFECT ||
                       m.shots[j].outcome == PENALTY_PERFECT_SAVE;
            assert(m.shots[j].direction < PENALTY_TARGET_COUNT);
            assert(m.shots[j].keeper < PENALTY_TARGET_COUNT);
        }
        assert(m.goals == goals && m.perfect == perfect);
        if (m.state == PENALTY_SUMMARY) assert(m.completed == 5);
    }
}

int main(void) {
    penalty_model_t menu;
    penalty_model_init(&menu, 42, 0);
    penalty_model_input(&menu, PENALTY_INPUT_OK, 1);
    for (unsigned i = 2; i <= 4; ++i)
        penalty_model_input(&menu, PENALTY_INPUT_RIGHT, i);
    assert(menu.selection == 2); /* PLAY, SETTINGS, HELP */
    boundaries(); calibrated_average_goal_rates(); meter_windows(); generated_zones();
    session(); timing_and_recovery(); menus_and_exit(); five_misses();
    event_sequences();
    puts("Penalty model: PASS");
    return 0;
}
