#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Worker-owned, RAM-only hold. Time begins at the actual disconnect event. */
typedef struct {
    uint64_t release_at_ms;
    uint32_t hold_ms;
    bool active;
} penalty_ble_fault_t;

void penalty_ble_fault_reset(penalty_ble_fault_t *fault);
bool penalty_ble_fault_arm(penalty_ble_fault_t *fault, unsigned seconds);
void penalty_ble_fault_disconnected(penalty_ble_fault_t *fault, uint64_t now);
bool penalty_ble_fault_release(penalty_ble_fault_t *fault, uint64_t now);
