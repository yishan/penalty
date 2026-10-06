#pragma once
#include "penalty_ble_protocol.h"

#define PENALTY_BLE_RETRY_MS 300u
#define PENALTY_BLE_RETRY_LIMIT 3u
typedef enum { PENALTY_BLE_RELIABLE_IDLE, PENALTY_BLE_RELIABLE_RETRY,
               PENALTY_BLE_RELIABLE_EXPIRED } penalty_ble_reliable_result_t;
typedef struct {
    penalty_ble_message_t message;
    uint64_t deadline;
    unsigned retries;
    bool pending;
} penalty_ble_reliable_t;
void penalty_ble_reliable_init(penalty_ble_reliable_t *r);
bool penalty_ble_reliable_begin(penalty_ble_reliable_t *r,const penalty_ble_message_t *message);
void penalty_ble_reliable_sent(penalty_ble_reliable_t *r,uint64_t now);
bool penalty_ble_reliable_ack(penalty_ble_reliable_t *r,uint16_t epoch,uint32_t ack);
penalty_ble_reliable_result_t penalty_ble_reliable_tick(penalty_ble_reliable_t *r,uint64_t now);
