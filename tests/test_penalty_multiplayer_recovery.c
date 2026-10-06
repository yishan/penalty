#include "penalty_multiplayer_core.h"
#include "penalty_ble_reliable.h"
#include "penalty_ble_fault.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint64_t at;
static uint32_t rng = 0x19870412;
static unsigned delivered;
static unsigned transmissions, lost, duplicated, old_epoch;
static uint32_t sequence[2];
static uint16_t epoch=1, fragment_id;
static penalty_ble_rx_window_t receiver[2];
static uint32_t random32(void) {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng;
}
static void equal_score(const penalty_mp_core_t *h, const penalty_mp_core_t *g) {
    assert(penalty_round_for_kick(h->duel.round_id) ==
           penalty_round_for_kick(g->duel.round_id));
    assert(h->duel.total_settled == g->duel.total_settled);
    assert(!memcmp(h->duel.goals, g->duel.goals, sizeof(h->duel.goals)));
    assert(!memcmp(h->duel.attempts, g->duel.attempts, sizeof(h->duel.attempts)));
    penalty_model_t a, b;
    penalty_mp_project(h, &a, PENALTY_LANGUAGE_EN, false, at);
    penalty_mp_project(g, &b, PENALTY_LANGUAGE_ZH_CN, false, at);
    assert(a.multiplayer_blue_shooter == (h->duel.current_shooter == PENALTY_DUEL_GUEST));
    assert(a.multiplayer_blue_shooter == b.multiplayer_blue_shooter);
}
static penalty_ble_message_t wire_roundtrip(const penalty_ble_message_t *m) {
    uint8_t wire[256], rebuilt[256], fragment[64]; size_t rebuilt_size=0;
    size_t size=penalty_ble_encode(m,wire,sizeof(wire)); assert(size);
    size_t mtu=(transmissions%2) ? 20 : 64;
    penalty_ble_reassembler_t assembler; penalty_ble_reassembler_init(&assembler);
    unsigned count=penalty_ble_fragment_count(size,mtu); ++fragment_id;
    for (unsigned i=0;i<count;++i) {
        size_t n=penalty_ble_make_fragment(fragment_id,wire,size,mtu,i,fragment,sizeof(fragment));
        assert(n);
        penalty_ble_reassembly_result_t result=penalty_ble_reassembler_push(&assembler,
            fragment,n,rebuilt,sizeof(rebuilt),&rebuilt_size);
        assert(result==(i+1==count ? PENALTY_BLE_REASSEMBLY_COMPLETE : PENALTY_BLE_REASSEMBLY_MORE));
    }
    penalty_ble_message_t decoded;
    assert(penalty_ble_decode(rebuilt,rebuilt_size,&decoded)==PENALTY_BLE_DECODE_OK);
    return decoded;
}
static void route(penalty_mp_core_t *destination,const penalty_mp_packet_t *p,unsigned sender) {
    penalty_ble_message_t m={.type=p->type,.epoch=epoch,.seq=++sequence[sender],
        .flags=PENALTY_BLE_FLAG_ACK_REQUIRED,.payload_len=p->length};
    memcpy(m.payload,p->bytes,p->length);
    penalty_ble_reliable_t reliable; penalty_ble_reliable_init(&reliable);
    assert(penalty_ble_reliable_begin(&reliable,&m));
    for (unsigned attempt=0;attempt<=PENALTY_BLE_RETRY_LIMIT;++attempt) {
        ++transmissions;
        penalty_ble_message_t received=wire_roundtrip(&reliable.message);
        if (attempt<3 && random32()%5==0) ++lost; /* Whole-message loss. */
        else {
            penalty_ble_rx_result_t accepted=penalty_ble_rx_accept(&receiver[sender],received.epoch,received.seq);
            if (accepted==PENALTY_BLE_RX_NEW) {
                penalty_mp_packet_t packet={.type=received.type,.length=received.payload_len};
                memcpy(packet.bytes,received.payload,packet.length);
                assert(penalty_mp_receive(destination,&packet,at)); ++delivered;
            } else { assert(accepted==PENALTY_BLE_RX_DUPLICATE); ++duplicated; }
            assert(penalty_ble_rx_accept(&receiver[sender],received.epoch,received.seq)==PENALTY_BLE_RX_DUPLICATE);
            ++duplicated;
            /* Old-epoch packets are rejected before reaching the game. */
            assert(penalty_ble_rx_accept(&receiver[sender],epoch-1,received.seq)==PENALTY_BLE_RX_WRONG_EPOCH);
            ++old_epoch;
            penalty_ble_message_t ack={.type=PENALTY_BLE_TRANSPORT_ACK,.epoch=epoch,.ack=received.seq};
            ack=wire_roundtrip(&ack);
            assert(!penalty_ble_reliable_ack(&reliable,epoch-1,ack.ack));
            if (attempt==3 || random32()%4!=0) assert(penalty_ble_reliable_ack(&reliable,ack.epoch,ack.ack));
            else ++lost; /* Application-applied ACK loss. */
        }
        if (!reliable.pending) return;
        penalty_ble_reliable_sent(&reliable,attempt*300);
        assert(penalty_ble_reliable_tick(&reliable,(attempt+1)*300)==PENALTY_BLE_RELIABLE_RETRY);
    }
    assert(!"retry limit");
}
static void pump(penalty_mp_core_t *h, penalty_mp_core_t *g) {
    for (unsigned step = 0; step < 32; ++step) {
        penalty_mp_packet_t p;
        bool moved = false;
        if (penalty_mp_pop(h, &p)) {
            route(g,&p,0); moved = true;
        }
        if (penalty_mp_pop(g, &p)) {
            route(h,&p,1); moved = true;
        }
        if (!moved) return;
    }
    assert(!"message cycle");
}
static void pair(penalty_mp_core_t *h, penalty_mp_core_t *g, uint32_t seed) {
    const uint8_t id[16] = {1,2,3,4};
    at = 100;
    ++epoch; memset(sequence,0,sizeof(sequence));
    penalty_ble_rx_window_init(&receiver[0],epoch); penalty_ble_rx_window_init(&receiver[1],epoch);
    penalty_mp_init(h, PENALTY_DUEL_HOST, seed % 3, id, seed, at);
    penalty_mp_init(g, PENALTY_DUEL_GUEST, PENALTY_NORMAL, id, seed + 1, at);
    penalty_mp_input(h, PENALTY_INPUT_OK, at); pump(h,g);
    penalty_mp_input(g, PENALTY_INPUT_OK, at); pump(h,g);
    penalty_mp_input(h, PENALTY_INPUT_OK, at); pump(h,g);
    at += PENALTY_COIN_SPIN_MS;
    penalty_mp_tick(h,at); penalty_mp_tick(g,at);
}
static void lock(penalty_mp_core_t *c, uint8_t value) {
    penalty_mp_input(c, PENALTY_INPUT_OK, at);
    penalty_duel_meter_t m = c->peer == c->duel.current_shooter
        ? c->duel.round_config.shooter_meter : c->duel.round_config.keeper_meter;
    penalty_shot_t shot = {.green_low=m.green_low,.green_high=m.green_high};
    unsigned elapsed = 0;
    while (elapsed < 4000 && penalty_power_at(&shot,elapsed) != value) ++elapsed;
    assert(elapsed < 4000);
    penalty_mp_input(c,PENALTY_INPUT_OK,at+elapsed);
    assert(c->local_locked);
}
static void restore(penalty_mp_core_t *h, penalty_mp_core_t *g) {
    penalty_duel_round_config_t random_before = h->duel.round_config;
    penalty_duel_action_t a = g->local_action;
    uint32_t previous_round=g->duel.round_id;
    bool locked = g->local_locked;
    penalty_mp_pause(h,at); penalty_mp_pause(g,at);
    ++epoch; memset(sequence,0,sizeof(sequence));
    penalty_ble_rx_window_init(&receiver[0],epoch); penalty_ble_rx_window_init(&receiver[1],epoch);
    uint64_t deadline = h->recovery_deadline;
    penalty_mp_pause(h,at+1); assert(h->recovery_deadline == deadline);
    assert(h->phase == PENALTY_MP_RECONNECT && g->phase == PENALTY_MP_RECONNECT);
    penalty_mp_input(g,PENALTY_INPUT_DOWN,at+1);
    at += 100;
    penalty_mp_link_ready(h,at); penalty_mp_link_ready(g,at);
    pump(h,g);
    assert(h->phase != PENALTY_MP_RECONNECT && g->phase != PENALTY_MP_RECONNECT);
    assert(!memcmp(&random_before,&h->duel.round_config,sizeof(random_before)));
    if (locked && previous_round == g->duel.round_id) {
        assert(g->local_action.target == a.target && g->local_action.timing_value == a.timing_value);
        assert(g->local_action.nonce == a.nonce);
    }
    equal_score(h,g);
}
static void finish_animation(penalty_mp_core_t *h, penalty_mp_core_t *g) {
    at += 1000;
    penalty_mp_tick(h,at); penalty_mp_tick(g,at);
    equal_score(h,g);
}
static void next(penalty_mp_core_t *h, penalty_mp_core_t *g) {
    penalty_mp_input(g,PENALTY_INPUT_OK,at); pump(h,g);
    penalty_mp_input(h,PENALTY_INPUT_OK,at); pump(h,g);
}
static void recovery_boundaries(void) {
    for (unsigned boundary = 0; boundary < 8; ++boundary) {
        penalty_mp_core_t h,g; pair(&h,&g,42+boundary);
        if (boundary == 0) {
            penalty_mp_input(&g,PENALTY_INPUT_OK,at); /* uncommitted charge */
        } else {
            lock(&g,50);
            if (boundary > 1) pump(&h,&g);
            if (boundary > 2) {
                lock(&h,50);
                if (boundary > 3) {
                    penalty_mp_packet_t request;
                    assert(penalty_mp_pop(&h,&request));
                    assert(penalty_mp_receive(&g,&request,at));
                }
                if (boundary > 4) {
                    penalty_mp_packet_t reveal;
                    assert(penalty_mp_pop(&g,&reveal));
                    assert(penalty_mp_receive(&h,&reveal,at));
                }
                if (boundary > 5) pump(&h,&g);
                if (boundary > 6) finish_animation(&h,&g);
            }
        }
        restore(&h,&g);
        if (!g.local_locked && g.phase == PENALTY_MP_AIM) lock(&g,50);
        pump(&h,&g);
        if (!h.local_locked && h.phase == PENALTY_MP_AIM) lock(&h,50);
        pump(&h,&g); finish_animation(&h,&g);
        assert(h.duel.total_settled == 1);
        next(&h,&g);
        assert(h.duel.round_id == 2 && g.duel.round_id == 2);
    }
    penalty_mp_core_t h,g; pair(&h,&g,2);
    penalty_mp_pause(&h,at); penalty_mp_tick(&h,at+PENALTY_MP_RECOVERY_MS);
    assert(h.phase == PENALTY_MP_FAILED && h.duel.match_result == PENALTY_DUEL_NO_CONTEST);
    assert(h.duel.abort_reason == PENALTY_DUEL_DISCONNECT_TIMEOUT);
}
static void full_match_and_faults(void) {
    unsigned complete = 0, early=0, sudden=0;
    bool orders[2]={false}, difficulties[3]={false};
    for (unsigned match = 1; match <= 180; ++match) {
        penalty_mp_core_t h,g; pair(&h,&g,match);
        orders[h.duel.first_shooter]=true; difficulties[h.duel.difficulty]=true;
        for (unsigned kick = 0; kick < 100; ++kick) {
            if ((random32()%5)==0) restore(&h,&g);
            h.local_action.target = random32()%6; g.local_action.target=random32()%6;
            lock(&g,30+random32()%61);
            if ((random32()%3)==0) restore(&h,&g); else pump(&h,&g);
            lock(&h,30+random32()%61);
            if ((random32()%3)==0) restore(&h,&g); else pump(&h,&g);
            finish_animation(&h,&g);
            if (h.phase == PENALTY_MP_DONE) {
                assert(g.phase == PENALTY_MP_DONE);
                assert(h.duel.match_result == g.duel.match_result);
                early+=h.duel.ended_early; sudden+=h.duel.sudden_death;
                ++complete; break;
            }
            next(&h,&g);
        }
    }
    assert(complete == 180 && delivered >= 10000);
    assert(early && sudden && orders[0] && orders[1] && difficulties[0] && difficulties[1] && difficulties[2]);
}
static void invalid_snapshots_and_exit(void) {
    static const unsigned offsets[]={20,21,22,23,24,26,27,28,32,40,58,59,124,156};
    for (unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);++i) {
        penalty_mp_core_t h,g; pair(&h,&g,100+i);
        penalty_mp_pause(&h,at); penalty_mp_pause(&g,at);
        penalty_mp_link_ready(&h,at+1);
        penalty_mp_packet_t p; assert(penalty_mp_pop(&h,&p));
        p.bytes[offsets[i]]^=0xff;
        assert(!penalty_mp_receive(&g,&p,at+1));
        assert(g.phase==PENALTY_MP_FAILED && g.duel.total_settled==0);
    }
    penalty_mp_core_t h,g; pair(&h,&g,42);
    penalty_mp_pause(&h,at); penalty_mp_pause(&g,at);
    penalty_mp_link_ready(&h,at+1);
    penalty_mp_packet_t p; assert(penalty_mp_pop(&h,&p));
    for (unsigned i=173;i<PENALTY_MP_SNAPSHOT_BYTES;++i) assert(!p.bytes[i]);
    assert(!penalty_mp_receive(&g,&p,at+PENALTY_MP_RECOVERY_MS));
    assert(g.duel.abort_reason==PENALTY_DUEL_DISCONNECT_TIMEOUT);
    pair(&h,&g,43); penalty_mp_cancel(&g); pump(&h,&g);
    assert(h.phase==PENALTY_MP_FAILED && g.phase==PENALTY_MP_FAILED);
    assert(h.duel.match_result==PENALTY_DUEL_NO_CONTEST);
    assert(h.duel.abort_reason==PENALTY_DUEL_USER_CANCELLED);
    penalty_mp_packet_t stale={.type=PENALTY_BLE_ACTION_COMMIT,.length=52};
    memcpy(stale.bytes,h.duel.match_id,16);
    pair(&h,&g,44); h.duel.match_id[15]=99;
    assert(!penalty_mp_receive(&h,&stale,at)); /* An old match cannot enter a new lobby. */
}
static void lost_open_boundaries(void) {
    for (unsigned boundary=0;boundary<4;++boundary) {
        penalty_mp_core_t h,g; pair(&h,&g,50+boundary);
        if (boundary==0) { /* New first round opening lost before coin animation. */
            penalty_mp_init(&h,PENALTY_DUEL_HOST,PENALTY_NORMAL,h.duel.match_id,42,at);
            penalty_mp_init(&g,PENALTY_DUEL_GUEST,PENALTY_NORMAL,h.duel.match_id,43,at);
            penalty_mp_input(&h,PENALTY_INPUT_OK,at); pump(&h,&g);
            penalty_mp_input(&g,PENALTY_INPUT_OK,at); pump(&h,&g);
            penalty_mp_input(&h,PENALTY_INPUT_OK,at); /* Do not deliver opening. */
        } else {
            lock(&g,50); pump(&h,&g); lock(&h,50); pump(&h,&g); finish_animation(&h,&g);
            if (boundary==1) { penalty_mp_input(&g,PENALTY_INPUT_OK,at); /* Guest NEXT lost. */ }
            else {
                penalty_mp_input(&g,PENALTY_INPUT_OK,at); pump(&h,&g);
                penalty_mp_input(&h,PENALTY_INPUT_OK,at); /* Next opening lost. */
            }
        }
        restore(&h,&g);
        assert(h.duel.round_id==g.duel.round_id);
    }
}
static void controlled_dev_holds(void) {
    for (unsigned peer=0;peer<2;++peer) {
        for (unsigned seconds=10;seconds<=35;seconds+=25) {
            penalty_mp_core_t h,g; pair(&h,&g,42+peer);
            penalty_mp_core_t *locked=peer ? &g : &h;
            lock(locked,50); pump(&h,&g);
            penalty_duel_action_t before=locked->local_action;
            penalty_duel_round_config_t config=h.duel.round_config;
            uint8_t match_id[16]; memcpy(match_id,h.duel.match_id,16);
            penalty_ble_fault_t fault={0};
            assert(penalty_ble_fault_arm(&fault,seconds));
            penalty_ble_fault_disconnected(&fault,at);
            penalty_mp_pause(&h,at); penalty_mp_pause(&g,at);
            uint64_t original_deadline=h.recovery_deadline;
            penalty_mp_pause(&h,at+1000); penalty_mp_pause(&g,at+1000);
            assert(h.recovery_deadline==original_deadline);
            at+=seconds*1000;
            penalty_mp_tick(&h,at); penalty_mp_tick(&g,at);
            if (seconds==10) {
                assert(penalty_ble_fault_release(&fault,at));
                ++epoch; memset(sequence,0,sizeof(sequence));
                penalty_ble_rx_window_init(&receiver[0],epoch);
                penalty_ble_rx_window_init(&receiver[1],epoch);
                penalty_mp_link_ready(&h,at); penalty_mp_link_ready(&g,at); pump(&h,&g);
                assert(h.phase!=PENALTY_MP_RECONNECT && g.phase!=PENALTY_MP_RECONNECT);
                assert(!memcmp(&before,&locked->local_action,sizeof(before)));
                assert(!memcmp(&config,&h.duel.round_config,sizeof(config)));
                assert(!memcmp(match_id,h.duel.match_id,16));
                assert(!memcmp(match_id,g.duel.match_id,16));
                assert(!h.duel.total_settled && !g.duel.total_settled);
                equal_score(&h,&g);
            } else {
                assert(h.phase==PENALTY_MP_FAILED && g.phase==PENALTY_MP_FAILED);
                assert(h.duel.match_result==PENALTY_DUEL_NO_CONTEST);
                assert(g.duel.match_result==PENALTY_DUEL_NO_CONTEST);
                penalty_ble_fault_reset(&fault);
                assert(!penalty_ble_fault_release(&fault,at));
            }
        }
    }
    puts("Penalty dev holds: PASS (both roles, 10s frozen lock/state, 35s no contest)");
}
int main(void) {
    controlled_dev_holds();
    recovery_boundaries();
    invalid_snapshots_and_exit();
    lost_open_boundaries();
    full_match_and_faults();
    assert(lost && duplicated && old_epoch);
    printf("Penalty M4: PASS (180 full matches, %u delivered/%u transmissions, %u losses, %u duplicates, %u old epochs)\n",
        delivered,transmissions,lost,duplicated,old_epoch);
}
