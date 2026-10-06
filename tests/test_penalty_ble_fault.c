#include "penalty_ble_fault.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    penalty_ble_fault_t fault={0};
    assert(!penalty_ble_fault_release(&fault,99999));
    assert(!penalty_ble_fault_arm(&fault,0));
    assert(!penalty_ble_fault_arm(&fault,30));
    assert(!penalty_ble_fault_arm(&fault,~0u));
    for (unsigned seconds=10; seconds<=35; seconds+=25) {
        assert(penalty_ble_fault_arm(&fault,seconds));
        assert(fault.active && !fault.release_at_ms);
        assert(!penalty_ble_fault_arm(&fault,seconds));
        assert(!penalty_ble_fault_release(&fault,1000000)); /* Await actual GAP. */
        penalty_ble_fault_disconnected(&fault,100);
        assert(fault.release_at_ms==100+seconds*1000);
        penalty_ble_fault_disconnected(&fault,1000); /* Duplicate cannot extend. */
        assert(fault.release_at_ms==100+seconds*1000);
        assert(!penalty_ble_fault_release(&fault,99+seconds*1000));
        assert(penalty_ble_fault_release(&fault,100+seconds*1000));
        assert(!fault.active && !fault.release_at_ms && !fault.hold_ms);
        assert(!penalty_ble_fault_release(&fault,1000000));
    }
    assert(penalty_ble_fault_arm(&fault,35));
    penalty_ble_fault_disconnected(&fault,0);
    penalty_ble_fault_reset(&fault); /* Failure/exit/new session. */
    assert(!fault.active && !penalty_ble_fault_release(&fault,35000));
    assert(penalty_ble_fault_arm(&fault,10));
    penalty_ble_fault_reset(&fault);
    puts("Penalty BLE fault policy: PASS (10s/35s, real-disconnect clock, duplicate, reset)");
}
