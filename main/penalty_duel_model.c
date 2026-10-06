#include "penalty_duel_model.h"

#include <limits.h>
#include <string.h>

typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t block[64];
    size_t block_len;
} sha256_ctx_t;

static uint32_t rotate_right(uint32_t value, unsigned count) {
    return (value >> count) | (value << (32u - count));
}

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t block[64]) {
    static const uint32_t constants[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    };
    uint32_t words[64];
    for (unsigned i = 0; i < 16; ++i) {
        words[i] = ((uint32_t)block[i * 4] << 24) |
                   ((uint32_t)block[i * 4 + 1] << 16) |
                   ((uint32_t)block[i * 4 + 2] << 8) |
                   block[i * 4 + 3];
    }
    for (unsigned i = 16; i < 64; ++i) {
        uint32_t s0 = rotate_right(words[i - 15], 7) ^
                      rotate_right(words[i - 15], 18) ^ (words[i - 15] >> 3);
        uint32_t s1 = rotate_right(words[i - 2], 17) ^
                      rotate_right(words[i - 2], 19) ^ (words[i - 2] >> 10);
        words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }
    uint32_t a = ctx->state[0], b = ctx->state[1];
    uint32_t c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5];
    uint32_t g = ctx->state[6], h = ctx->state[7];
    for (unsigned i = 0; i < 64; ++i) {
        uint32_t sum1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
        uint32_t choose = (e & f) ^ (~e & g);
        uint32_t temp1 = h + sum1 + choose + constants[i] + words[i];
        uint32_t sum0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = sum0 + majority;
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }
    ctx->state[0] += a; ctx->state[1] += b;
    ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f;
    ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(sha256_ctx_t *ctx) {
    *ctx = (sha256_ctx_t){
        .state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                  0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19},
    };
}

static void sha256_update(sha256_ctx_t *ctx, const void *data, size_t length) {
    const uint8_t *bytes = data;
    ctx->bit_count += (uint64_t)length * 8u;
    while (length) {
        size_t available = sizeof(ctx->block) - ctx->block_len;
        size_t take = length < available ? length : available;
        memcpy(ctx->block + ctx->block_len, bytes, take);
        ctx->block_len += take;
        bytes += take;
        length -= take;
        if (ctx->block_len == sizeof(ctx->block)) {
            sha256_transform(ctx, ctx->block);
            ctx->block_len = 0;
        }
    }
}

static void sha256_finish(sha256_ctx_t *ctx, uint8_t digest[32]) {
    uint64_t bit_count = ctx->bit_count;
    uint8_t marker = 0x80;
    uint8_t zero = 0;
    sha256_update(ctx, &marker, 1);
    while (ctx->block_len != 56) sha256_update(ctx, &zero, 1);
    uint8_t length_bytes[8];
    for (unsigned i = 0; i < 8; ++i)
        length_bytes[7 - i] = (uint8_t)(bit_count >> (i * 8));
    sha256_update(ctx, length_bytes, sizeof(length_bytes));
    for (unsigned i = 0; i < 8; ++i) {
        digest[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)ctx->state[i];
    }
}

static bool valid_peer(penalty_duel_peer_t peer) {
    return (unsigned)peer < PENALTY_DUEL_PEER_COUNT;
}

static bool valid_meter(const penalty_duel_meter_t *meter) {
    return meter && meter->green_low >= 30 && meter->green_high <= 90 &&
           meter->green_low <= meter->green_high &&
           meter->perfect_low >= meter->green_low &&
           meter->perfect_low + 1u <= meter->green_high;
}

static bool valid_config(const penalty_duel_round_config_t *config) {
    return config && valid_meter(&config->shooter_meter) &&
           valid_meter(&config->keeper_meter) && config->shot_roll < 100 &&
           config->save_roll < 100;
}

static bool valid_action(const penalty_duel_action_t *action) {
    return action && (unsigned)action->target < PENALTY_TARGET_COUNT &&
           action->timing_value <= 100;
}

static void hash_u32(sha256_ctx_t *ctx, uint32_t value) {
    uint8_t bytes[4] = {
        (uint8_t)value, (uint8_t)(value >> 8),
        (uint8_t)(value >> 16), (uint8_t)(value >> 24),
    };
    sha256_update(ctx, bytes, sizeof(bytes));
}

static void hash_u64(sha256_ctx_t *ctx, uint64_t value) {
    uint8_t bytes[8];
    for (unsigned i = 0; i < 8; ++i) bytes[i] = (uint8_t)(value >> (i * 8));
    sha256_update(ctx, bytes, sizeof(bytes));
}

void penalty_duel_make_commit(const penalty_duel_model_t *model,
                              penalty_duel_peer_t peer,
                              const penalty_duel_action_t *action,
                              uint8_t out[PENALTY_DUEL_COMMIT_BYTES]) {
    static const uint8_t domain[] = "PENALTY-DUEL-ACTION-V1";
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, domain, sizeof(domain) - 1);
    sha256_update(&ctx, model->match_id, sizeof(model->match_id));
    hash_u32(&ctx, model->round_id);
    uint8_t fields[4] = {
        (uint8_t)peer,
        peer == model->current_shooter ? PENALTY_ROLE_SHOOTER : PENALTY_ROLE_KEEPER,
        (uint8_t)action->target,
        action->timing_value,
    };
    sha256_update(&ctx, fields, sizeof(fields));
    hash_u64(&ctx, action->nonce);
    sha256_finish(&ctx, out);
}

void penalty_duel_make_random_commit(const penalty_duel_model_t *model,
                                     const penalty_duel_round_config_t *config,
                                     uint8_t out[PENALTY_DUEL_COMMIT_BYTES]) {
    static const uint8_t domain[] = "PENALTY-DUEL-RANDOM-V1";
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, domain, sizeof(domain) - 1);
    sha256_update(&ctx, model->match_id, sizeof(model->match_id));
    hash_u32(&ctx, model->round_id);
    uint8_t rolls[2] = {config->shot_roll, config->save_roll};
    sha256_update(&ctx, rolls, sizeof(rolls));
    hash_u64(&ctx, config->random_nonce);
    sha256_finish(&ctx, out);
}

void penalty_duel_init(penalty_duel_model_t *model,
                       const uint8_t match_id[PENALTY_DUEL_MATCH_ID_BYTES],
                       penalty_difficulty_t difficulty) {
    memset(model, 0, sizeof(*model));
    memcpy(model->match_id, match_id, PENALTY_DUEL_MATCH_ID_BYTES);
    model->phase = PENALTY_DUEL_WAIT_COIN;
    model->difficulty = (unsigned)difficulty <= PENALTY_HARD ? difficulty : PENALTY_NORMAL;
    model->match_result = PENALTY_DUEL_MATCH_IN_PROGRESS;
}

static void start_round(penalty_duel_model_t *model,
                        const penalty_duel_round_config_t *config) {
    ++model->round_id;
    model->round_config = *config;
    penalty_duel_make_random_commit(model, config, model->random_commit);
    memset(model->commit_set, 0, sizeof(model->commit_set));
    memset(model->commits, 0, sizeof(model->commits));
    memset(model->reveal_set, 0, sizeof(model->reveal_set));
    memset(model->actions, 0, sizeof(model->actions));
    memset(&model->round_result, 0, sizeof(model->round_result));
    model->phase = PENALTY_DUEL_WAIT_COMMITS;
}

bool penalty_duel_trigger_coin(penalty_duel_model_t *model,
                               penalty_duel_peer_t actor,
                               uint32_t random_value,
                               const penalty_duel_round_config_t *config) {
    if (!model || actor != PENALTY_DUEL_HOST ||
        model->phase != PENALTY_DUEL_WAIT_COIN || !valid_config(config)) return false;
    model->first_shooter = (random_value & 1u) ? PENALTY_DUEL_GUEST : PENALTY_DUEL_HOST;
    model->current_shooter = model->first_shooter;
    start_round(model, config);
    return true;
}

bool penalty_duel_commit(penalty_duel_model_t *model,
                         penalty_duel_peer_t peer,
                         const uint8_t digest[PENALTY_DUEL_COMMIT_BYTES]) {
    if (!model || !valid_peer(peer) || !digest) return false;
    if (model->commit_set[peer])
        return memcmp(model->commits[peer], digest, PENALTY_DUEL_COMMIT_BYTES) == 0;
    if (model->phase != PENALTY_DUEL_WAIT_COMMITS) return false;
    memcpy(model->commits[peer], digest, PENALTY_DUEL_COMMIT_BYTES);
    model->commit_set[peer] = true;
    if (model->commit_set[PENALTY_DUEL_HOST] && model->commit_set[PENALTY_DUEL_GUEST])
        model->phase = PENALTY_DUEL_WAIT_REVEALS;
    return true;
}

static bool timing_is_perfect(uint8_t timing, const penalty_duel_meter_t *meter) {
    return timing >= meter->perfect_low && timing <= meter->perfect_low + 1u;
}

static bool timing_is_green(uint8_t timing, const penalty_duel_meter_t *meter) {
    return timing >= meter->green_low && timing <= meter->green_high;
}

static uint8_t shot_chance(uint8_t timing, const penalty_duel_meter_t *meter) {
    if (timing_is_perfect(timing, meter)) return 95;
    if (timing_is_green(timing, meter)) return 80;
    return 35;
}

static uint8_t save_chance(const penalty_duel_action_t *shot,
                           const penalty_duel_action_t *keeper,
                           const penalty_duel_meter_t *meter) {
    bool exact = shot->target == keeper->target;
    bool same_column = penalty_target_column(shot->target) ==
                       penalty_target_column(keeper->target);
    if (!same_column || keeper->timing_value < 30 || keeper->timing_value > 90) return 0;
    if (timing_is_perfect(keeper->timing_value, meter)) return exact ? 90 : 35;
    if (timing_is_green(keeper->timing_value, meter)) return exact ? 65 : 20;
    return exact ? 25 : 5;
}

static void resolve_round(penalty_duel_model_t *model) {
    penalty_duel_peer_t shooter = model->current_shooter;
    penalty_duel_peer_t keeper = shooter == PENALTY_DUEL_HOST
        ? PENALTY_DUEL_GUEST : PENALTY_DUEL_HOST;
    const penalty_duel_action_t *shot = &model->actions[shooter];
    const penalty_duel_action_t *keep = &model->actions[keeper];
    penalty_duel_outcome_t outcome;
    bool on_target = false;
    if (shot->timing_value < 30) outcome = PENALTY_DUEL_MISS_WEAK;
    else if (shot->timing_value > 90) outcome = PENALTY_DUEL_MISS_HIGH;
    else if (model->round_config.shot_roll >=
             shot_chance(shot->timing_value, &model->round_config.shooter_meter))
        outcome = PENALTY_DUEL_MISS_ACCURACY;
    else {
        on_target = true;
        outcome = model->round_config.save_roll <
                  save_chance(shot, keep, &model->round_config.keeper_meter)
            ? PENALTY_DUEL_SAVE : PENALTY_DUEL_GOAL;
    }
    bool scored = outcome == PENALTY_DUEL_GOAL;
    model->round_result = (penalty_duel_round_result_t){
        .result_id = model->round_id,
        .shooter = shooter,
        .outcome = outcome,
        .shot_on_target = on_target,
        .scored = scored,
    };
    ++model->attempts[shooter];
    if (scored) ++model->goals[shooter];
    if (model->sudden_death) {
        model->pair_done[shooter] = true;
        model->pair_scored[shooter] = scored;
    }
    ++model->total_settled;
    model->phase = PENALTY_DUEL_ROUND_RESULT;
}

bool penalty_duel_reveal(penalty_duel_model_t *model,
                         penalty_duel_peer_t peer,
                         const penalty_duel_action_t *action) {
    if (!model || !valid_peer(peer) || !valid_action(action)) return false;
    if (model->reveal_set[peer])
        return model->actions[peer].target == action->target &&
               model->actions[peer].timing_value == action->timing_value &&
               model->actions[peer].nonce == action->nonce;
    if (model->phase != PENALTY_DUEL_WAIT_REVEALS || !model->commit_set[peer]) return false;
    uint8_t digest[PENALTY_DUEL_COMMIT_BYTES];
    penalty_duel_make_commit(model, peer, action, digest);
    if (memcmp(digest, model->commits[peer], sizeof(digest)) != 0) return false;
    model->actions[peer] = *action;
    model->reveal_set[peer] = true;
    if (model->reveal_set[PENALTY_DUEL_HOST] && model->reveal_set[PENALTY_DUEL_GUEST])
        resolve_round(model);
    return true;
}

static void complete(penalty_duel_model_t *model, penalty_duel_peer_t winner,
                     bool early) {
    model->match_result = winner == PENALTY_DUEL_HOST
        ? PENALTY_DUEL_HOST_WIN : PENALTY_DUEL_GUEST_WIN;
    model->ended_early = early;
    model->phase = PENALTY_DUEL_COMPLETE;
}

static bool decide_after_round(penalty_duel_model_t *model) {
    if (model->sudden_death) {
        if (!model->pair_done[PENALTY_DUEL_HOST] ||
            !model->pair_done[PENALTY_DUEL_GUEST]) {
            model->current_shooter = model->current_shooter == PENALTY_DUEL_HOST
                ? PENALTY_DUEL_GUEST : PENALTY_DUEL_HOST;
            return false;
        }
        if (model->pair_scored[PENALTY_DUEL_HOST] !=
            model->pair_scored[PENALTY_DUEL_GUEST]) {
            complete(model, model->pair_scored[PENALTY_DUEL_HOST]
                ? PENALTY_DUEL_HOST : PENALTY_DUEL_GUEST, false);
            return true;
        }
        ++model->sudden_death_round;
        memset(model->pair_done, 0, sizeof(model->pair_done));
        memset(model->pair_scored, 0, sizeof(model->pair_scored));
        model->current_shooter = model->first_shooter;
        return false;
    }

    uint32_t host_remaining = model->attempts[PENALTY_DUEL_HOST] < PENALTY_DUEL_REGULAR_KICKS
        ? PENALTY_DUEL_REGULAR_KICKS - model->attempts[PENALTY_DUEL_HOST] : 0;
    uint32_t guest_remaining = model->attempts[PENALTY_DUEL_GUEST] < PENALTY_DUEL_REGULAR_KICKS
        ? PENALTY_DUEL_REGULAR_KICKS - model->attempts[PENALTY_DUEL_GUEST] : 0;
    if (model->goals[PENALTY_DUEL_HOST] >
        model->goals[PENALTY_DUEL_GUEST] + guest_remaining) {
        complete(model, PENALTY_DUEL_HOST, true);
        return true;
    }
    if (model->goals[PENALTY_DUEL_GUEST] >
        model->goals[PENALTY_DUEL_HOST] + host_remaining) {
        complete(model, PENALTY_DUEL_GUEST, true);
        return true;
    }
    if (model->attempts[PENALTY_DUEL_HOST] == PENALTY_DUEL_REGULAR_KICKS &&
        model->attempts[PENALTY_DUEL_GUEST] == PENALTY_DUEL_REGULAR_KICKS) {
        if (model->goals[PENALTY_DUEL_HOST] != model->goals[PENALTY_DUEL_GUEST]) {
            complete(model, model->goals[PENALTY_DUEL_HOST] > model->goals[PENALTY_DUEL_GUEST]
                ? PENALTY_DUEL_HOST : PENALTY_DUEL_GUEST, false);
            return true;
        }
        model->sudden_death = true;
        model->sudden_death_round = 1;
        model->current_shooter = model->first_shooter;
        memset(model->pair_done, 0, sizeof(model->pair_done));
        memset(model->pair_scored, 0, sizeof(model->pair_scored));
        return false;
    }
    model->current_shooter = model->current_shooter == PENALTY_DUEL_HOST
        ? PENALTY_DUEL_GUEST : PENALTY_DUEL_HOST;
    return false;
}

bool penalty_duel_advance(penalty_duel_model_t *model,
                          penalty_duel_peer_t actor,
                          const penalty_duel_round_config_t *next_config) {
    if (!model || actor != PENALTY_DUEL_HOST ||
        model->phase != PENALTY_DUEL_ROUND_RESULT || !valid_config(next_config)) return false;
    if (decide_after_round(model)) return true;
    start_round(model, next_config);
    return true;
}

bool penalty_duel_abort(penalty_duel_model_t *model,
                        penalty_duel_abort_reason_t reason) {
    if (!model || reason == PENALTY_DUEL_ABORT_NONE ||
        model->phase == PENALTY_DUEL_COMPLETE || model->phase == PENALTY_DUEL_ABORTED)
        return false;
    model->abort_reason = reason;
    model->match_result = PENALTY_DUEL_NO_CONTEST;
    model->phase = PENALTY_DUEL_ABORTED;
    return true;
}

void penalty_duel_export_snapshot(const penalty_duel_model_t *model,
                                  penalty_duel_snapshot_t *snapshot) {
    memset(snapshot, 0, sizeof(*snapshot));
    memcpy(snapshot->match_id, model->match_id, sizeof(snapshot->match_id));
    snapshot->phase = model->phase;
    snapshot->difficulty = model->difficulty;
    snapshot->first_shooter = model->first_shooter;
    snapshot->current_shooter = model->current_shooter;
    snapshot->round_id = model->round_id;
    memcpy(snapshot->attempts, model->attempts, sizeof(snapshot->attempts));
    memcpy(snapshot->goals, model->goals, sizeof(snapshot->goals));
    snapshot->total_settled = model->total_settled;
    snapshot->sudden_death_round = model->sudden_death_round;
    snapshot->sudden_death = model->sudden_death;
    snapshot->ended_early = model->ended_early;
    memcpy(snapshot->pair_done, model->pair_done, sizeof(snapshot->pair_done));
    memcpy(snapshot->pair_scored, model->pair_scored, sizeof(snapshot->pair_scored));
    memcpy(snapshot->commit_set, model->commit_set, sizeof(snapshot->commit_set));
    memcpy(snapshot->commits, model->commits, sizeof(snapshot->commits));
    memcpy(snapshot->reveal_set, model->reveal_set, sizeof(snapshot->reveal_set));
    snapshot->shooter_meter = model->round_config.shooter_meter;
    snapshot->keeper_meter = model->round_config.keeper_meter;
    memcpy(snapshot->random_commit, model->random_commit, sizeof(snapshot->random_commit));
    snapshot->random_revealed = model->phase == PENALTY_DUEL_ROUND_RESULT ||
                                model->phase == PENALTY_DUEL_COMPLETE;
    if (snapshot->random_revealed) {
        snapshot->shot_roll = model->round_config.shot_roll;
        snapshot->save_roll = model->round_config.save_roll;
        snapshot->random_nonce = model->round_config.random_nonce;
    }
    snapshot->round_result = model->round_result;
    snapshot->match_result = model->match_result;
    snapshot->abort_reason = model->abort_reason;
}

bool penalty_duel_apply_snapshot(penalty_duel_model_t *model,
                                 const penalty_duel_snapshot_t *snapshot) {
    if (!model || !snapshot || snapshot->phase > PENALTY_DUEL_PROTOCOL_ERROR ||
        (unsigned)snapshot->difficulty > PENALTY_HARD ||
        !valid_peer(snapshot->first_shooter) || !valid_peer(snapshot->current_shooter) ||
        (snapshot->phase != PENALTY_DUEL_WAIT_COIN &&
         snapshot->phase != PENALTY_DUEL_ABORTED &&
         (!valid_meter(&snapshot->shooter_meter) || !valid_meter(&snapshot->keeper_meter))))
        return false;
    penalty_duel_round_config_t config = {
        .shooter_meter = snapshot->shooter_meter,
        .keeper_meter = snapshot->keeper_meter,
        .shot_roll = snapshot->shot_roll,
        .save_roll = snapshot->save_roll,
        .random_nonce = snapshot->random_nonce,
    };
    if (snapshot->random_revealed) {
        penalty_duel_model_t verifier = {0};
        memcpy(verifier.match_id, snapshot->match_id, sizeof(verifier.match_id));
        verifier.round_id = snapshot->round_id;
        uint8_t digest[PENALTY_DUEL_COMMIT_BYTES];
        penalty_duel_make_random_commit(&verifier, &config, digest);
        if (memcmp(digest, snapshot->random_commit, sizeof(digest)) != 0) return false;
    } else if (snapshot->phase == PENALTY_DUEL_ROUND_RESULT ||
               snapshot->phase == PENALTY_DUEL_COMPLETE) return false;
    memset(model, 0, sizeof(*model));
    memcpy(model->match_id, snapshot->match_id, sizeof(model->match_id));
    model->phase = snapshot->phase;
    model->difficulty = snapshot->difficulty;
    model->first_shooter = snapshot->first_shooter;
    model->current_shooter = snapshot->current_shooter;
    model->round_id = snapshot->round_id;
    memcpy(model->attempts, snapshot->attempts, sizeof(model->attempts));
    memcpy(model->goals, snapshot->goals, sizeof(model->goals));
    model->total_settled = snapshot->total_settled;
    model->sudden_death_round = snapshot->sudden_death_round;
    model->sudden_death = snapshot->sudden_death;
    model->ended_early = snapshot->ended_early;
    memcpy(model->pair_done, snapshot->pair_done, sizeof(model->pair_done));
    memcpy(model->pair_scored, snapshot->pair_scored, sizeof(model->pair_scored));
    memcpy(model->commit_set, snapshot->commit_set, sizeof(model->commit_set));
    memcpy(model->commits, snapshot->commits, sizeof(model->commits));
    memcpy(model->reveal_set, snapshot->reveal_set, sizeof(model->reveal_set));
    model->round_config = config;
    memcpy(model->random_commit, snapshot->random_commit, sizeof(model->random_commit));
    model->round_result = snapshot->round_result;
    model->match_result = snapshot->match_result;
    model->abort_reason = snapshot->abort_reason;
    return true;
}
