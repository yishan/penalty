#include "penalty_scene.h"

static int target_x(penalty_direction_t target) {
    return 110 + penalty_target_column(target) * 50;
}

static int target_y(penalty_direction_t target) {
    return penalty_target_row(target) ? 80 : 58;
}

static unsigned keeper_frame(penalty_direction_t target) {
    static const unsigned frames[] = {1, 3, 2};
    return frames[penalty_target_column(target)];
}

static int lerp(int a, int b, int t) { return a + (b - a) * t / 1000; }

penalty_scene_t penalty_scene_at(const penalty_model_t *m, uint64_t now) {
    penalty_scene_t s = {
        .striker = {88, 95, 0}, .keeper = {132, 43, 0},
        .ball_x = 171, .ball_y = 147, .shadow_y = 153,
        .target_x = target_x(m->current.direction), .target_y = target_y(m->current.direction),
        .ball_visible = true,
        .target_visible = m->state == PENALTY_AIM || m->state == PENALTY_CHARGE,
    };
    bool moving = m->state == PENALTY_FLIGHT || m->state == PENALTY_RESULT;
    if (!moving || m->current.outcome == PENALTY_TIMEOUT) return s;
    uint64_t elapsed = now >= m->since_ms ? now - m->since_ms : 0;
    if (m->state == PENALTY_RESULT || elapsed > PENALTY_FLIGHT_MS) elapsed = PENALTY_FLIGHT_MS;
    s.striker.frame = elapsed < 50 ? 1 : 2;
    s.striker.x = s.striker.frame == 1 ? 86 : 92; /* All support feet stay at (120,177). */
    int t = elapsed <= 100 ? 0 : (elapsed - 100) * 1000 / (PENALTY_FLIGHT_MS - 100);
    bool weak = m->current.outcome == PENALTY_WEAK;
    bool save = weak || m->current.outcome == PENALTY_SAVE ||
                m->current.outcome == PENALTY_GREEN_SAVE ||
                m->current.outcome == PENALTY_PERFECT_SAVE;
    bool special = m->current.outcome == PENALTY_PERFECT || m->current.outcome == PENALTY_GREEN_GOAL;
    penalty_direction_t kd = save ? m->current.direction : m->current.keeper;
    unsigned frame = keeper_frame(kd);
    const int glove_x[] = {28, 8, 49, 28}, glove_y[] = {43, 32, 27, 43};
    int dest_x = target_x(m->current.direction);
    int dest_y = target_y(m->current.direction);
    if (m->current.outcome == PENALTY_HIGH) dest_y = 20;
    int kt = t <= 150 ? 0 : (t - 150) * 1000 / 850;
    if (special) kt = kt * 3 / 4;
    if (kt > 0) {
        s.keeper.frame = frame;
        s.keeper.x = lerp(132, target_x(kd) - glove_x[frame], kt);
        s.keeper.y = lerp(43, target_y(kd) - glove_y[frame], kt);
    }
    s.ball_x = lerp(171, dest_x, t);
    s.ball_y = lerp(147, dest_y, t) - 12 * t * (1000 - t) / 1000000;
    s.shadow_y = lerp(153, 94, t);
    s.ball_frame = t * 5 / 1000;
    return s;
}
