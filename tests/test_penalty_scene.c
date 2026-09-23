#include "penalty_scene.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    penalty_model_t m;
    penalty_model_init(&m, 42, 0);
    penalty_model_input(&m, PENALTY_INPUT_OK, 1);
    penalty_model_input(&m, PENALTY_INPUT_OK, 2);
    penalty_scene_t ready = penalty_scene_at(&m, 2);
    assert(!ready.blue_striker);
    assert(ready.striker.frame == 0 && ready.ball_x == 131 && ready.ball_y == 147);
    assert(ready.target_x == 120 && ready.target_y == 58);
    assert(ready.target_visible && ready.ball_visible);
    for (unsigned d = 0; d < PENALTY_TARGET_COUNT; ++d) {
      for (unsigned k = 0; k < PENALTY_TARGET_COUNT; ++k) {
        for (unsigned outcome = 0; outcome <= PENALTY_PERFECT_SAVE; ++outcome) {
            m.current.direction = d; m.current.keeper = k; m.current.outcome = outcome;
            if (outcome == PENALTY_TIMEOUT) {
                m.state = PENALTY_RESULT;
                penalty_scene_t timeout = penalty_scene_at(&m, 2000);
                assert(timeout.striker.frame == 0 && timeout.keeper.frame == 0);
                assert(timeout.ball_x == ready.ball_x && timeout.ball_y == ready.ball_y);
                continue;
            }
            m.state = PENALTY_FLIGHT; m.since_ms = 1000;
            penalty_scene_t contact = penalty_scene_at(&m, 1100);
            assert(contact.ball_x == ready.ball_x && contact.ball_y == ready.ball_y);
            assert(contact.striker.frame == 4 && !contact.target_visible);
            penalty_scene_t end = penalty_scene_at(&m, 1600);
            for (unsigned t = 0; t <= 700; ++t) {
                penalty_scene_t s = penalty_scene_at(&m, 1000 + t);
                assert(s.ball_frame < 6 && s.keeper.frame < 7 && s.striker.frame < 5);
                assert(s.ball_x >= 65 && s.ball_x <= 175 && s.ball_y >= 18 && s.ball_y <= 147);
                assert(s.striker.y + 88 <= 183);
                if (t <= 100) assert(s.ball_x == ready.ball_x && s.ball_y == ready.ball_y);
                if (t >= 600) assert(s.ball_x == end.ball_x && s.ball_y == end.ball_y);
            }
            if (outcome == PENALTY_SAVE || outcome == PENALTY_GREEN_SAVE ||
                outcome == PENALTY_PERFECT_SAVE || outcome == PENALTY_WEAK) {
                const int gx[] = {28, 8, 49, 28, 22, 8, 47};
                const int gy[] = {43, 32, 27, 43, 7, 44, 45};
                assert(end.ball_x == end.keeper.x + gx[end.keeper.frame]);
                assert(end.ball_y == end.keeper.y + gy[end.keeper.frame]);
            }
            if (penalty_is_goal(outcome)) {
                assert(end.ball_x == 70 + (int)penalty_target_column(d) * 50);
                assert(end.ball_y == (penalty_target_row(d) ? 80 : 58));
            }
            m.state = PENALTY_RESULT;
            penalty_scene_t result = penalty_scene_at(&m, 2000);
            if (outcome == PENALTY_TIMEOUT) {
                assert(result.striker.frame == 0 && result.keeper.frame == 0);
                assert(result.ball_x == ready.ball_x && result.ball_y == ready.ball_y);
            } else assert(result.ball_x == end.ball_x && result.ball_y == end.ball_y);
        }
      }
    }

    penalty_model_t keeper;
    penalty_model_init(&keeper, 42, 0);
    penalty_model_input(&keeper, PENALTY_INPUT_OK, 1);
    penalty_model_input(&keeper, PENALTY_INPUT_DOWN, 2);
    penalty_model_input(&keeper, PENALTY_INPUT_OK, 3);
    assert(keeper.mode == PENALTY_MODE_KEEPER && keeper.state == PENALTY_AIM);
    static const unsigned cue_frame[PENALTY_TARGET_COUNT] = {3, 4, 5, 8, 7, 6};
    static const unsigned dive_frame[PENALTY_TARGET_COUNT] = {1, 4, 2, 6, 3, 5};
    for (unsigned shot = 0; shot < PENALTY_TARGET_COUNT; ++shot) {
        keeper.current.opponent_target = (penalty_direction_t)shot;
        penalty_scene_t cue = penalty_scene_at(&keeper, keeper.since_ms + 1);
        assert(cue.blue_striker && cue.striker.frame == cue_frame[shot]);
        penalty_scene_t expired = penalty_scene_at(&keeper,
            keeper.since_ms + penalty_keeper_cue_ms(keeper.difficulty));
        assert(expired.blue_striker && expired.striker.frame == 0);
        for (unsigned dive = 0; dive < PENALTY_TARGET_COUNT; ++dive) {
            keeper.current.player_target = (penalty_direction_t)dive;
            keeper.current.outcome = PENALTY_KEEPER_WRONG_WAY;
            keeper.state = PENALTY_FLIGHT;
            keeper.since_ms = 1000;
            penalty_scene_t contact = penalty_scene_at(&keeper, 1000);
            penalty_scene_t follow = penalty_scene_at(&keeper, 1100);
            penalty_scene_t end = penalty_scene_at(&keeper, 1600);
            assert(contact.blue_striker && contact.striker.frame == 1);
            assert(follow.striker.frame == 2);
            assert(end.keeper.frame == dive_frame[dive]);
            assert(end.ball_x == 70 + (int)penalty_target_column((penalty_direction_t)shot) * 50);
            assert(end.ball_y == (penalty_target_row((penalty_direction_t)shot) ? 80 : 58));
        }
        keeper.state = PENALTY_AIM;
        keeper.since_ms = 3;
    }
    puts("Penalty scene: PASS");
}
