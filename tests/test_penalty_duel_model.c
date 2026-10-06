#include "penalty_duel_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t MATCH_ID[PENALTY_DUEL_MATCH_ID_BYTES] = {
    0x50, 0x45, 0x4e, 0x41, 0x4c, 0x54, 0x59, 0x2d,
    0x44, 0x55, 0x45, 0x4c, 0x2d, 0x30, 0x30, 0x31,
};

static penalty_duel_round_config_t round_config(uint8_t shot_roll,
                                                 uint8_t save_roll) {
    penalty_duel_round_config_t config = {
        .shooter_meter = {.green_low = 60, .green_high = 75, .perfect_low = 68},
        .keeper_meter = {.green_low = 60, .green_high = 75, .perfect_low = 68},
        .shot_roll = shot_roll,
        .save_roll = save_roll,
        .random_nonce = 0xa5a5000000000000ULL | shot_roll << 8 | save_roll,
    };
    return config;
}

static penalty_duel_action_t action(penalty_direction_t target,
                                    uint8_t timing,
                                    uint64_t nonce) {
    penalty_duel_action_t value = {
        .target = target,
        .timing_value = timing,
        .nonce = nonce,
    };
    return value;
}

static void commit_and_reveal(penalty_duel_model_t *model,
                              penalty_duel_action_t host_action,
                              penalty_duel_action_t guest_action) {
    uint8_t host_commit[PENALTY_DUEL_COMMIT_BYTES];
    uint8_t guest_commit[PENALTY_DUEL_COMMIT_BYTES];
    penalty_duel_make_commit(model, PENALTY_DUEL_HOST, &host_action, host_commit);
    penalty_duel_make_commit(model, PENALTY_DUEL_GUEST, &guest_action, guest_commit);
    assert(penalty_duel_commit(model, PENALTY_DUEL_GUEST, guest_commit));
    assert(model->phase == PENALTY_DUEL_WAIT_COMMITS);
    assert(penalty_duel_commit(model, PENALTY_DUEL_HOST, host_commit));
    assert(model->phase == PENALTY_DUEL_WAIT_REVEALS);
    assert(penalty_duel_reveal(model, PENALTY_DUEL_HOST, &host_action));
    assert(model->phase == PENALTY_DUEL_WAIT_REVEALS);
    assert(penalty_duel_reveal(model, PENALTY_DUEL_GUEST, &guest_action));
    assert(model->phase == PENALTY_DUEL_ROUND_RESULT);
}

static void creator_controls_coin_without_fixing_roles(void) {
    penalty_duel_model_t model;
    penalty_duel_init(&model, MATCH_ID, PENALTY_NORMAL);
    penalty_duel_round_config_t config = round_config(0, 99);

    assert(model.phase == PENALTY_DUEL_WAIT_COIN);
    penalty_duel_snapshot_t waiting;
    penalty_duel_export_snapshot(&model, &waiting);
    penalty_duel_model_t restored;
    assert(penalty_duel_apply_snapshot(&restored, &waiting));
    assert(restored.phase == PENALTY_DUEL_WAIT_COIN);
    assert(!penalty_duel_trigger_coin(&model, PENALTY_DUEL_GUEST, 0, &config));
    assert(model.phase == PENALTY_DUEL_WAIT_COIN);
    assert(penalty_duel_trigger_coin(&model, PENALTY_DUEL_HOST, 1, &config));
    assert(model.phase == PENALTY_DUEL_WAIT_COMMITS);
    assert(model.first_shooter == PENALTY_DUEL_GUEST);
    assert(model.current_shooter == PENALTY_DUEL_GUEST);

    penalty_duel_init(&model, MATCH_ID, PENALTY_NORMAL);
    assert(penalty_duel_trigger_coin(&model, PENALTY_DUEL_HOST, 2, &config));
    assert(model.first_shooter == PENALTY_DUEL_HOST);
}

static void commits_hide_choices_and_reveals_are_immutable(void) {
    penalty_duel_model_t model;
    penalty_duel_init(&model, MATCH_ID, PENALTY_NORMAL);
    penalty_duel_round_config_t config = round_config(0, 99);
    assert(penalty_duel_trigger_coin(&model, PENALTY_DUEL_HOST, 2, &config));

    penalty_duel_action_t host = action(PENALTY_TOP_LEFT, 68, 0x1111222233334444ULL);
    penalty_duel_action_t guest = action(PENALTY_BOTTOM_RIGHT, 68, 0x5555666677778888ULL);
    uint8_t host_commit[PENALTY_DUEL_COMMIT_BYTES];
    uint8_t guest_commit[PENALTY_DUEL_COMMIT_BYTES];
    penalty_duel_make_commit(&model, PENALTY_DUEL_HOST, &host, host_commit);
    penalty_duel_make_commit(&model, PENALTY_DUEL_GUEST, &guest, guest_commit);
    static const uint8_t expected_host_commit[PENALTY_DUEL_COMMIT_BYTES] = {
        0x45, 0x57, 0xee, 0x77, 0x2b, 0xb0, 0x44, 0x70,
        0xa1, 0x10, 0x10, 0x3d, 0x71, 0x1e, 0x90, 0x79,
        0xf5, 0x15, 0xad, 0x1a, 0xd1, 0xc3, 0xa8, 0x5d,
        0xa0, 0xa2, 0xe3, 0x25, 0x80, 0x5c, 0x94, 0x04,
    };
    assert(memcmp(host_commit, expected_host_commit, sizeof(host_commit)) == 0);

    assert(penalty_duel_commit(&model, PENALTY_DUEL_GUEST, guest_commit));
    penalty_duel_snapshot_t snapshot;
    penalty_duel_export_snapshot(&model, &snapshot);
    assert(snapshot.commit_set[PENALTY_DUEL_GUEST]);
    assert(!snapshot.reveal_set[PENALTY_DUEL_GUEST]);
    assert(memcmp(snapshot.commits[PENALTY_DUEL_GUEST], guest_commit,
                  PENALTY_DUEL_COMMIT_BYTES) == 0);
    assert(!snapshot.random_revealed);
    assert(snapshot.shot_roll == 0 && snapshot.save_roll == 0 &&
           snapshot.random_nonce == 0);

    penalty_duel_action_t tampered = guest;
    tampered.target = PENALTY_TOP_RIGHT;
    assert(!penalty_duel_reveal(&model, PENALTY_DUEL_GUEST, &tampered));
    assert(!model.reveal_set[PENALTY_DUEL_GUEST]);

    assert(penalty_duel_commit(&model, PENALTY_DUEL_HOST, host_commit));
    assert(penalty_duel_reveal(&model, PENALTY_DUEL_HOST, &host));
    assert(penalty_duel_reveal(&model, PENALTY_DUEL_GUEST, &guest));
    assert(model.total_settled == 1);
    penalty_duel_round_result_t locked = model.round_result;

    penalty_duel_export_snapshot(&model, &snapshot);
    assert(snapshot.random_revealed);
    penalty_duel_model_t restored;
    assert(penalty_duel_apply_snapshot(&restored, &snapshot));
    assert(restored.total_settled == 1);
    assert(memcmp(&restored.round_result, &model.round_result,
                  sizeof(model.round_result)) == 0);
    snapshot.shot_roll ^= 1;
    assert(!penalty_duel_apply_snapshot(&restored, &snapshot));

    assert(penalty_duel_reveal(&model, PENALTY_DUEL_GUEST, &guest));
    assert(model.total_settled == 1);
    assert(memcmp(&locked, &model.round_result, sizeof(locked)) == 0);
    tampered.timing_value = 70;
    assert(!penalty_duel_reveal(&model, PENALTY_DUEL_GUEST, &tampered));
    assert(model.total_settled == 1);
}

static penalty_duel_outcome_t resolve_once(uint8_t shooter_timing,
                                           penalty_direction_t shot_target,
                                           uint8_t keeper_timing,
                                           penalty_direction_t keeper_target,
                                           uint8_t shot_roll,
                                           uint8_t save_roll) {
    penalty_duel_model_t model;
    penalty_duel_init(&model, MATCH_ID, PENALTY_NORMAL);
    penalty_duel_round_config_t config = round_config(shot_roll, save_roll);
    assert(penalty_duel_trigger_coin(&model, PENALTY_DUEL_HOST, 2, &config));
    commit_and_reveal(&model,
        action(shot_target, shooter_timing, 1),
        action(keeper_target, keeper_timing, 2));
    return model.round_result.outcome;
}

static void pvp_probability_boundaries(void) {
    assert(resolve_once(29, PENALTY_TOP_LEFT, 68, PENALTY_TOP_LEFT, 0, 99) ==
           PENALTY_DUEL_MISS_WEAK);
    assert(resolve_once(91, PENALTY_TOP_LEFT, 68, PENALTY_TOP_LEFT, 0, 99) ==
           PENALTY_DUEL_MISS_HIGH);
    assert(resolve_once(68, PENALTY_TOP_LEFT, 68, PENALTY_TOP_LEFT, 94, 99) ==
           PENALTY_DUEL_GOAL);
    assert(resolve_once(68, PENALTY_TOP_LEFT, 68, PENALTY_TOP_LEFT, 95, 99) ==
           PENALTY_DUEL_MISS_ACCURACY);
    assert(resolve_once(65, PENALTY_TOP_LEFT, 68, PENALTY_TOP_LEFT, 79, 99) ==
           PENALTY_DUEL_GOAL);
    assert(resolve_once(65, PENALTY_TOP_LEFT, 68, PENALTY_TOP_LEFT, 80, 99) ==
           PENALTY_DUEL_MISS_ACCURACY);
    assert(resolve_once(50, PENALTY_TOP_LEFT, 68, PENALTY_TOP_LEFT, 34, 99) ==
           PENALTY_DUEL_GOAL);
    assert(resolve_once(50, PENALTY_TOP_LEFT, 68, PENALTY_TOP_LEFT, 35, 99) ==
           PENALTY_DUEL_MISS_ACCURACY);

    assert(resolve_once(68, PENALTY_TOP_LEFT, 68, PENALTY_TOP_LEFT, 0, 89) ==
           PENALTY_DUEL_SAVE);
    assert(resolve_once(68, PENALTY_TOP_LEFT, 68, PENALTY_TOP_LEFT, 0, 90) ==
           PENALTY_DUEL_GOAL);
    assert(resolve_once(68, PENALTY_TOP_LEFT, 68, PENALTY_BOTTOM_LEFT, 0, 34) ==
           PENALTY_DUEL_SAVE);
    assert(resolve_once(68, PENALTY_TOP_LEFT, 68, PENALTY_BOTTOM_LEFT, 0, 35) ==
           PENALTY_DUEL_GOAL);
    assert(resolve_once(68, PENALTY_TOP_LEFT, 68, PENALTY_TOP_RIGHT, 0, 0) ==
           PENALTY_DUEL_GOAL);
}

static void play_kick(penalty_duel_model_t *model, bool score, uint64_t nonce) {
    penalty_duel_peer_t shooter = model->current_shooter;
    penalty_duel_action_t shot = action(PENALTY_TOP_LEFT, score ? 68 : 10, nonce);
    penalty_duel_action_t keep = action(PENALTY_TOP_RIGHT, 68, nonce + 1);
    if (shooter == PENALTY_DUEL_HOST) commit_and_reveal(model, shot, keep);
    else commit_and_reveal(model, keep, shot);
}

static void advance(penalty_duel_model_t *model, uint8_t shot_roll, uint8_t save_roll) {
    penalty_duel_round_config_t next = round_config(shot_roll, save_roll);
    assert(penalty_duel_advance(model, PENALTY_DUEL_HOST, &next));
}

static void invalid_next_round_does_not_mutate_state(void) {
    penalty_duel_model_t model;
    penalty_duel_round_config_t config = round_config(0, 99);
    penalty_duel_init(&model, MATCH_ID, PENALTY_NORMAL);
    assert(penalty_duel_trigger_coin(&model, PENALTY_DUEL_HOST, 2, &config));
    play_kick(&model, true, 90);
    penalty_duel_model_t before = model;
    penalty_duel_round_config_t invalid = config;
    invalid.shooter_meter.green_low = 10;
    assert(!penalty_duel_advance(&model, PENALTY_DUEL_HOST, &invalid));
    assert(memcmp(&before, &model, sizeof(model)) == 0);
}

static void regular_match_can_end_early(void) {
    penalty_duel_model_t model;
    penalty_duel_round_config_t config = round_config(0, 99);
    penalty_duel_init(&model, MATCH_ID, PENALTY_NORMAL);
    assert(penalty_duel_trigger_coin(&model, PENALTY_DUEL_HOST, 2, &config));

    for (unsigned kick = 0; kick < 6; ++kick) {
        bool host_scores = model.current_shooter == PENALTY_DUEL_HOST;
        play_kick(&model, host_scores, 100 + kick * 2);
        if (kick < 5) advance(&model, 0, 99);
    }
    assert(model.attempts[PENALTY_DUEL_HOST] == 3);
    assert(model.attempts[PENALTY_DUEL_GUEST] == 3);
    assert(model.goals[PENALTY_DUEL_HOST] == 3);
    assert(model.goals[PENALTY_DUEL_GUEST] == 0);
    advance(&model, 0, 99);
    assert(model.phase == PENALTY_DUEL_COMPLETE);
    assert(model.match_result == PENALTY_DUEL_HOST_WIN);
    assert(model.ended_early);
}

static void tied_regular_match_enters_paired_sudden_death(void) {
    penalty_duel_model_t model;
    penalty_duel_round_config_t config = round_config(0, 99);
    penalty_duel_init(&model, MATCH_ID, PENALTY_NORMAL);
    assert(penalty_duel_trigger_coin(&model, PENALTY_DUEL_HOST, 2, &config));

    for (unsigned kick = 0; kick < 10; ++kick) {
        play_kick(&model, true, 200 + kick * 2);
        advance(&model, 0, 99);
    }
    assert(model.sudden_death);
    assert(model.sudden_death_round == 1);
    assert(model.phase == PENALTY_DUEL_WAIT_COMMITS);
    assert(model.current_shooter == model.first_shooter);

    play_kick(&model, true, 300);
    advance(&model, 0, 99);
    assert(model.phase == PENALTY_DUEL_WAIT_COMMITS);
    play_kick(&model, false, 302);
    advance(&model, 0, 99);
    assert(model.phase == PENALTY_DUEL_COMPLETE);
    assert(model.match_result == PENALTY_DUEL_HOST_WIN);
}

static void abandoned_match_has_no_winner(void) {
    penalty_duel_model_t model;
    penalty_duel_init(&model, MATCH_ID, PENALTY_HARD);
    assert(penalty_duel_abort(&model, PENALTY_DUEL_DISCONNECT_TIMEOUT));
    assert(model.phase == PENALTY_DUEL_ABORTED);
    assert(model.match_result == PENALTY_DUEL_NO_CONTEST);
    assert(model.abort_reason == PENALTY_DUEL_DISCONNECT_TIMEOUT);
}

int main(void) {
    creator_controls_coin_without_fixing_roles();
    commits_hide_choices_and_reveals_are_immutable();
    pvp_probability_boundaries();
    invalid_next_round_does_not_mutate_state();
    regular_match_can_end_early();
    tied_regular_match_enters_paired_sudden_death();
    abandoned_match_has_no_winner();
    puts("penalty duel model tests passed");
    return 0;
}
