#pragma once

#include "penalty_ble_protocol.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PENALTY_BLE_SNAPSHOT_SCHEMA 1u
#define PENALTY_BLE_ADVERTISEMENT_BYTES 12u
#define PENALTY_BLE_PROTOCOL_INFO_BYTES 20u
#define PENALTY_BLE_M4_PROTOCOL_INFO_BYTES 22u
#define PENALTY_BLE_LINK_SNAPSHOT_BYTES 24u

#define PENALTY_BLE_CAP_LINK_V1 (UINT32_C(1) << 0)
#define PENALTY_BLE_CAP_FRAGMENT_V1 (UINT32_C(1) << 1)
#define PENALTY_BLE_CAP_SNAPSHOT_V1 (UINT32_C(1) << 2)
#define PENALTY_BLE_CAP_SC_NUMERIC_COMPARE (UINT32_C(1) << 3)
#define PENALTY_BLE_CAP_DUEL_V1 (UINT32_C(1) << 4)
#define PENALTY_BLE_CAP_COMMIT_REVEAL_V1 (UINT32_C(1) << 5)
#define PENALTY_BLE_CAP_RECONNECT_V1 (UINT32_C(1) << 6)
#define PENALTY_BLE_CAP_M3_LOOP_V1 (UINT32_C(1) << 7)
#define PENALTY_BLE_CAP_M4_MATCH_V1 (UINT32_C(1) << 8)

/* M2 advertises only capabilities that this milestone actually implements. */
#define PENALTY_BLE_M2_CAPABILITIES \
    (PENALTY_BLE_CAP_LINK_V1 | PENALTY_BLE_CAP_FRAGMENT_V1 | \
     PENALTY_BLE_CAP_SNAPSHOT_V1 | PENALTY_BLE_CAP_SC_NUMERIC_COMPARE)
#define PENALTY_BLE_M2_REQUIRED_CAPABILITIES \
    (PENALTY_BLE_CAP_LINK_V1 | PENALTY_BLE_CAP_SC_NUMERIC_COMPARE)

#define PENALTY_BLE_SECURITY_ENCRYPTED 0x01u
#define PENALTY_BLE_SECURITY_AUTHENTICATED 0x02u


typedef enum {
    PENALTY_BLE_LINK_HOST,
    PENALTY_BLE_LINK_GUEST,
} penalty_ble_link_role_t;

typedef enum {
    PENALTY_BLE_LINK_OFF,
    PENALTY_BLE_LINK_STARTING,
    PENALTY_BLE_LINK_ADVERTISING,
    PENALTY_BLE_LINK_SCANNING,
    PENALTY_BLE_LINK_CONNECTING,
    PENALTY_BLE_LINK_SECURING,
    PENALTY_BLE_LINK_NUMERIC_CONFIRM,
    PENALTY_BLE_LINK_DISCOVERING,
    PENALTY_BLE_LINK_BINDING,
    PENALTY_BLE_LINK_READY,
    PENALTY_BLE_LINK_DISCONNECTED,
    PENALTY_BLE_LINK_REJECTED,
    PENALTY_BLE_LINK_STOPPING,
    PENALTY_BLE_LINK_ERROR,
} penalty_ble_link_state_t;

typedef struct {
    uint8_t protocol_major;
    uint8_t protocol_minor;
    uint16_t lobby_code;
    uint32_t offered_capabilities;
    uint32_t required_capabilities;
} penalty_ble_advertisement_t;

typedef struct {
    uint8_t protocol_major;
    uint8_t protocol_minor;
    uint8_t snapshot_schema;
    uint32_t offered_capabilities;
    uint32_t required_capabilities;
    uint64_t boot_instance_id;
} penalty_ble_protocol_info_t;

typedef enum {
    PENALTY_BLE_NEGOTIATE_OK,
    PENALTY_BLE_NEGOTIATE_MAJOR_MISMATCH,
    PENALTY_BLE_NEGOTIATE_SCHEMA_MISMATCH,
    PENALTY_BLE_NEGOTIATE_MISSING_CAPABILITY,
} penalty_ble_negotiate_result_t;

typedef struct {
    uint8_t schema;
    penalty_ble_link_role_t role;
    penalty_ble_link_state_t state;
    uint8_t security_flags;
    uint16_t lobby_code;
    uint16_t epoch;
    uint32_t tx_sequence;
    uint32_t rx_sequence;
    uint64_t boot_instance_id;
} penalty_ble_link_snapshot_t;

typedef struct {
    penalty_ble_link_role_t role;
    penalty_ble_link_state_t state;
    uint16_t lobby_code;
    uint32_t numeric_value;
    uint32_t negotiated_capabilities;
    uint64_t peer_boot_instance_id;
    penalty_ble_protocol_info_t local_info;
} penalty_ble_session_core_t;

size_t penalty_ble_advertisement_encode(
    const penalty_ble_advertisement_t *advertisement,
    uint8_t *output,
    size_t capacity);
bool penalty_ble_advertisement_decode(const uint8_t *input,
                                      size_t length,
                                      penalty_ble_advertisement_t *advertisement);
size_t penalty_ble_protocol_info_encode(const penalty_ble_protocol_info_t *info,
                                        uint8_t *output,
                                        size_t capacity);
bool penalty_ble_protocol_info_decode(const uint8_t *input,
                                      size_t length,
                                      penalty_ble_protocol_info_t *info);
penalty_ble_negotiate_result_t penalty_ble_protocol_negotiate(
    const penalty_ble_protocol_info_t *local,
    const penalty_ble_protocol_info_t *remote,
    uint32_t *negotiated_capabilities);
size_t penalty_ble_link_snapshot_encode(const penalty_ble_link_snapshot_t *snapshot,
                                        uint8_t *output,
                                        size_t capacity);
bool penalty_ble_link_snapshot_decode(const uint8_t *input,
                                      size_t length,
                                      penalty_ble_link_snapshot_t *snapshot);

void penalty_ble_session_core_init(penalty_ble_session_core_t *core,
                                   penalty_ble_link_role_t role,
                                   uint16_t lobby_code,
                                   uint64_t boot_instance_id);
bool penalty_ble_session_core_on_synced(penalty_ble_session_core_t *core);
bool penalty_ble_session_core_on_connecting(penalty_ble_session_core_t *core);
bool penalty_ble_session_core_on_connected(penalty_ble_session_core_t *core);
bool penalty_ble_session_core_on_numeric(penalty_ble_session_core_t *core,
                                         uint32_t numeric_value);
bool penalty_ble_session_core_confirm_numeric(penalty_ble_session_core_t *core,
                                              bool accepted);
bool penalty_ble_session_core_on_secured(penalty_ble_session_core_t *core);
bool penalty_ble_session_core_begin_binding(penalty_ble_session_core_t *core);
bool penalty_ble_session_core_on_negotiated(
    penalty_ble_session_core_t *core,
    const penalty_ble_protocol_info_t *remote);
void penalty_ble_session_core_on_disconnected(penalty_ble_session_core_t *core);
void penalty_ble_session_core_fail(penalty_ble_session_core_t *core);
