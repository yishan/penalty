#pragma once

#include "penalty_model.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PENALTY_DUEL_MATCH_ID_BYTES 16
#define PENALTY_DUEL_COMMIT_BYTES 32
#define PENALTY_DUEL_REGULAR_KICKS 5

typedef enum {
    PENALTY_DUEL_HOST = 0,
    PENALTY_DUEL_GUEST = 1,
    PENALTY_DUEL_PEER_COUNT = 2,
} penalty_duel_peer_t;

typedef enum {
    PENALTY_DUEL_WAIT_COIN,
    PENALTY_DUEL_WAIT_COMMITS,
    PENALTY_DUEL_WAIT_REVEALS,
    PENALTY_DUEL_ROUND_RESULT,
    PENALTY_DUEL_COMPLETE,
    PENALTY_DUEL_ABORTED,
    PENALTY_DUEL_PROTOCOL_ERROR,
} penalty_duel_phase_t;

typedef enum {
    PENALTY_DUEL_RESULT_NONE,
    PENALTY_DUEL_GOAL,
    PENALTY_DUEL_SAVE,
    PENALTY_DUEL_MISS_WEAK,
    PENALTY_DUEL_MISS_HIGH,
    PENALTY_DUEL_MISS_ACCURACY,
} penalty_duel_outcome_t;

typedef enum {
    PENALTY_DUEL_MATCH_IN_PROGRESS,
    PENALTY_DUEL_HOST_WIN,
    PENALTY_DUEL_GUEST_WIN,
    PENALTY_DUEL_NO_CONTEST,
} penalty_duel_match_result_t;

typedef enum {
    PENALTY_DUEL_ABORT_NONE,
    PENALTY_DUEL_DISCONNECT_TIMEOUT,
    PENALTY_DUEL_USER_CANCELLED,
    PENALTY_DUEL_SECURITY_REJECTED,
    PENALTY_DUEL_PROTOCOL_FAILED,
} penalty_duel_abort_reason_t;

typedef struct {
    uint8_t green_low;
    uint8_t green_high;
    uint8_t perfect_low;
} penalty_duel_meter_t;

typedef struct {
    penalty_duel_meter_t shooter_meter;
    penalty_duel_meter_t keeper_meter;
    uint8_t shot_roll;
    uint8_t save_roll;
    uint64_t random_nonce;
} penalty_duel_round_config_t;

typedef struct {
    penalty_direction_t target;
    uint8_t timing_value;
    uint64_t nonce;
} penalty_duel_action_t;

typedef struct {
    uint32_t result_id;
    penalty_duel_peer_t shooter;
    penalty_duel_outcome_t outcome;
    bool shot_on_target;
    bool scored;
} penalty_duel_round_result_t;

typedef struct {
    uint8_t match_id[PENALTY_DUEL_MATCH_ID_BYTES];
    penalty_duel_phase_t phase;
    penalty_difficulty_t difficulty;
    penalty_duel_peer_t first_shooter;
    penalty_duel_peer_t current_shooter;
    uint32_t round_id;
    uint32_t attempts[PENALTY_DUEL_PEER_COUNT];
    uint32_t goals[PENALTY_DUEL_PEER_COUNT];
    uint32_t total_settled;
    uint32_t sudden_death_round;
    bool sudden_death;
    bool ended_early;
    bool pair_done[PENALTY_DUEL_PEER_COUNT];
    bool pair_scored[PENALTY_DUEL_PEER_COUNT];
    bool commit_set[PENALTY_DUEL_PEER_COUNT];
    uint8_t commits[PENALTY_DUEL_PEER_COUNT][PENALTY_DUEL_COMMIT_BYTES];
    bool reveal_set[PENALTY_DUEL_PEER_COUNT];
    penalty_duel_action_t actions[PENALTY_DUEL_PEER_COUNT];
    penalty_duel_round_config_t round_config;
    uint8_t random_commit[PENALTY_DUEL_COMMIT_BYTES];
    penalty_duel_round_result_t round_result;
    penalty_duel_match_result_t match_result;
    penalty_duel_abort_reason_t abort_reason;
} penalty_duel_model_t;

/* A network snapshot deliberately omits unrevealed action fields and nonces. */
typedef struct {
    uint8_t match_id[PENALTY_DUEL_MATCH_ID_BYTES];
    penalty_duel_phase_t phase;
    penalty_difficulty_t difficulty;
    penalty_duel_peer_t first_shooter;
    penalty_duel_peer_t current_shooter;
    uint32_t round_id;
    uint32_t attempts[PENALTY_DUEL_PEER_COUNT];
    uint32_t goals[PENALTY_DUEL_PEER_COUNT];
    uint32_t total_settled;
    uint32_t sudden_death_round;
    bool sudden_death;
    bool ended_early;
    bool pair_done[PENALTY_DUEL_PEER_COUNT];
    bool pair_scored[PENALTY_DUEL_PEER_COUNT];
    bool commit_set[PENALTY_DUEL_PEER_COUNT];
    uint8_t commits[PENALTY_DUEL_PEER_COUNT][PENALTY_DUEL_COMMIT_BYTES];
    bool reveal_set[PENALTY_DUEL_PEER_COUNT];
    penalty_duel_meter_t shooter_meter;
    penalty_duel_meter_t keeper_meter;
    uint8_t random_commit[PENALTY_DUEL_COMMIT_BYTES];
    bool random_revealed;
    uint8_t shot_roll;
    uint8_t save_roll;
    uint64_t random_nonce;
    penalty_duel_round_result_t round_result;
    penalty_duel_match_result_t match_result;
    penalty_duel_abort_reason_t abort_reason;
} penalty_duel_snapshot_t;

void penalty_duel_init(penalty_duel_model_t *model,
                       const uint8_t match_id[PENALTY_DUEL_MATCH_ID_BYTES],
                       penalty_difficulty_t difficulty);
bool penalty_duel_trigger_coin(penalty_duel_model_t *model,
                               penalty_duel_peer_t actor,
                               uint32_t random_value,
                               const penalty_duel_round_config_t *config);
void penalty_duel_make_commit(const penalty_duel_model_t *model,
                              penalty_duel_peer_t peer,
                              const penalty_duel_action_t *action,
                              uint8_t out[PENALTY_DUEL_COMMIT_BYTES]);
void penalty_duel_make_random_commit(const penalty_duel_model_t *model,
                                     const penalty_duel_round_config_t *config,
                                     uint8_t out[PENALTY_DUEL_COMMIT_BYTES]);
bool penalty_duel_commit(penalty_duel_model_t *model,
                         penalty_duel_peer_t peer,
                         const uint8_t digest[PENALTY_DUEL_COMMIT_BYTES]);
bool penalty_duel_reveal(penalty_duel_model_t *model,
                         penalty_duel_peer_t peer,
                         const penalty_duel_action_t *action);
bool penalty_duel_advance(penalty_duel_model_t *model,
                          penalty_duel_peer_t actor,
                          const penalty_duel_round_config_t *next_config);
bool penalty_duel_abort(penalty_duel_model_t *model,
                        penalty_duel_abort_reason_t reason);
void penalty_duel_export_snapshot(const penalty_duel_model_t *model,
                                  penalty_duel_snapshot_t *snapshot);
bool penalty_duel_apply_snapshot(penalty_duel_model_t *model,
                                 const penalty_duel_snapshot_t *snapshot);
