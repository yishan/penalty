#pragma once

#include "penalty_ble_protocol.h"
#include "penalty_duel_model.h"

#define PENALTY_MP_RECOVERY_MS 30000u
#define PENALTY_MP_SNAPSHOT_BYTES 193u

/* Match state and secrets live in RAM; reboot cannot resume a match. */
typedef enum {
    PENALTY_MP_SETTINGS, PENALTY_MP_COIN, PENALTY_MP_COIN_SPIN,
    PENALTY_MP_AIM, PENALTY_MP_CHARGE, PENALTY_MP_WAIT,
    PENALTY_MP_ANIMATION, PENALTY_MP_RESULT, PENALTY_MP_DONE,
    PENALTY_MP_FAILED, PENALTY_MP_RECONNECT
} penalty_mp_phase_t;

typedef struct {
    penalty_ble_message_type_t type;
    uint16_t length;
    uint8_t bytes[PENALTY_BLE_MAX_PAYLOAD_BYTES];
} penalty_mp_packet_t;

typedef struct {
    penalty_duel_model_t duel;
    penalty_duel_peer_t peer;
    penalty_mp_phase_t phase;
    penalty_duel_action_t local_action;
    uint64_t since_ms;
    uint32_t random;
    uint32_t (*random_fn)(void); /* Production uses ESP hardware RNG. */
    bool local_locked, reveal_sent, settings_sent, settings_confirmed;
    bool next_ready[2];
    penalty_mp_packet_t outbox[4];
    uint8_t out_head, out_count;
    penalty_mp_phase_t resume_phase;
    uint64_t recovery_deadline;
    bool snapshot_applied;
} penalty_mp_core_t;

void penalty_mp_init(penalty_mp_core_t *core, penalty_duel_peer_t peer,
                     penalty_difficulty_t difficulty, const uint8_t match_id[16],
                     uint32_t seed, uint64_t now);
void penalty_mp_input(penalty_mp_core_t *core, penalty_input_t input, uint64_t at);
void penalty_mp_tick(penalty_mp_core_t *core, uint64_t now);
void penalty_mp_pause(penalty_mp_core_t *core, uint64_t now);
void penalty_mp_link_ready(penalty_mp_core_t *core, uint64_t now);
void penalty_mp_cancel(penalty_mp_core_t *core);
bool penalty_mp_receive(penalty_mp_core_t *core, const penalty_mp_packet_t *packet,
                        uint64_t now);
bool penalty_mp_pop(penalty_mp_core_t *core, penalty_mp_packet_t *packet);
void penalty_mp_project(const penalty_mp_core_t *core, penalty_model_t *view,
                        penalty_language_t language, bool muted, uint64_t now);
