#include "penalty_scene.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    penalty_model_t m;
    penalty_model_init(&m, 42, 0);
    penalty_model_input(&m, PENALTY_INPUT_OK, 1);
    penalty_model_input(&m, PENALTY_INPUT_OK, 2);
    penalty_scene_t ready = penalty_scene_at(&m, 2);
    assert(ready.striker.frame == 0 && ready.ball_x == 171 && ready.ball_y == 147);
    assert(ready.striker.x + 32 == 120 && ready.striker.y + 82 == 177);
    assert(ready.target_x == 160 && ready.target_y == 80);
    assert(ready.target_visible && ready.ball_visible);
    for (unsigned d = 0; d < PENALTY_TARGET_COUNT; ++d) {
      for (unsigned k = 0; k < PENALTY_TARGET_COUNT; ++k) {
        for (unsigned outcome = 0; outcome < PENALTY_OUTCOME_COUNT; ++outcome) {
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
            assert(contact.striker.frame == 2 && !contact.target_visible);
            assert(contact.striker.x + 28 == 120 && contact.striker.y + 82 == 177);
            /* Calibrated toe at (71,52), ball's near edge touches the toe. */
            assert(contact.ball_x - (contact.striker.x + 71) == 8);
            assert(contact.ball_y == contact.striker.y + 52);
            penalty_scene_t end = penalty_scene_at(&m, 1600);
            for (unsigned t = 0; t <= 700; ++t) {
                penalty_scene_t s = penalty_scene_at(&m, 1000 + t);
                assert(s.ball_frame < 6 && s.keeper.frame < 4 && s.striker.frame < 3);
                assert(s.ball_x >= 105 && s.ball_x <= 215 && s.ball_y >= 18 && s.ball_y <= 147);
                assert(s.striker.y + 88 <= 183);
                if (t <= 100) assert(s.ball_x == ready.ball_x && s.ball_y == ready.ball_y);
                if (t >= 600) assert(s.ball_x == end.ball_x && s.ball_y == end.ball_y);
            }
            if (outcome == PENALTY_SAVE || outcome == PENALTY_GREEN_SAVE ||
                outcome == PENALTY_PERFECT_SAVE || outcome == PENALTY_WEAK) {
                const int gx[] = {28, 8, 49, 28}, gy[] = {43, 32, 27, 43};
                assert(end.ball_x == end.keeper.x + gx[end.keeper.frame]);
                assert(end.ball_y == end.keeper.y + gy[end.keeper.frame]);
            }
            if (penalty_is_goal(outcome)) {
                assert(end.ball_x == 110 + (int)penalty_target_column(d) * 50);
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
    puts("Penalty scene: PASS");
}
