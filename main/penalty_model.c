#include "penalty_model.h"

#include <string.h>

static void change_state(penalty_model_t *m, penalty_state_t state, uint64_t now) {
    m->state = state;
    m->since_ms = now;
}

static uint32_t random_next(penalty_model_t *m) {
    uint32_t x;
    x = m->random;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    m->random = x;
    return x;
}

static uint32_t random_below(penalty_model_t *m, uint32_t bound) {
    /* xorshift has UINT32_MAX nonzero states. Rejection avoids modulo bias. */
    uint32_t limit = UINT32_MAX - UINT32_MAX % bound, value;
    do { value = random_next(m) - 1; } while (value >= limit);
    return value % bound;
}

static void aim(penalty_model_t *m, uint64_t now) {
    static const uint8_t widths[] = {20, 11, 6};
    unsigned width = widths[m->difficulty], starts = 62 - width;
    unsigned old = m->current.green_low;
    bool exclude = old >= 30 && m->current.green_high <= 90 &&
                   m->current.green_high - old + 1 == width;
    unsigned start = random_below(m, starts - exclude);
    if (exclude && start >= old - 30) ++start;
    penalty_direction_t keeper = (penalty_direction_t)random_below(m, PENALTY_TARGET_COUNT);
    unsigned perfect = 30 + start + random_below(m, width - 1);
    unsigned roll = random_below(m, 100);
    m->current = (penalty_shot_t){
        .direction = PENALTY_BOTTOM_CENTER,
        .keeper = keeper,
        .green_low = 30 + start, .green_high = 30 + start + width - 1,
        .perfect_low = perfect, .outcome_roll = roll,
    };
    change_state(m, PENALTY_AIM, now);
}

static void new_session(penalty_model_t *m, uint64_t now) {
    m->completed = m->goals = m->perfect = m->selection = 0;
    memset(m->shots, 0, sizeof(m->shots));
    aim(m, now);
}

const char *penalty_difficulty_text(penalty_difficulty_t difficulty) {
    static const char *const names[] = {"EASY", "NORMAL", "HARD"};
    return (unsigned)difficulty < 3 ? names[difficulty] : "";
}

uint16_t penalty_meter_half_ms(const penalty_shot_t *s) {
    return 800 + (s->green_high - s->green_low + 1) * 32;
}

uint8_t penalty_power_at(const penalty_shot_t *s, uint64_t elapsed) {
    uint32_t half = penalty_meter_half_ms(s);
    uint32_t phase = elapsed % (2 * half);
    if (phase > half) phase = 2 * half - phase;
    uint32_t enter = s->green_low * 8;
    uint32_t slow = (s->green_high - s->green_low + 1) * 40;
    if (phase < enter) return phase / 8;
    if (phase < enter + slow) return s->green_low + (phase - enter) / 40;
    return s->green_high + 1 + (phase - enter - slow) / 8;
}

uint8_t penalty_target_column(penalty_direction_t target) {
    static const uint8_t columns[PENALTY_TARGET_COUNT] = {0, 1, 2, 2, 1, 0};
    return (unsigned)target < PENALTY_TARGET_COUNT ? columns[target] : 1;
}

uint8_t penalty_target_row(penalty_direction_t target) {
    return (unsigned)target < PENALTY_TARGET_COUNT && target >= PENALTY_BOTTOM_RIGHT ? 1 : 0;
}

static uint8_t goal_chance(const penalty_shot_t *s) {
    bool exact = s->direction == s->keeper;
    bool same_column = penalty_target_column(s->direction) == penalty_target_column(s->keeper);
    bool perfect = s->power >= s->perfect_low && s->power <= s->perfect_low + 1;
    bool green = s->power >= s->green_low && s->power <= s->green_high;
    if (perfect) return exact ? 75 : same_column ? 85 : 95;
    if (green) return exact ? 30 : same_column ? 40 : 80;
    return exact ? 0 : same_column ? 10 : 35;
}

penalty_outcome_t penalty_judge(const penalty_shot_t *s) {
    if (s->power < 30) return PENALTY_WEAK;
    if (s->power > 90) return PENALTY_HIGH;
    bool goal = s->outcome_roll < goal_chance(s);
    if (s->power >= s->perfect_low && s->power <= s->perfect_low + 1)
        return goal ? PENALTY_PERFECT : PENALTY_PERFECT_SAVE;
    if (s->power >= s->green_low && s->power <= s->green_high)
        return goal ? PENALTY_GREEN_GOAL : PENALTY_GREEN_SAVE;
    return goal ? PENALTY_GOAL : PENALTY_SAVE;
}

bool penalty_is_goal(penalty_outcome_t outcome) {
    return outcome == PENALTY_GOAL || outcome == PENALTY_PERFECT || outcome == PENALTY_GREEN_GOAL;
}

const char *penalty_outcome_text(penalty_outcome_t outcome) {
    static const char *const names[] = {
        "GOAL!", "PERFECT!", "SAVE", "SAVE - WEAK", "MISS - HIGH", "MISS - TIME",
        "GOOD SHOT!", "GREEN - SAVE", "PERFECT - SAVE"
    };
    return (unsigned)outcome < sizeof(names) / sizeof(names[0]) ? names[outcome] : "";
}

void penalty_model_init(penalty_model_t *m, uint32_t seed, uint64_t now) {
    *m = (penalty_model_t){ .random = seed ? seed : 1, .last_input_ms = now };
    change_state(m, PENALTY_ORIENT, now);
}

void penalty_model_set_language(penalty_model_t *m, penalty_language_t language) {
    m->language = (unsigned)language < PENALTY_LANGUAGE_COUNT
        ? language : PENALTY_LANGUAGE_EN;
}

static void settle(penalty_model_t *m, uint64_t now) {
    if (m->completed >= PENALTY_SHOTS) return;
    m->shots[m->completed++] = m->current;
    m->goals += penalty_is_goal(m->current.outcome);
    m->perfect += m->current.outcome == PENALTY_PERFECT ||
                  m->current.outcome == PENALTY_PERFECT_SAVE;
    change_state(m, PENALTY_RESULT, now);
}

void penalty_model_tick(penalty_model_t *m, uint64_t now) {
    if (now < m->since_ms) return;
    uint64_t elapsed = now - m->since_ms;
    switch (m->state) {
    case PENALTY_CHARGE:
        m->current.power = penalty_power_at(&m->current, elapsed);
        if (elapsed >= PENALTY_TIMEOUT_MS) {
            m->current.outcome = PENALTY_TIMEOUT;
            settle(m, now);
        }
        break;
    case PENALTY_FLIGHT:
        if (elapsed >= PENALTY_FLIGHT_MS) settle(m, now);
        break;
    case PENALTY_RESULT:
        if (elapsed >= PENALTY_RESULT_MS) {
            if (m->completed == PENALTY_SHOTS) {
                m->selection = 0;
                change_state(m, PENALTY_SUMMARY, now);
            } else aim(m, now);
        }
        break;
    default: break;
    }
}

void penalty_model_input(penalty_model_t *m, penalty_input_t input, uint64_t at) {
    if (at <= m->last_input_ms || at < m->since_ms) return;
    m->last_input_ms = at;
    if (input == PENALTY_INPUT_EXIT) {
        change_state(m, PENALTY_EXITING, at);
        return;
    }
    penalty_state_t before = m->state;
    penalty_model_tick(m, at);
    /* A press during a transition cannot operate the newly appearing page. */
    if (before != m->state) return;
    int step = input == PENALTY_INPUT_LEFT ? -1 : input == PENALTY_INPUT_RIGHT ? 1 : 0;
    switch (m->state) {
    case PENALTY_ORIENT:
        if (input == PENALTY_INPUT_OK) change_state(m, PENALTY_TITLE, at);
        break;
    case PENALTY_TITLE: {
        int selection = (int)m->selection + step;
        if (selection >= 0 && selection <= 2) m->selection = (uint8_t)selection;
        if (input != PENALTY_INPUT_OK) break;
        if (m->selection == 0) new_session(m, at);
        else if (m->selection == 1) {
            m->selection = 0;
            change_state(m, PENALTY_SETTINGS, at);
        } else change_state(m, PENALTY_HELP, at);
        break;
    }
    case PENALTY_SETTINGS: {
        int selection = (int)m->selection + step;
        if (selection >= 0 && selection <= 3) m->selection = (uint8_t)selection;
        if (input != PENALTY_INPUT_OK) break;
        if (m->selection == 0) m->difficulty = (m->difficulty + 1) % 3;
        else if (m->selection == 1) m->muted = !m->muted;
        else if (m->selection == 2)
            m->language = m->language == PENALTY_LANGUAGE_EN
                ? PENALTY_LANGUAGE_ZH_CN : PENALTY_LANGUAGE_EN;
        else {
            m->selection = 1;
            change_state(m, PENALTY_TITLE, at);
        }
        break;
    }
    case PENALTY_SUMMARY: {
        int selection = (int)m->selection + step;
        if (selection >= 0 && selection <= 1) m->selection = (uint8_t)selection;
        if (input != PENALTY_INPUT_OK) break;
        if (m->selection == 0) new_session(m, at);
        else {
            m->selection = 0;
            change_state(m, PENALTY_TITLE, at);
        }
        break;
    }
    case PENALTY_HELP:
        if (input == PENALTY_INPUT_OK) {
            m->selection = 0;
            change_state(m, PENALTY_TITLE, at);
        }
        break;
    case PENALTY_AIM: {
        if (step) {
            int direction = (int)m->current.direction + step;
            if (direction < 0) direction += PENALTY_TARGET_COUNT;
            if (direction >= PENALTY_TARGET_COUNT) direction -= PENALTY_TARGET_COUNT;
            m->current.direction = (penalty_direction_t)direction;
        }
        if (input == PENALTY_INPUT_OK) {
            change_state(m, PENALTY_CHARGE, at);
        }
        break;
    }
    case PENALTY_CHARGE:
        if (input == PENALTY_INPUT_OK) {
            m->current.outcome = penalty_judge(&m->current);
            change_state(m, PENALTY_FLIGHT, at);
        }
        break;
    default: break;
    }
}

void penalty_model_resync(penalty_model_t *m, uint64_t now) {
    if (now < m->since_ms) return;
    if (m->state == PENALTY_CHARGE || m->state == PENALTY_AIM) {
        /* Retry the same uncommitted kick, never reroll its hidden decision. */
        m->current.power = 0;
        m->current.direction = PENALTY_BOTTOM_CENTER;
        change_state(m, PENALTY_AIM, now);
    }
    m->last_input_ms = now;
}
