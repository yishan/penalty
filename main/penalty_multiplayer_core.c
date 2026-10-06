#include "penalty_multiplayer_core.h"

#include <string.h>
#include <limits.h>

#define PREFIX 20u /* Full match ID + round ID; never native structs on wire. */
#define OPEN_BYTES 60u
#define RESULT_BYTES 55u

static void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
}
static uint32_t get32(const uint8_t *p) {
    uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i) v |= (uint32_t)p[i] << (8 * i);
    return v;
}
static void put64(uint8_t *p, uint64_t v) {
    for (unsigned i = 0; i < 8; ++i) p[i] = (uint8_t)(v >> (8 * i));
}
static uint64_t get64(const uint8_t *p) {
    uint64_t v = 0;
    for (unsigned i = 0; i < 8; ++i) v |= (uint64_t)p[i] << (8 * i);
    return v;
}
static uint32_t random32(penalty_mp_core_t *c) {
    if (c->random_fn) return c->random_fn();
    c->random ^= c->random << 13;
    c->random ^= c->random >> 17;
    c->random ^= c->random << 5;
    return c->random;
}
static uint64_t random64(penalty_mp_core_t *c) {
    uint64_t high = random32(c);
    return (high << 32) | random32(c);
}
static void fail(penalty_mp_core_t *c) {
    c->phase = PENALTY_MP_FAILED;
    penalty_duel_abort(&c->duel, PENALTY_DUEL_PROTOCOL_FAILED);
    c->duel.phase=PENALTY_DUEL_PROTOCOL_ERROR;
    c->duel.match_result=PENALTY_DUEL_NO_CONTEST;
    c->duel.abort_reason=PENALTY_DUEL_PROTOCOL_FAILED;
}
static penalty_mp_packet_t *packet(penalty_mp_core_t *c,
                                    penalty_ble_message_type_t type, uint16_t len) {
    if (c->out_count == 4 || len > PENALTY_BLE_MAX_PAYLOAD_BYTES) {
        fail(c);
        return NULL;
    }
    penalty_mp_packet_t *p = &c->outbox[(c->out_head + c->out_count++) % 4];
    memset(p, 0, sizeof(*p));
    p->type = type;
    p->length = len;
    memcpy(p->bytes, c->duel.match_id, 16);
    put32(p->bytes + 16, c->duel.round_id);
    return p;
}
bool penalty_mp_pop(penalty_mp_core_t *c, penalty_mp_packet_t *p) {
    if (!c || !p || !c->out_count) return false;
    *p = c->outbox[c->out_head];
    c->out_head = (c->out_head + 1) % 4;
    --c->out_count;
    return true;
}
void penalty_mp_init(penalty_mp_core_t *c, penalty_duel_peer_t peer,
                     penalty_difficulty_t difficulty, const uint8_t id[16],
                     uint32_t seed, uint64_t now) {
    memset(c, 0, sizeof(*c));
    c->peer = peer;
    c->phase = PENALTY_MP_SETTINGS;
    c->random = seed ? seed : 1;
    c->since_ms = now;
    penalty_duel_init(&c->duel, id, difficulty);
}
static penalty_duel_meter_t make_meter(penalty_mp_core_t *c) {
    static const uint8_t widths[] = {20, 11, 6};
    unsigned width = widths[c->duel.difficulty];
    uint8_t low = 30 + random32(c) % (62 - width);
    return (penalty_duel_meter_t){low, low + width - 1,
                                  low + random32(c) % (width - 1)};
}
static penalty_duel_round_config_t make_config(penalty_mp_core_t *c) {
    penalty_duel_round_config_t config = {0};
    config.shooter_meter = make_meter(c);
    config.keeper_meter = make_meter(c);
    config.shot_roll = random32(c) % 100;
    config.save_roll = random32(c) % 100;
    config.random_nonce = random64(c);
    return config;
}
static void encode_meter(uint8_t *p, penalty_duel_meter_t m) {
    p[0] = m.green_low; p[1] = m.green_high; p[2] = m.perfect_low;
}
static penalty_duel_meter_t decode_meter(const uint8_t *p) {
    return (penalty_duel_meter_t){p[0], p[1], p[2]};
}
static void encode_action(uint8_t *p, penalty_duel_action_t a) {
    p[0] = a.target; p[1] = a.timing_value; put64(p + 2, a.nonce);
}
static penalty_duel_action_t decode_action(const uint8_t *p) {
    return (penalty_duel_action_t){p[0], p[1], get64(p + 2)};
}
static void opened(penalty_mp_core_t *c, uint64_t now, bool coin) {
    c->phase = coin ? PENALTY_MP_COIN_SPIN : PENALTY_MP_AIM;
    c->since_ms = now;
    c->local_action = (penalty_duel_action_t){.target = PENALTY_TOP_CENTER};
    c->local_locked = c->reveal_sent = false;
    memset(c->next_ready, 0, sizeof(c->next_ready));
}
static void send_open(penalty_mp_core_t *c, uint64_t now, bool coin) {
    penalty_mp_packet_t *p = packet(c, PENALTY_BLE_ROUND_OPEN, OPEN_BYTES);
    if (!p) return;
    p->bytes[20] = c->duel.first_shooter;
    p->bytes[21] = c->duel.difficulty;
    encode_meter(p->bytes + 22, c->duel.round_config.shooter_meter);
    encode_meter(p->bytes + 25, c->duel.round_config.keeper_meter);
    memcpy(p->bytes + 28, c->duel.random_commit, 32);
    opened(c, now, coin);
}
static void result_ready(penalty_mp_core_t *c, uint64_t now) {
    penalty_duel_model_t finish = c->duel;
    if (penalty_duel_advance(&finish, PENALTY_DUEL_HOST, &finish.round_config) &&
        finish.phase == PENALTY_DUEL_COMPLETE) c->duel = finish;
    /* A short relative delay precedes the existing local ball animation. */
    c->phase = PENALTY_MP_ANIMATION;
    c->since_ms = now + 180;
}
static void send_result(penalty_mp_core_t *c, uint64_t now) {
    penalty_mp_packet_t *p = packet(c, PENALTY_BLE_ROUND_RESULT, RESULT_BYTES);
    if (!p) return;
    const penalty_duel_round_config_t *r = &c->duel.round_config;
    p->bytes[20] = r->shot_roll; p->bytes[21] = r->save_roll;
    put64(p->bytes + 22, r->random_nonce);
    encode_action(p->bytes + 30, c->duel.actions[0]);
    encode_action(p->bytes + 40, c->duel.actions[1]);
    p->bytes[50] = c->duel.round_result.outcome;
    put32(p->bytes + 51, c->duel.total_settled);
    result_ready(c, now);
}
static void request_reveals(penalty_mp_core_t *c) {
    if (c->peer != PENALTY_DUEL_HOST || c->reveal_sent ||
        c->duel.phase != PENALTY_DUEL_WAIT_REVEALS) return;
    penalty_mp_packet_t *p = packet(c, PENALTY_BLE_REVEAL_REQUEST, 84);
    if (!p) return;
    memcpy(p->bytes + PREFIX, c->duel.commits, 64);
    if (!penalty_duel_reveal(&c->duel, c->peer, &c->local_action)) {
        fail(c); return;
    }
    c->reveal_sent = true;
}
static penalty_duel_meter_t local_meter(const penalty_mp_core_t *c) {
    return c->peer == c->duel.current_shooter
        ? c->duel.round_config.shooter_meter : c->duel.round_config.keeper_meter;
}
static uint8_t power_at(const penalty_mp_core_t *c, uint64_t at) {
    if (at < c->since_ms || at - c->since_ms >= PENALTY_TIMEOUT_MS) return 0;
    penalty_duel_meter_t m = local_meter(c);
    penalty_shot_t shot = {.green_low = m.green_low, .green_high = m.green_high,
                           .perfect_low = m.perfect_low};
    return penalty_power_at(&shot, at - c->since_ms);
}
static void lock_action(penalty_mp_core_t *c, uint64_t at) {
    c->local_action.timing_value = power_at(c, at);
    c->local_action.nonce = random64(c);
    uint8_t digest[32];
    penalty_duel_make_commit(&c->duel, c->peer, &c->local_action, digest);
    if (!penalty_duel_commit(&c->duel, c->peer, digest)) { fail(c); return; }
    c->local_locked = true;
    c->phase = PENALTY_MP_WAIT;
    if (c->peer == PENALTY_DUEL_GUEST) {
        penalty_mp_packet_t *p = packet(c, PENALTY_BLE_ACTION_COMMIT, 52);
        if (p) memcpy(p->bytes + PREFIX, digest, 32);
    } else request_reveals(c);
}
static void try_next(penalty_mp_core_t *c, uint64_t now) {
    if (c->peer != PENALTY_DUEL_HOST || !c->next_ready[0] || !c->next_ready[1]) return;
    if (c->duel.round_id == UINT32_MAX) { fail(c); return; }
    penalty_duel_round_config_t config = make_config(c);
    if (!penalty_duel_advance(&c->duel, PENALTY_DUEL_HOST, &config)) { fail(c); return; }
    send_open(c, now, false);
}
void penalty_mp_input(penalty_mp_core_t *c, penalty_input_t input, uint64_t at) {
    if (!c) return;
    if (c->phase == PENALTY_MP_SETTINGS) {
        if (c->peer == PENALTY_DUEL_HOST && !c->settings_sent) {
            if (input == PENALTY_INPUT_UP || input == PENALTY_INPUT_DOWN)
                c->duel.difficulty = (c->duel.difficulty +
                    (input == PENALTY_INPUT_UP ? 2 : 1)) % 3;
            else if (input == PENALTY_INPUT_OK) {
                penalty_mp_packet_t *p = packet(c, PENALTY_BLE_LOBBY_SETTINGS, 22);
                if (p) { p->bytes[20] = c->duel.difficulty; c->settings_sent = true; }
            }
        } else if (c->peer == PENALTY_DUEL_GUEST && c->settings_sent &&
                   !c->settings_confirmed && input == PENALTY_INPUT_OK) {
            penalty_mp_packet_t *p = packet(c, PENALTY_BLE_LOBBY_SETTINGS, 22);
            if (p) { p->bytes[20] = c->duel.difficulty; p->bytes[21] = 1;
                     c->settings_confirmed = true; }
        }
    } else if (c->phase == PENALTY_MP_COIN && c->peer == PENALTY_DUEL_HOST &&
               input == PENALTY_INPUT_OK) {
        penalty_duel_round_config_t config = make_config(c);
        uint32_t coin = random32(c);
        if (!penalty_duel_trigger_coin(&c->duel, c->peer, coin, &config)) fail(c);
        else send_open(c, at, true);
    } else if (c->phase == PENALTY_MP_AIM) {
        static const penalty_direction_t order[] = {PENALTY_TOP_LEFT, PENALTY_BOTTOM_LEFT,
            PENALTY_TOP_CENTER, PENALTY_BOTTOM_CENTER, PENALTY_TOP_RIGHT, PENALTY_BOTTOM_RIGHT};
        if (input == PENALTY_INPUT_OK) { c->phase = PENALTY_MP_CHARGE; c->since_ms = at; }
        else if (input == PENALTY_INPUT_UP || input == PENALTY_INPUT_DOWN) {
            unsigned index = 0;
            while (order[index] != c->local_action.target) ++index;
            c->local_action.target = order[(index + (input == PENALTY_INPUT_UP ? 5 : 1)) % 6];
        }
    } else if (c->phase == PENALTY_MP_CHARGE && input == PENALTY_INPUT_OK && at >= c->since_ms) {
        lock_action(c, at);
    } else if (c->phase == PENALTY_MP_RESULT && input == PENALTY_INPUT_OK &&
               !c->next_ready[c->peer]) {
        c->next_ready[c->peer] = true;
        if (c->peer == PENALTY_DUEL_GUEST) (void)packet(c, PENALTY_BLE_ANIMATION_START, PREFIX);
        else try_next(c, at);
    }
}
void penalty_mp_tick(penalty_mp_core_t *c, uint64_t now) {
    if (!c) return;
    if (c->phase == PENALTY_MP_RECONNECT) {
        if (now >= c->recovery_deadline) {
            penalty_duel_abort(&c->duel, PENALTY_DUEL_DISCONNECT_TIMEOUT);
            c->duel.phase=PENALTY_DUEL_ABORTED;
            c->duel.match_result=PENALTY_DUEL_NO_CONTEST;
            c->duel.abort_reason=PENALTY_DUEL_DISCONNECT_TIMEOUT;
            c->phase = PENALTY_MP_FAILED; c->out_count = 0;
        }
        return;
    }
    if (now < c->since_ms) return;
    if (c->phase == PENALTY_MP_COIN_SPIN && now - c->since_ms >= PENALTY_COIN_SPIN_MS)
        opened(c, now, false);
    else if (c->phase == PENALTY_MP_CHARGE &&
             now - c->since_ms >= PENALTY_TIMEOUT_MS + 100)
        lock_action(c, c->since_ms + PENALTY_TIMEOUT_MS);
    else if (c->phase == PENALTY_MP_ANIMATION && now - c->since_ms >= PENALTY_FLIGHT_MS) {
        c->phase = c->duel.phase == PENALTY_DUEL_COMPLETE ? PENALTY_MP_DONE : PENALTY_MP_RESULT;
        c->since_ms += PENALTY_FLIGHT_MS;
    }
}
void penalty_mp_pause(penalty_mp_core_t *c, uint64_t now) {
    if (!c || c->phase == PENALTY_MP_FAILED || c->phase == PENALTY_MP_RECONNECT) return;
    c->resume_phase = c->phase;
    if (c->resume_phase == PENALTY_MP_CHARGE || c->resume_phase == PENALTY_MP_COIN_SPIN)
        c->resume_phase = PENALTY_MP_AIM;
    if (c->resume_phase == PENALTY_MP_ANIMATION)
        c->resume_phase = c->duel.phase == PENALTY_DUEL_COMPLETE ? PENALTY_MP_DONE : PENALTY_MP_RESULT;
    if (!c->local_locked) c->local_action = (penalty_duel_action_t){.target=PENALTY_TOP_CENTER};
    c->phase = PENALTY_MP_RECONNECT;
    c->recovery_deadline = now + PENALTY_MP_RECOVERY_MS;
    c->snapshot_applied = false;
    c->out_count = c->out_head = 0;
}
static void send_snapshot(penalty_mp_core_t *c) {
    penalty_duel_snapshot_t s;
    penalty_duel_export_snapshot(&c->duel,&s);
    penalty_mp_packet_t *p = packet(c,PENALTY_BLE_SNAPSHOT,PENALTY_MP_SNAPSHOT_BYTES);
    if (!p) return;
    uint8_t *b = p->bytes;
    b[20]=1; b[21]=c->resume_phase; b[22]=s.difficulty;
    b[23]=s.first_shooter; b[24]=s.current_shooter;
    b[25]=(c->settings_sent ? 1:0)|(c->settings_confirmed ? 2:0)|
        (c->next_ready[0] ? 4:0)|(c->next_ready[1] ? 8:0)|
        (s.sudden_death ? 16:0)|(s.ended_early ? 32:0)|
        (s.pair_done[0] ? 64:0)|(s.pair_done[1] ? 128:0);
    b[26]=s.phase; b[27]=s.match_result;
    put32(b+28,s.total_settled); put32(b+32,s.attempts[0]); put32(b+36,s.attempts[1]);
    put32(b+40,s.goals[0]); put32(b+44,s.goals[1]); put32(b+48,s.sudden_death_round);
    encode_meter(b+52,s.shooter_meter); encode_meter(b+55,s.keeper_meter);
    b[58]=(s.commit_set[0] ? 1:0)|(s.commit_set[1] ? 2:0);
    /* Unsettled reveals are private. Guest verifies reveals from ROUND_RESULT. */
    b[59]=s.random_revealed ? 3 : 0;
    memcpy(b+60,s.commits,64); memcpy(b+124,s.random_commit,32);
    b[156]=s.random_revealed; b[157]=s.shot_roll; b[158]=s.save_roll;
    put64(b+159,s.random_nonce); b[167]=s.round_result.outcome;
    b[168]=(s.round_result.shot_on_target ? 1:0)|(s.round_result.scored ? 2:0)|
        (s.pair_scored[0] ? 4:0)|(s.pair_scored[1] ? 8:0);
    put32(b+169,s.round_result.result_id);
    if (s.random_revealed) {
        encode_action(b+173,c->duel.actions[0]); encode_action(b+183,c->duel.actions[1]);
    }
}
void penalty_mp_link_ready(penalty_mp_core_t *c, uint64_t now) {
    if (!c || c->phase != PENALTY_MP_RECONNECT) return;
    penalty_mp_tick(c,now);
    if (c->phase == PENALTY_MP_RECONNECT && c->peer == PENALTY_DUEL_HOST) send_snapshot(c);
}
void penalty_mp_cancel(penalty_mp_core_t *c) {
    if (!c || c->phase==PENALTY_MP_DONE) return;
    c->out_count = c->out_head = 0;
    (void)packet(c,PENALTY_BLE_ABORT,PREFIX+1);
    c->outbox[c->out_head].bytes[PREFIX]=PENALTY_DUEL_USER_CANCELLED;
    penalty_duel_abort(&c->duel,PENALTY_DUEL_USER_CANCELLED);
    c->duel.phase=PENALTY_DUEL_ABORTED; c->duel.match_result=PENALTY_DUEL_NO_CONTEST;
    c->duel.abort_reason=PENALTY_DUEL_USER_CANCELLED;
    c->phase = PENALTY_MP_FAILED;
}
static bool apply_snapshot(penalty_mp_core_t *c,const penalty_mp_packet_t *p) {
    const uint8_t *b=p->bytes;
    if (p->length != PENALTY_MP_SNAPSHOT_BYTES || b[20]!=1 || b[21]>PENALTY_MP_DONE ||
        b[22]>PENALTY_HARD || b[23]>1 || b[24]>1 || b[26]>PENALTY_DUEL_COMPLETE ||
        b[27]>PENALTY_DUEL_GUEST_WIN || b[58]>3 || b[59]>3 || b[156]>1 ||
        b[167]>PENALTY_DUEL_MISS_ACCURACY || b[168]>15) return false;
    penalty_duel_snapshot_t s={0};
    memcpy(s.match_id,b,16); s.round_id=get32(b+16);
    s.phase=b[26]; s.difficulty=b[22]; s.first_shooter=b[23]; s.current_shooter=b[24];
    s.total_settled=get32(b+28); s.attempts[0]=get32(b+32); s.attempts[1]=get32(b+36);
    s.goals[0]=get32(b+40); s.goals[1]=get32(b+44); s.sudden_death_round=get32(b+48);
    s.sudden_death=(b[25]&16)!=0; s.ended_early=(b[25]&32)!=0;
    for (unsigned i=0;i<2;++i) {
        s.pair_done[i]=(b[25]&(64u<<i))!=0; s.pair_scored[i]=(b[168]&(4u<<i))!=0;
        s.commit_set[i]=(b[58]&(1u<<i))!=0; s.reveal_set[i]=(b[59]&(1u<<i))!=0;
    }
    s.shooter_meter=decode_meter(b+52); s.keeper_meter=decode_meter(b+55);
    memcpy(s.commits,b+60,64); memcpy(s.random_commit,b+124,32);
    s.random_revealed=b[156]; s.shot_roll=b[157]; s.save_roll=b[158]; s.random_nonce=get64(b+159);
    s.round_result=(penalty_duel_round_result_t){.result_id=get32(b+169),
        .shooter=s.current_shooter,.outcome=b[167],.shot_on_target=(b[168]&1)!=0,.scored=(b[168]&2)!=0};
    s.match_result=b[27];
    bool settled=s.random_revealed;
    bool settings=b[21]==PENALTY_MP_SETTINGS || b[21]==PENALTY_MP_COIN;
    bool choosing=b[21]==PENALTY_MP_AIM || b[21]==PENALTY_MP_WAIT;
    bool result=b[21]==PENALTY_MP_RESULT || b[21]==PENALTY_MP_DONE;
    if ((!settings && !choosing && !result) ||
        (settings && (s.round_id || s.phase!=PENALTY_DUEL_WAIT_COIN || settled)) ||
        (choosing && (!s.round_id || settled ||
            (s.phase!=PENALTY_DUEL_WAIT_COMMITS && s.phase!=PENALTY_DUEL_WAIT_REVEALS))) ||
        (result && (!settled || b[58]!=3 || b[59]!=3 || s.round_result.result_id!=s.round_id)) ||
        (settled!=(s.phase==PENALTY_DUEL_ROUND_RESULT || s.phase==PENALTY_DUEL_COMPLETE)) ||
        ((b[21]==PENALTY_MP_DONE)!=(s.phase==PENALTY_DUEL_COMPLETE)) ||
        (s.phase==PENALTY_DUEL_COMPLETE ? s.match_result==PENALTY_DUEL_MATCH_IN_PROGRESS :
            s.match_result!=PENALTY_DUEL_MATCH_IN_PROGRESS) ||
        ((b[25]&2) && !(b[25]&1)) ||
        (s.round_id && s.current_shooter!=(s.round_id%2 ? s.first_shooter : 1-s.first_shooter))) return false;
    if (s.goals[0]>s.attempts[0] || s.goals[1]>s.attempts[1] ||
        (uint64_t)s.attempts[0]+s.attempts[1]!=s.total_settled ||
        s.total_settled<c->duel.total_settled ||
        (uint64_t)s.total_settled> (uint64_t)c->duel.total_settled+1 ||
        s.round_id<c->duel.round_id || (uint64_t)s.round_id>(uint64_t)c->duel.round_id+1 ||
        (s.random_revealed ? s.total_settled!=s.round_id :
            (s.round_id && s.total_settled!=s.round_id-1))) return false;
    bool same = s.round_id==c->duel.round_id;
    if (c->settings_sent && (memcmp(s.match_id,c->duel.match_id,16) || s.difficulty!=c->duel.difficulty)) return false;
    if (same && s.round_id && (memcmp(s.random_commit,c->duel.random_commit,32) ||
        s.first_shooter!=c->duel.first_shooter || s.current_shooter!=c->duel.current_shooter ||
        memcmp(&s.shooter_meter,&c->duel.round_config.shooter_meter,sizeof(s.shooter_meter)) ||
        memcmp(&s.keeper_meter,&c->duel.round_config.keeper_meter,sizeof(s.keeper_meter)))) return false;
    penalty_duel_model_t model;
    if (!penalty_duel_apply_snapshot(&model,&s)) return false;
    if (s.total_settled==c->duel.total_settled) {
        if (memcmp(s.goals,c->duel.goals,sizeof(s.goals)) ||
            memcmp(s.attempts,c->duel.attempts,sizeof(s.attempts))) return false;
    } else {
        /* A result lost in flight is reverified, not blindly accepted as score. */
        if (!same || !s.random_revealed || !c->local_locked) return false;
        penalty_duel_model_t verifier=c->duel;
        verifier.phase=PENALTY_DUEL_WAIT_REVEALS;
        memcpy(verifier.commits,s.commits,64);
        verifier.commit_set[0]=verifier.commit_set[1]=true;
        memset(verifier.reveal_set,0,sizeof(verifier.reveal_set));
        verifier.round_config=model.round_config;
        penalty_duel_action_t a=decode_action(b+173), d=decode_action(b+183);
        if (!penalty_duel_reveal(&verifier,PENALTY_DUEL_HOST,&a) ||
            !penalty_duel_reveal(&verifier,PENALTY_DUEL_GUEST,&d) ||
            verifier.round_result.outcome!=s.round_result.outcome ||
            memcmp(verifier.goals,s.goals,sizeof(s.goals)) ||
            memcmp(verifier.attempts,s.attempts,sizeof(s.attempts))) return false;
        penalty_mp_core_t expected={.duel=verifier}; result_ready(&expected,0);
        if (expected.duel.phase!=s.phase || expected.duel.match_result!=s.match_result ||
            expected.duel.ended_early!=s.ended_early) return false;
    }
    if (s.random_revealed) {
        model.actions[0]=decode_action(b+173); model.actions[1]=decode_action(b+183);
        for (unsigned i=0;i<2;++i) {
            uint8_t digest[32]; penalty_duel_make_commit(&model,i,&model.actions[i],digest);
            if (memcmp(digest,model.commits[i],32)) return false;
        }
    }
    if (same && c->local_locked) {
        uint8_t digest[32]; penalty_duel_make_commit(&model,c->peer,&c->local_action,digest);
        if (model.commit_set[c->peer] && memcmp(digest,model.commits[c->peer],32)) return false;
        if (!model.commit_set[c->peer] && !penalty_duel_commit(&model,c->peer,digest)) return false;
    } else {
        c->local_locked=false; c->local_action=(penalty_duel_action_t){.target=PENALTY_TOP_CENTER};
        c->next_ready[c->peer]=false;
    }
    c->duel=model;
    c->settings_sent=(b[25]&1)!=0;
    c->settings_confirmed=c->settings_confirmed || (b[25]&2)!=0;
    c->next_ready[PENALTY_DUEL_HOST]=(b[25]&4)!=0;
    c->resume_phase=b[21];
    if (c->resume_phase==PENALTY_MP_AIM || c->resume_phase==PENALTY_MP_WAIT)
        c->resume_phase=c->local_locked ? PENALTY_MP_WAIT : PENALTY_MP_AIM;
    c->reveal_sent=false;
    c->snapshot_applied=true;
    (void)packet(c,PENALTY_BLE_STATE_ACK,PREFIX);
    return c->phase!=PENALTY_MP_FAILED;
}
static void continue_after_resume(penalty_mp_core_t *c, uint64_t now) {
    c->phase=c->resume_phase; c->since_ms=now; c->recovery_deadline=0;
    if (c->peer==PENALTY_DUEL_HOST) {
        if (c->duel.phase==PENALTY_DUEL_WAIT_REVEALS) {
            c->reveal_sent=false; request_reveals(c);
        } else if (c->phase==PENALTY_MP_RESULT) try_next(c,now);
    } else if (c->phase==PENALTY_MP_SETTINGS && c->settings_confirmed) {
        penalty_mp_packet_t *p=packet(c,PENALTY_BLE_LOBBY_SETTINGS,22);
        if (p) { p->bytes[20]=c->duel.difficulty; p->bytes[21]=1; }
    } else if (c->phase==PENALTY_MP_WAIT && c->local_locked) {
        penalty_mp_packet_t *p=packet(c,PENALTY_BLE_ACTION_COMMIT,52);
        if (p) memcpy(p->bytes+PREFIX,c->duel.commits[c->peer],32);
    } else if (c->phase==PENALTY_MP_RESULT && c->next_ready[c->peer]) {
        (void)packet(c,PENALTY_BLE_ANIMATION_START,PREFIX);
    }
}
bool penalty_mp_receive(penalty_mp_core_t *c, const penalty_mp_packet_t *p, uint64_t now) {
    if (!c || !p || p->length < PREFIX || p->length > PENALTY_BLE_MAX_PAYLOAD_BYTES ||
        c->phase == PENALTY_MP_FAILED) return false;
    penalty_mp_tick(c,now);
    if (c->phase==PENALTY_MP_FAILED) return false;
    bool host = c->peer == PENALTY_DUEL_HOST;
    uint32_t round = get32(p->bytes + 16);
    if (p->type==PENALTY_BLE_SNAPSHOT && !host && c->phase==PENALTY_MP_RECONNECT) {
        if (!apply_snapshot(c,p)) goto bad;
        return true;
    }
    /* Only the first Host settings packet may bind a Guest's match identity. */
    if (!host && p->type == PENALTY_BLE_LOBBY_SETTINGS && !c->settings_sent && round == 0) {
        if (p->length != 22 || p->bytes[20] > PENALTY_HARD || p->bytes[21] != 0) goto bad;
        penalty_duel_init(&c->duel, p->bytes, p->bytes[20]);
        c->settings_sent = true;
        return true;
    }
    if (memcmp(p->bytes, c->duel.match_id, 16) != 0) goto bad;
    if (p->type==PENALTY_BLE_ABORT) {
        if (p->length!=PREFIX+1 || p->bytes[PREFIX]!=PENALTY_DUEL_USER_CANCELLED) goto bad;
        penalty_duel_abort(&c->duel,PENALTY_DUEL_USER_CANCELLED); c->phase=PENALTY_MP_FAILED;
        c->duel.phase=PENALTY_DUEL_ABORTED; c->duel.match_result=PENALTY_DUEL_NO_CONTEST;
        c->duel.abort_reason=PENALTY_DUEL_USER_CANCELLED;
        return true;
    }
    if (c->phase==PENALTY_MP_RECONNECT) {
        if (now>=c->recovery_deadline || round!=c->duel.round_id || p->length!=PREFIX) goto bad;
        if (host && p->type==PENALTY_BLE_STATE_ACK) {
            (void)packet(c,PENALTY_BLE_RESUME,PREFIX); continue_after_resume(c,now); return true;
        }
        if (!host && p->type==PENALTY_BLE_RESUME && c->snapshot_applied) {
            continue_after_resume(c,now); return true;
        }
        goto bad;
    }
    if (round<c->duel.round_id) return true; /* Already applied, never score twice. */
    if (p->type == PENALTY_BLE_ROUND_OPEN && !host) {
        if (round==c->duel.round_id && p->length==OPEN_BYTES && p->bytes[20]==c->duel.first_shooter &&
            p->bytes[21]==c->duel.difficulty && !memcmp(p->bytes+28,c->duel.random_commit,32)) {
            penalty_duel_meter_t shooter=decode_meter(p->bytes+22), keeper=decode_meter(p->bytes+25);
            if (memcmp(&shooter,&c->duel.round_config.shooter_meter,sizeof(shooter)) ||
                memcmp(&keeper,&c->duel.round_config.keeper_meter,sizeof(keeper))) goto bad;
            return true;
        }
        if (p->length != OPEN_BYTES || round != c->duel.round_id + 1 ||
            p->bytes[20] > PENALTY_DUEL_GUEST || p->bytes[21] != c->duel.difficulty ||
            (round == 1 ? c->phase != PENALTY_MP_COIN :
             c->phase != PENALTY_MP_RESULT || !c->next_ready[c->peer]) ||
            (round > 1 && p->bytes[20] != c->duel.first_shooter)) goto bad;
        penalty_duel_round_config_t config = {
            .shooter_meter = decode_meter(p->bytes + 22),
            .keeper_meter = decode_meter(p->bytes + 25),
        };
        bool ok = round == 1
            ? penalty_duel_trigger_coin(&c->duel, PENALTY_DUEL_HOST, p->bytes[20], &config)
            : penalty_duel_advance(&c->duel, PENALTY_DUEL_HOST, &config);
        if (!ok) goto bad;
        memcpy(c->duel.random_commit, p->bytes + 28, 32);
        opened(c, now, round == 1);
        return true;
    }
    if (round != c->duel.round_id) goto bad;
    switch (p->type) {
    case PENALTY_BLE_LOBBY_SETTINGS:
        if (!host || p->length != 22 || !c->settings_sent ||
            c->settings_confirmed || c->phase != PENALTY_MP_SETTINGS ||
            p->bytes[20] != c->duel.difficulty || p->bytes[21] != 1) goto bad;
        c->settings_confirmed = true; c->phase = PENALTY_MP_COIN;
        (void)packet(c, PENALTY_BLE_COIN_RESULT, PREFIX);
        break;
    case PENALTY_BLE_COIN_RESULT:
        if (host || p->length != PREFIX || c->phase != PENALTY_MP_SETTINGS ||
            !c->settings_confirmed) goto bad;
        c->phase = PENALTY_MP_COIN;
        break;
    case PENALTY_BLE_ACTION_COMMIT:
        if (!host || p->length != 52 ||
            !penalty_duel_commit(&c->duel, PENALTY_DUEL_GUEST, p->bytes + PREFIX)) goto bad;
        request_reveals(c);
        break;
    case PENALTY_BLE_REVEAL_REQUEST:
        if (host || p->length != 84 || !c->local_locked ||
            memcmp(c->duel.commits[c->peer], p->bytes + PREFIX + 32, 32) != 0 ||
            !penalty_duel_commit(&c->duel, PENALTY_DUEL_HOST, p->bytes + PREFIX)) goto bad;
        c->reveal_sent = true;
        penalty_mp_packet_t *reveal = packet(c, PENALTY_BLE_ACTION_REVEAL, 30);
        if (reveal) encode_action(reveal->bytes + PREFIX, c->local_action);
        break;
    case PENALTY_BLE_ACTION_REVEAL: {
        if (!host || p->length != 30 || !c->reveal_sent) goto bad;
        penalty_duel_action_t action = decode_action(p->bytes + PREFIX);
        if (!penalty_duel_reveal(&c->duel, PENALTY_DUEL_GUEST, &action)) goto bad;
        if (c->duel.total_settled == round && c->phase == PENALTY_MP_WAIT)
            send_result(c, now);
        break;
    }
    case PENALTY_BLE_ROUND_RESULT: {
        if (!host && p->length==RESULT_BYTES && c->duel.total_settled==round &&
            get32(p->bytes+51)==round && c->duel.round_result.outcome==p->bytes[50]) {
            uint8_t actions[20]; encode_action(actions,c->duel.actions[0]); encode_action(actions+10,c->duel.actions[1]);
            if (p->bytes[20]!=c->duel.round_config.shot_roll || p->bytes[21]!=c->duel.round_config.save_roll ||
                get64(p->bytes+22)!=c->duel.round_config.random_nonce || memcmp(actions,p->bytes+30,20)) goto bad;
            return true;
        }
        if (host || p->length != RESULT_BYTES || !c->reveal_sent ||
            c->phase != PENALTY_MP_WAIT || c->duel.phase != PENALTY_DUEL_WAIT_REVEALS) goto bad;
        penalty_duel_round_config_t config = c->duel.round_config;
        config.shot_roll = p->bytes[20]; config.save_roll = p->bytes[21];
        config.random_nonce = get64(p->bytes + 22);
        uint8_t digest[32];
        penalty_duel_make_random_commit(&c->duel, &config, digest);
        if (config.shot_roll >= 100 || config.save_roll >= 100 ||
            memcmp(digest, c->duel.random_commit, 32) != 0) goto bad;
        /* Validate on a copy; malformed results never partly apply a score. */
        penalty_duel_model_t verifier = c->duel;
        verifier.round_config = config;
        penalty_duel_action_t a = decode_action(p->bytes + 30);
        penalty_duel_action_t b = decode_action(p->bytes + 40);
        if (!penalty_duel_reveal(&verifier, PENALTY_DUEL_HOST, &a) ||
            !penalty_duel_reveal(&verifier, PENALTY_DUEL_GUEST, &b) ||
            verifier.round_result.outcome != p->bytes[50] ||
            verifier.total_settled != get32(p->bytes + 51)) goto bad;
        c->duel = verifier;
        result_ready(c, now);
        break;
    }
    case PENALTY_BLE_ANIMATION_START:
        if (!host || p->length != PREFIX || c->duel.total_settled != round ||
            (c->phase != PENALTY_MP_RESULT && c->phase != PENALTY_MP_ANIMATION)) goto bad;
        c->next_ready[PENALTY_DUEL_GUEST] = true;
        try_next(c, now);
        break;
    default:
        goto bad;
    }
    return c->phase != PENALTY_MP_FAILED;
bad:
    fail(c);
    return false;
}
void penalty_mp_project(const penalty_mp_core_t *c, penalty_model_t *v,
                        penalty_language_t language, bool muted, uint64_t now) {
    memset(v, 0, sizeof(*v));
    v->language = language; v->muted = muted; v->difficulty = c->duel.difficulty;
    v->multiplayer = true;
    v->multiplayer_blue_shooter = c->duel.current_shooter == PENALTY_DUEL_GUEST;
    v->turn_role = c->peer == c->duel.current_shooter ? PENALTY_ROLE_SHOOTER : PENALTY_ROLE_KEEPER;
    v->session_kind = PENALTY_SESSION_SHOOTOUT;
    v->since_ms = c->since_ms;
    v->state = c->phase == PENALTY_MP_AIM ? PENALTY_AIM :
        c->phase == PENALTY_MP_CHARGE ? PENALTY_CHARGE :
        c->phase == PENALTY_MP_ANIMATION ? PENALTY_FLIGHT :
        c->phase == PENALTY_MP_RESULT || c->phase == PENALTY_MP_DONE ? PENALTY_RESULT :
        c->phase == PENALTY_MP_COIN_SPIN ? PENALTY_COIN_SPIN : PENALTY_HANDOFF;
    penalty_duel_meter_t meter = local_meter(c);
    v->current = (penalty_shot_t){.player_target = c->local_action.target,
        .green_low = meter.green_low, .green_high = meter.green_high,
        .perfect_low = meter.perfect_low, .timing_value = c->phase == PENALTY_MP_CHARGE
            ? power_at(c, now) : c->local_action.timing_value};
    v->match.coin_face = c->peer == c->duel.first_shooter ? PENALTY_COIN_FOOTBALL : PENALTY_COIN_GLOVE;
    if (c->phase == PENALTY_MP_ANIMATION || c->phase == PENALTY_MP_RESULT || c->phase == PENALTY_MP_DONE) {
        v->current.player_target = c->duel.actions[c->peer].target;
        v->current.opponent_target = c->duel.actions[1 - c->peer].target;
        penalty_duel_outcome_t outcome = c->duel.round_result.outcome;
        v->multiplayer_accuracy_miss = outcome == PENALTY_DUEL_MISS_ACCURACY;
        bool keeper = v->turn_role == PENALTY_ROLE_KEEPER;
        v->current.outcome = outcome == PENALTY_DUEL_GOAL
            ? (keeper ? PENALTY_KEEPER_WRONG_WAY : PENALTY_GOAL)
            : outcome == PENALTY_DUEL_SAVE ? (keeper ? PENALTY_KEEPER_CATCH : PENALTY_SAVE)
            : outcome == PENALTY_DUEL_MISS_WEAK ? PENALTY_WEAK
            : PENALTY_HIGH;
    }
}
