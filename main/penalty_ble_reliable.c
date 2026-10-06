#include "penalty_ble_reliable.h"
#include <string.h>

void penalty_ble_reliable_init(penalty_ble_reliable_t *r) { memset(r,0,sizeof(*r)); }
bool penalty_ble_reliable_begin(penalty_ble_reliable_t *r,const penalty_ble_message_t *m) {
    if (!r || !m || r->pending || !m->seq || m->payload_len>PENALTY_BLE_MAX_PAYLOAD_BYTES)
        return false;
    r->message=*m; r->pending=true; r->retries=0; r->deadline=0; return true;
}
void penalty_ble_reliable_sent(penalty_ble_reliable_t *r,uint64_t now) {
    if (r && r->pending) r->deadline=now+PENALTY_BLE_RETRY_MS;
}
bool penalty_ble_reliable_ack(penalty_ble_reliable_t *r,uint16_t epoch,uint32_t ack) {
    if (!r || !r->pending || r->message.epoch!=epoch || r->message.seq!=ack) return false;
    r->pending=false; r->deadline=0; return true;
}
penalty_ble_reliable_result_t penalty_ble_reliable_tick(penalty_ble_reliable_t *r,uint64_t now) {
    if (!r || !r->pending || !r->deadline || now<r->deadline) return PENALTY_BLE_RELIABLE_IDLE;
    if (r->retries==PENALTY_BLE_RETRY_LIMIT) return PENALTY_BLE_RELIABLE_EXPIRED;
    ++r->retries; r->deadline=0; return PENALTY_BLE_RELIABLE_RETRY;
}
