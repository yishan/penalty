#include "penalty_ble_reliable.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    penalty_ble_reliable_t r;
    penalty_ble_reliable_init(&r);
    penalty_ble_message_t m={.type=PENALTY_BLE_ACTION_COMMIT,.epoch=42,.seq=123,.payload_len=52};
    assert(penalty_ble_reliable_begin(&r,&m));
    assert(!penalty_ble_reliable_begin(&r,&m));
    assert(!penalty_ble_reliable_ack(&r,41,123));
    assert(!penalty_ble_reliable_ack(&r,42,122));
    for (unsigned retry=0;retry<3;++retry) {
        penalty_ble_reliable_sent(&r,retry*300);
        assert(penalty_ble_reliable_tick(&r,retry*300+299)==PENALTY_BLE_RELIABLE_IDLE);
        assert(penalty_ble_reliable_tick(&r,(retry+1)*300)==PENALTY_BLE_RELIABLE_RETRY);
        assert(r.message.seq==123 && r.message.payload_len==52);
    }
    penalty_ble_reliable_sent(&r,900);
    assert(penalty_ble_reliable_tick(&r,1200)==PENALTY_BLE_RELIABLE_EXPIRED);
    assert(penalty_ble_reliable_ack(&r,42,123));
    assert(!penalty_ble_reliable_ack(&r,42,123));
    assert(penalty_ble_reliable_tick(&r,9999)==PENALTY_BLE_RELIABLE_IDLE);
    puts("Penalty BLE reliability: PASS (300ms/3 retries, immutable sequence, epoch-bound ACK)");
}
