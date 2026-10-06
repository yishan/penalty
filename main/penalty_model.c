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
        .player_target = PENALTY_TOP_CENTER,
        .opponent_target = keeper,
        .green_low = 30 + start, .green_high = 30 + start + width - 1,
        .perfect_low = perfect, .outcome_roll = roll,
    };
    change_state(m, PENALTY_AIM, now);
}

static void new_practice(penalty_model_t *m, penalty_session_kind_t kind, uint64_t now) {
    m->completed = m->goals = m->saves = m->perfect = m->selection = 0;
    m->session_kind = kind;
    m->turn_role = kind == PENALTY_SESSION_KEEP_PRACTICE
        ? PENALTY_ROLE_KEEPER : PENALTY_ROLE_SHOOTER;
    memset(m->shots, 0, sizeof(m->shots));
    memset(&m->match, 0, sizeof(m->match));
    aim(m, now);
}

static void new_shootout(penalty_model_t *m, uint64_t now) {
    m->completed = m->goals = m->saves = m->perfect = m->selection = 0;
    m->session_kind = PENALTY_SESSION_SHOOTOUT;
    m->turn_role = PENALTY_ROLE_SHOOTER;
    memset(m->shots, 0, sizeof(m->shots));
    memset(&m->match, 0, sizeof(m->match));
    m->match.coin_face = PENALTY_COIN_UNSET;
    m->match.result = PENALTY_MATCH_IN_PROGRESS;
    change_state(m, PENALTY_COIN_READY, now);
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

static penalty_direction_t move_target(penalty_direction_t target, int step) {
    static const penalty_direction_t down[PENALTY_TARGET_COUNT] = {
        PENALTY_BOTTOM_LEFT, PENALTY_BOTTOM_CENTER, PENALTY_BOTTOM_RIGHT,
        PENALTY_TOP_CENTER, PENALTY_TOP_LEFT, PENALTY_TOP_RIGHT,
    };
    static const penalty_direction_t up[PENALTY_TARGET_COUNT] = {
        PENALTY_BOTTOM_CENTER, PENALTY_BOTTOM_RIGHT, PENALTY_BOTTOM_LEFT,
        PENALTY_TOP_RIGHT, PENALTY_TOP_CENTER, PENALTY_TOP_LEFT,
    };
    if ((unsigned)target >= PENALTY_TARGET_COUNT) return PENALTY_TOP_CENTER;
    return step > 0 ? down[target] : up[target];
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

static uint8_t save_chance(const penalty_shot_t *s) {
    bool exact = s->player_target == s->opponent_target;
    bool same_column = penalty_target_column(s->player_target) ==
                       penalty_target_column(s->opponent_target);
    bool perfect = s->timing_value >= s->perfect_low &&
                   s->timing_value <= s->perfect_low + 1;
    bool green = s->timing_value >= s->green_low &&
                 s->timing_value <= s->green_high;
    if (!same_column) return 0;
    if (perfect) return exact ? 90 : 35;
    if (green) return exact ? 65 : 20;
    return exact ? 25 : 5;
}

penalty_outcome_t penalty_judge_keeper(const penalty_shot_t *s) {
    if (s->timing_value < 30) return PENALTY_KEEPER_EARLY;
    if (s->timing_value > 90) return PENALTY_KEEPER_LATE;
    if (penalty_target_column(s->player_target) !=
        penalty_target_column(s->opponent_target))
        return PENALTY_KEEPER_WRONG_WAY;
    if (s->outcome_roll >= save_chance(s)) return PENALTY_KEEPER_READ_MISS;
    return s->player_target == s->opponent_target
        ? PENALTY_KEEPER_CATCH : PENALTY_KEEPER_PARRY;
}

uint16_t penalty_keeper_cue_ms(penalty_difficulty_t difficulty) {
    static const uint16_t durations[] = {900, 600, 350};
    return (unsigned)difficulty < 3 ? durations[difficulty] : durations[0];
}

bool penalty_keeper_cue_visible(const penalty_model_t *m, uint64_t now) {
    if (!m || !penalty_player_is_keeper(m) || m->state != PENALTY_AIM ||
        now < m->since_ms) return false;
    return now - m->since_ms < penalty_keeper_cue_ms(m->difficulty);
}

bool penalty_player_is_keeper(const penalty_model_t *m) {
    return m && m->turn_role == PENALTY_ROLE_KEEPER;
}

uint8_t penalty_coin_frame_at(const penalty_model_t *m, uint64_t now) {
    if (!m) return 0;
    if (m->state == PENALTY_COIN_RESULT)
        return m->match.coin_face == PENALTY_COIN_GLOVE ? 4 : 0;
    if (m->state != PENALTY_COIN_SPIN || now <= m->since_ms) return 0;
    uint64_t elapsed = now - m->since_ms;
    if (elapsed >= PENALTY_COIN_SPIN_MS) elapsed = PENALTY_COIN_SPIN_MS - 1;
    return (uint8_t)(elapsed * 8 / PENALTY_COIN_SPIN_MS);
}

uint32_t penalty_round_for_kick(uint32_t kick_id) {
    return kick_id / 2 + kick_id % 2; /* Avoid overflow at UINT32_MAX. */
}

uint32_t penalty_shootout_round(const penalty_model_t *m) {
    if (!m) return 0;
    if (m->match.sudden_death) return m->match.sudden_death_round;
    bool settled = m->state == PENALTY_RESULT || m->state == PENALTY_MATCH_SUMMARY;
    uint32_t round = settled ? penalty_round_for_kick(m->match.total_settled)
                            : m->match.total_settled / 2 + 1;
    if (!round) round = 1;
    return round > PENALTY_SHOTS ? PENALTY_SHOTS : round;
}

bool penalty_is_goal(penalty_outcome_t outcome) {
    return outcome == PENALTY_GOAL || outcome == PENALTY_PERFECT || outcome == PENALTY_GREEN_GOAL;
}

bool penalty_is_save(penalty_outcome_t outcome) {
    return outcome == PENALTY_KEEPER_CATCH || outcome == PENALTY_KEEPER_PARRY;
}

const char *penalty_outcome_text(penalty_outcome_t outcome) {
    static const char *const names[] = {
        "GOAL!", "PERFECT!", "SAVE", "SAVE - WEAK", "MISS - HIGH", "MISS - TIME",
        "GOOD SHOT!", "GREEN - SAVE", "PERFECT - SAVE",
        "CAUGHT!", "PARRIED!", "WRONG WAY", "READ IT - MISSED", "TOO EARLY", "TOO LATE"
    };
    return (unsigned)outcome < sizeof(names) / sizeof(names[0]) ? names[outcome] : "";
}

void penalty_model_init(penalty_model_t *m, uint32_t seed, uint64_t now) {
    *m = (penalty_model_t){
        .random = seed ? seed : 1,
        .last_input_ms = now,
        .session_kind = PENALTY_SESSION_SHOOT_PRACTICE,
        .turn_role = PENALTY_ROLE_SHOOTER,
        .language = PENALTY_DEFAULT_LANGUAGE,
    };
    change_state(m, PENALTY_COVER, now);
}

void penalty_model_set_language(penalty_model_t *m, penalty_language_t language) {
    m->language = (unsigned)language < PENALTY_LANGUAGE_COUNT
        ? language : PENALTY_DEFAULT_LANGUAGE;
}

static bool shot_stopped_by_keeper(penalty_outcome_t outcome) {
    return outcome == PENALTY_SAVE || outcome == PENALTY_WEAK ||
           outcome == PENALTY_GREEN_SAVE || outcome == PENALTY_PERFECT_SAVE;
}

static void remember_match_kick(penalty_model_t *m, bool scored) {
    penalty_match_t *match = &m->match;
    match->recent[match->recent_head] = (penalty_recent_kick_t){
        .player_role = m->turn_role,
        .outcome = m->current.outcome,
        .scoring_side_scored = scored,
    };
    match->recent_head = (uint8_t)((match->recent_head + 1) % PENALTY_RECENT_KICKS);
    if (match->recent_count < PENALTY_RECENT_KICKS) ++match->recent_count;
}

static void settle_match(penalty_model_t *m) {
    penalty_match_t *match = &m->match;
    bool keeper_turn = penalty_player_is_keeper(m);
    bool scored = keeper_turn ? !penalty_is_save(m->current.outcome)
                              : penalty_is_goal(m->current.outcome);
    bool sudden_death = match->sudden_death;
    remember_match_kick(m, scored);
    ++match->total_settled;
    if (keeper_turn) {
        if (!sudden_death && match->ai_attempts < PENALTY_SHOTS && scored)
            match->ai_regular_scored_bits |= (uint8_t)(1u << match->ai_attempts);
        ++match->ai_attempts;
        if (scored) ++match->ai_goals;
        else if (penalty_is_save(m->current.outcome)) ++match->player_saves;
        if (sudden_death) {
            match->pair_ai_done = true;
            match->pair_ai_scored = scored;
        }
    } else {
        if (!sudden_death && match->player_attempts < PENALTY_SHOTS && scored)
            match->player_regular_scored_bits |= (uint8_t)(1u << match->player_attempts);
        ++match->player_attempts;
        if (scored) ++match->player_goals;
        else if (shot_stopped_by_keeper(m->current.outcome)) ++match->ai_saves;
        if (sudden_death) {
            match->pair_player_done = true;
            match->pair_player_scored = scored;
        }
    }
}

static void settle(penalty_model_t *m, uint64_t now) {
    if (m->session_kind == PENALTY_SESSION_SHOOTOUT) {
        settle_match(m);
    } else {
        if (m->completed >= PENALTY_SHOTS) return;
        m->shots[m->completed++] = m->current;
        if (penalty_player_is_keeper(m)) m->saves += penalty_is_save(m->current.outcome);
        else m->goals += penalty_is_goal(m->current.outcome);
        m->perfect += m->current.timing_value >= m->current.perfect_low &&
                      m->current.timing_value <= m->current.perfect_low + 1;
    }
    change_state(m, PENALTY_RESULT, now);
}

static void decide_regular_match(penalty_model_t *m) {
    penalty_match_t *match = &m->match;
    uint32_t player_remaining = PENALTY_SHOTS - match->player_attempts;
    uint32_t ai_remaining = PENALTY_SHOTS - match->ai_attempts;
    if (match->player_goals > match->ai_goals + ai_remaining) {
        match->result = PENALTY_MATCH_PLAYER_WIN;
    } else if (match->ai_goals > match->player_goals + player_remaining) {
        match->result = PENALTY_MATCH_AI_WIN;
    }
    if (match->result != PENALTY_MATCH_IN_PROGRESS) {
        match->ended_early = match->player_attempts < PENALTY_SHOTS ||
                             match->ai_attempts < PENALTY_SHOTS;
    } else if (match->player_attempts == PENALTY_SHOTS &&
               match->ai_attempts == PENALTY_SHOTS) {
        if (match->player_goals > match->ai_goals)
            match->result = PENALTY_MATCH_PLAYER_WIN;
        else if (match->ai_goals > match->player_goals)
            match->result = PENALTY_MATCH_AI_WIN;
        else {
            match->sudden_death = true;
            match->sudden_death_round = 1;
            match->pair_player_done = match->pair_ai_done = false;
            match->pair_player_scored = match->pair_ai_scored = false;
        }
    }
}

static void decide_sudden_death(penalty_model_t *m) {
    penalty_match_t *match = &m->match;
    if (!match->pair_player_done || !match->pair_ai_done) return;
    if (match->pair_player_scored != match->pair_ai_scored) {
        match->result = match->pair_player_scored
            ? PENALTY_MATCH_PLAYER_WIN : PENALTY_MATCH_AI_WIN;
        return;
    }
    ++match->sudden_death_round;
    match->pair_player_done = match->pair_ai_done = false;
    match->pair_player_scored = match->pair_ai_scored = false;
}

static void finish_match_result(penalty_model_t *m, uint64_t now) {
    if (m->match.sudden_death) decide_sudden_death(m);
    else decide_regular_match(m);
    if (m->match.result != PENALTY_MATCH_IN_PROGRESS) {
        m->selection = 0;
        change_state(m, PENALTY_MATCH_SUMMARY, now);
        return;
    }
    m->turn_role = m->turn_role == PENALTY_ROLE_SHOOTER
        ? PENALTY_ROLE_KEEPER : PENALTY_ROLE_SHOOTER;
    change_state(m, PENALTY_HANDOFF, now);
}

void penalty_model_tick(penalty_model_t *m, uint64_t now) {
    if (now < m->since_ms) return;
    uint64_t elapsed = now - m->since_ms;
    switch (m->state) {
    case PENALTY_COIN_SPIN:
        if (elapsed >= PENALTY_COIN_SPIN_MS) change_state(m, PENALTY_COIN_RESULT, now);
        break;
    case PENALTY_CHARGE:
        m->current.power = penalty_power_at(&m->current, elapsed);
        if (elapsed >= PENALTY_TIMEOUT_MS) {
            m->current.outcome = penalty_player_is_keeper(m)
                ? PENALTY_KEEPER_LATE : PENALTY_TIMEOUT;
            settle(m, now);
        }
        break;
    case PENALTY_FLIGHT:
        if (elapsed >= PENALTY_FLIGHT_MS) settle(m, now);
        break;
    case PENALTY_RESULT:
        if (elapsed >= PENALTY_RESULT_MS) {
            if (m->session_kind == PENALTY_SESSION_SHOOTOUT) {
                finish_match_result(m, now);
            } else if (m->completed == PENALTY_SHOTS) {
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
        if (m->session_kind == PENALTY_SESSION_SHOOTOUT &&
            m->match.result == PENALTY_MATCH_IN_PROGRESS)
            m->match.result = PENALTY_MATCH_ABANDONED;
        change_state(m, PENALTY_EXITING, at);
        return;
    }
    penalty_state_t before = m->state;
    penalty_model_tick(m, at);
    /* A press during a transition cannot operate the newly appearing page. */
    if (before != m->state) return;
    int step = input == PENALTY_INPUT_UP ? -1 : input == PENALTY_INPUT_DOWN ? 1 : 0;
    switch (m->state) {
    case PENALTY_COVER:
        if (input == PENALTY_INPUT_OK) change_state(m, PENALTY_TITLE, at);
        break;
    case PENALTY_TITLE: {
        m->selection = (uint8_t)((m->selection + step + 5) % 5);
        if (input != PENALTY_INPUT_OK) break;
        if (m->selection == 0) new_shootout(m, at);
        else if (m->selection == 1)
            new_practice(m, PENALTY_SESSION_SHOOT_PRACTICE, at);
        else if (m->selection == 2)
            new_practice(m, PENALTY_SESSION_KEEP_PRACTICE, at);
        else if (m->selection == 3) {
            m->selection = 0;
            change_state(m, PENALTY_SETTINGS, at);
        } else change_state(m, PENALTY_HELP, at);
        break;
    }
    case PENALTY_SETTINGS: {
        m->selection = (uint8_t)((m->selection + step + 4) % 4);
        if (input != PENALTY_INPUT_OK) break;
        if (m->selection == 0) m->difficulty = (m->difficulty + 1) % 3;
        else if (m->selection == 1) m->muted = !m->muted;
        else if (m->selection == 2)
            m->language = m->language == PENALTY_LANGUAGE_EN
                ? PENALTY_LANGUAGE_ZH_CN : PENALTY_LANGUAGE_EN;
        else {
            m->selection = 3;
            change_state(m, PENALTY_TITLE, at);
        }
        break;
    }
    case PENALTY_SUMMARY: {
        int selection = (int)m->selection + step;
        if (selection >= 0 && selection <= 1) m->selection = (uint8_t)selection;
        if (input != PENALTY_INPUT_OK) break;
        if (m->selection == 0) new_practice(m, m->session_kind, at);
        else {
            m->selection = 0;
            change_state(m, PENALTY_TITLE, at);
        }
        break;
    }
    case PENALTY_MATCH_SUMMARY: {
        int selection = (int)m->selection + step;
        if (selection >= 0 && selection <= 1) m->selection = (uint8_t)selection;
        if (input != PENALTY_INPUT_OK) break;
        if (m->selection == 0) new_shootout(m, at);
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
    case PENALTY_COIN_READY:
        if (input == PENALTY_INPUT_OK) {
            m->match.coin_face = (penalty_coin_face_t)(PENALTY_COIN_FOOTBALL +
                                                       random_below(m, 2));
            m->match.first_role = m->match.coin_face == PENALTY_COIN_FOOTBALL
                ? PENALTY_ROLE_SHOOTER : PENALTY_ROLE_KEEPER;
            m->turn_role = m->match.first_role;
            change_state(m, PENALTY_COIN_SPIN, at);
        }
        break;
    case PENALTY_COIN_RESULT:
        if (input == PENALTY_INPUT_OK) aim(m, at);
        break;
    case PENALTY_HANDOFF:
        if (input == PENALTY_INPUT_OK) aim(m, at);
        break;
    case PENALTY_AIM: {
        if (step) m->current.direction = move_target(m->current.direction, step);
        if (input == PENALTY_INPUT_OK) {
            change_state(m, PENALTY_CHARGE, at);
        }
        break;
    }
    case PENALTY_CHARGE:
        if (input == PENALTY_INPUT_OK) {
            m->current.outcome = penalty_player_is_keeper(m)
                ? penalty_judge_keeper(&m->current) : penalty_judge(&m->current);
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
        m->current.timing_value = 0;
        m->current.player_target = PENALTY_TOP_CENTER;
        change_state(m, PENALTY_AIM, now);
    }
    m->last_input_ms = now;
}
