#include "penalty_multiplayer_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint64_t clock_ms;
static void check_jersey_projection(const penalty_mp_core_t *h, const penalty_mp_core_t *g) {
    penalty_model_t a, b;
    penalty_mp_project(h, &a, PENALTY_LANGUAGE_EN, false, clock_ms);
    penalty_mp_project(g, &b, PENALTY_LANGUAGE_ZH_CN, false, clock_ms);
    assert(a.multiplayer && b.multiplayer);
    assert(a.multiplayer_blue_shooter == (h->duel.current_shooter == PENALTY_DUEL_GUEST));
    assert(a.multiplayer_blue_shooter == b.multiplayer_blue_shooter);
    assert(a.turn_role != b.turn_role);
    assert(penalty_round_for_kick(h->duel.round_id) ==
           penalty_round_for_kick(g->duel.round_id));
}
static void pump(penalty_mp_core_t *host, penalty_mp_core_t *guest) {
    for (unsigned step = 0; step < 32; ++step) {
        penalty_mp_packet_t p;
        bool moved = false;
        if (penalty_mp_pop(host, &p)) {
            assert(penalty_mp_receive(guest, &p, clock_ms)); moved = true;
        }
        if (penalty_mp_pop(guest, &p)) {
            assert(penalty_mp_receive(host, &p, clock_ms)); moved = true;
        }
        if (!moved) return;
    }
    assert(!"outbox cycle");
}
static void pair(penalty_mp_core_t *h, penalty_mp_core_t *g, unsigned seed, unsigned difficulty) {
    const uint8_t id[16] = {1, 2, 3};
    const uint8_t blank[16] = {0};
    penalty_mp_init(h, PENALTY_DUEL_HOST, difficulty, id, seed, clock_ms);
    penalty_mp_init(g, PENALTY_DUEL_GUEST, PENALTY_NORMAL, blank, seed + 42, clock_ms);
    penalty_mp_input(g, PENALTY_INPUT_OK, clock_ms); /* Cannot confirm unknown settings. */
    assert(!g->out_count);
    penalty_mp_input(h, PENALTY_INPUT_OK, clock_ms); pump(h, g);
    assert(g->duel.difficulty == difficulty);
    penalty_mp_input(g, PENALTY_INPUT_OK, clock_ms); pump(h, g);
    assert(h->phase == PENALTY_MP_COIN && g->phase == PENALTY_MP_COIN);
    penalty_mp_input(g, PENALTY_INPUT_OK, clock_ms); assert(!g->out_count);
    penalty_mp_input(h, PENALTY_INPUT_OK, clock_ms); pump(h, g);
    assert(h->phase == PENALTY_MP_COIN_SPIN && g->phase == PENALTY_MP_COIN_SPIN);
    assert(h->duel.current_shooter == g->duel.current_shooter);
    assert(memcmp(h->duel.random_commit, g->duel.random_commit, 32) == 0);
    clock_ms += 1000;
    penalty_mp_tick(h, clock_ms); penalty_mp_tick(g, clock_ms);
}
static void lock(penalty_mp_core_t *c, uint8_t value) {
    penalty_mp_input(c, PENALTY_INPUT_OK, clock_ms);
    assert(c->phase == PENALTY_MP_CHARGE);
    /* Test exact boundary values through the real timestamp-to-meter function. */
    uint64_t elapsed = 0;
    penalty_duel_meter_t m = c->peer == c->duel.current_shooter
        ? c->duel.round_config.shooter_meter : c->duel.round_config.keeper_meter;
    penalty_shot_t shot = {.green_low = m.green_low, .green_high = m.green_high};
    while (elapsed < 4000 && penalty_power_at(&shot, elapsed) != value) ++elapsed;
    assert(elapsed < 4000);
    penalty_mp_input(c, PENALTY_INPUT_OK, clock_ms + elapsed);
    assert(c->phase == PENALTY_MP_WAIT && c->local_action.timing_value == value);
}
int main(void) {
    bool orders[2] = {false};
    for (unsigned seed = 1; seed <= 36; ++seed) {
        penalty_mp_core_t h, g;
        clock_ms = 100;
        pair(&h, &g, seed, seed % 3);
        orders[h.duel.first_shooter] = true;
        check_jersey_projection(&h, &g);
        for (unsigned round = 1; round <= 2; ++round) {
            h.local_action.target = seed % 6;
            g.local_action.target = (seed / 6) % 6;
            lock(&g, 40); pump(&h, &g);
            assert(!g.reveal_sent && !h.duel.reveal_set[1]);
            assert(!h.duel.total_settled || round == 2);
            lock(&h, 75); pump(&h, &g);
            assert(h.phase == PENALTY_MP_ANIMATION && g.phase == PENALTY_MP_ANIMATION);
            assert(h.duel.total_settled == round && g.duel.total_settled == round);
            assert(memcmp(h.duel.goals, g.duel.goals, sizeof(h.duel.goals)) == 0);
            assert(h.duel.round_result.outcome == g.duel.round_result.outcome);
            check_jersey_projection(&h, &g);
            assert(memcmp(h.duel.random_commit, g.duel.random_commit, 32) == 0);
            penalty_model_t v;
            penalty_mp_project(&g, &v, PENALTY_LANGUAGE_EN, false, clock_ms + 180);
            assert(v.state == PENALTY_FLIGHT);
            clock_ms += 800;
            penalty_mp_tick(&h, clock_ms); penalty_mp_tick(&g, clock_ms);
            check_jersey_projection(&h, &g);
            if (round == 1) {
                penalty_mp_input(&g, PENALTY_INPUT_OK, clock_ms); pump(&h, &g);
                assert(h.phase == PENALTY_MP_RESULT);
                penalty_mp_input(&h, PENALTY_INPUT_OK, clock_ms); pump(&h, &g);
                assert(h.phase == PENALTY_MP_AIM && g.phase == PENALTY_MP_AIM);
                assert(h.duel.current_shooter != h.duel.first_shooter);
                check_jersey_projection(&h, &g);
            }
        }
        assert(h.phase == PENALTY_MP_RESULT && g.phase == PENALTY_MP_RESULT);
        assert(h.duel.attempts[0] == 1 && h.duel.attempts[1] == 1);
    }
    assert(orders[0] && orders[1]);
    penalty_mp_core_t h, g;
    clock_ms = 100;
    pair(&h, &g, 40, PENALTY_NORMAL);
    lock(&h, 50); lock(&g, 50);
    penalty_mp_packet_t p;
    assert(penalty_mp_pop(&g, &p));
    assert(penalty_mp_receive(&h, &p, clock_ms));
    assert(penalty_mp_pop(&h, &p));
    assert(penalty_mp_receive(&g, &p, clock_ms));
    assert(penalty_mp_pop(&g, &p));
    assert(penalty_mp_receive(&h, &p, clock_ms));
    assert(penalty_mp_pop(&h, &p));
    p.bytes[22] ^= 1; /* Host random reveal does not match precommit. */
    assert(!penalty_mp_receive(&g, &p, clock_ms));
    assert(g.phase == PENALTY_MP_FAILED && g.duel.total_settled == 0);
    clock_ms = 100;
    pair(&h, &g, 42, PENALTY_HARD);
    penalty_mp_input(&g, PENALTY_INPUT_OK, clock_ms);
    penalty_mp_tick(&g, clock_ms + 4100);
    assert(g.local_action.timing_value == 0 && g.local_locked);
    penalty_mp_packet_t invalid = {.type = PENALTY_BLE_ACTION_REVEAL, .length = 30};
    memcpy(invalid.bytes, h.duel.match_id, 16);
    invalid.bytes[16] = 1;
    assert(!penalty_mp_receive(&h, &invalid, clock_ms)); /* No reveal before both commits. */
    assert(h.duel.total_settled == 0);
    puts("Penalty multiplayer core: PASS (both orders, 36 target pairs, role swap, commitment verification, timeout)");
}
