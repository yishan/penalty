#include "penalty_ble_fault.h"

void penalty_ble_fault_reset(penalty_ble_fault_t *fault) {
    *fault = (penalty_ble_fault_t){0};
}
bool penalty_ble_fault_arm(penalty_ble_fault_t *fault, unsigned seconds) {
    if (fault->active || (seconds != 10 && seconds != 35)) return false;
    *fault = (penalty_ble_fault_t){.hold_ms = seconds * 1000, .active = true};
    return true;
}
void penalty_ble_fault_disconnected(penalty_ble_fault_t *fault, uint64_t now) {
    if (fault->active && !fault->release_at_ms)
        fault->release_at_ms = now + fault->hold_ms;
}
bool penalty_ble_fault_release(penalty_ble_fault_t *fault, uint64_t now) {
    if (!fault->active || !fault->release_at_ms || now < fault->release_at_ms)
        return false;
    penalty_ble_fault_reset(fault);
    return true;
}
