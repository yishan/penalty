#include "penalty_scene.h"

static int target_x(penalty_direction_t target) {
    return 70 + penalty_target_column(target) * 50;
}

static int target_y(penalty_direction_t target) {
    return penalty_target_row(target) ? 80 : 58;
}

static unsigned keeper_frame(penalty_direction_t target) {
    static const unsigned frames[PENALTY_TARGET_COUNT] = {1, 4, 2, 6, 3, 5};
    return (unsigned)target < PENALTY_TARGET_COUNT ? frames[target] : 0;
}

static unsigned blue_cue_frame(penalty_direction_t target) {
    static const unsigned frames[PENALTY_TARGET_COUNT] = {3, 4, 5, 8, 7, 6};
    return (unsigned)target < PENALTY_TARGET_COUNT ? frames[target] : 0;
}

static int lerp(int a, int b, int t) { return a + (b - a) * t / 1000; }

penalty_scene_t penalty_scene_at(const penalty_model_t *m, uint64_t now) {
    bool keeper_turn = penalty_player_is_keeper(m);
    penalty_scene_t s = {
        .striker = {48, 95, 0}, .keeper = {92, 43, 0},
        .ball_x = 131, .ball_y = 147, .shadow_y = 153,
        .target_x = target_x(m->current.direction), .target_y = target_y(m->current.direction),
        .ball_visible = true,
        .target_visible = m->state == PENALTY_AIM || m->state == PENALTY_CHARGE,
        .blue_striker = m->multiplayer ? m->multiplayer_blue_shooter : keeper_turn,
    };
    if (!m->multiplayer && keeper_turn && m->state == PENALTY_AIM &&
        penalty_keeper_cue_visible(m, now))
        s.striker.frame = blue_cue_frame(m->current.opponent_target);
    else if (!m->multiplayer && !keeper_turn && m->state == PENALTY_CHARGE)
        s.striker.frame = now - m->since_ms < 100 ? 1 : 2;
    bool moving = m->state == PENALTY_FLIGHT || m->state == PENALTY_RESULT;
    if (!moving || m->current.outcome == PENALTY_TIMEOUT ||
        (keeper_turn && m->state == PENALTY_RESULT &&
         m->current.outcome == PENALTY_KEEPER_LATE && m->current.timing_value == 0)) return s;
    uint64_t elapsed = now >= m->since_ms ? now - m->since_ms : 0;
    if (m->state == PENALTY_RESULT || elapsed > PENALTY_FLIGHT_MS) elapsed = PENALTY_FLIGHT_MS;
    s.striker.frame = s.blue_striker
        ? (elapsed < 100 ? 1 : 2) : (elapsed < 100 ? 3 : 4);
    int t = elapsed <= 100 ? 0 : (elapsed - 100) * 1000 / (PENALTY_FLIGHT_MS - 100);
    bool weak = m->current.outcome == PENALTY_WEAK;
    bool save = keeper_turn ? penalty_is_save(m->current.outcome) :
                (weak && !m->multiplayer) || m->current.outcome == PENALTY_SAVE ||
                m->current.outcome == PENALTY_GREEN_SAVE ||
                m->current.outcome == PENALTY_PERFECT_SAVE;
    bool special = m->current.outcome == PENALTY_PERFECT || m->current.outcome == PENALTY_GREEN_GOAL;
    penalty_direction_t shot_direction = keeper_turn
        ? m->current.opponent_target : m->current.player_target;
    penalty_direction_t kd = keeper_turn
        ? m->current.player_target
        : save && !m->multiplayer ? m->current.player_target : m->current.opponent_target;
    unsigned frame = keeper_frame(kd);
    const int glove_x[] = {28, 8, 49, 28, 22, 8, 47};
    const int glove_y[] = {43, 32, 27, 43, 7, 44, 45};
    int dest_x = target_x(shot_direction);
    int dest_y = target_y(shot_direction);
    if (m->current.outcome == PENALTY_HIGH) dest_y = 20;
    if (m->multiplayer && weak) dest_y = 120;
    if (m->multiplayer_accuracy_miss) {
        dest_x = penalty_target_column(shot_direction) == 2 ? 190 : 50;
        dest_y = target_y(shot_direction);
    }
    int kt = t <= 150 ? 0 : (t - 150) * 1000 / 850;
    if (special) kt = kt * 3 / 4;
    if (kt > 0) {
        s.keeper.frame = frame;
        s.keeper.x = lerp(92, target_x(kd) - glove_x[frame], kt);
        s.keeper.y = lerp(43, target_y(kd) - glove_y[frame], kt);
    }
    if (save) {
        dest_x = target_x(kd);
        dest_y = target_y(kd);
    }
    s.ball_x = lerp(131, dest_x, t);
    s.ball_y = lerp(147, dest_y, t) - 12 * t * (1000 - t) / 1000000;
    s.shadow_y = lerp(153, 94, t);
    s.ball_frame = t * 5 / 1000;
    if (save && t == 1000) {
        s.ball_x = s.keeper.x + glove_x[frame];
        s.ball_y = s.keeper.y + glove_y[frame];
    }
    return s;
}
