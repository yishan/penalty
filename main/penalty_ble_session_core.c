#include "penalty_ble_session_core.h"

#include <string.h>

static void put_u16(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

static uint16_t get_u16(const uint8_t *input) {
    return (uint16_t)input[0] | (uint16_t)input[1] << 8;
}

static void put_u32(uint8_t *output, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) output[i] = (uint8_t)(value >> (i * 8));
}

static uint32_t get_u32(const uint8_t *input) {
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= (uint32_t)input[i] << (i * 8);
    return value;
}

static void put_u64(uint8_t *output, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) output[i] = (uint8_t)(value >> (i * 8));
}

static uint64_t get_u64(const uint8_t *input) {
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= (uint64_t)input[i] << (i * 8);
    return value;
}

static bool valid_role(penalty_ble_link_role_t role) {
    return role == PENALTY_BLE_LINK_HOST || role == PENALTY_BLE_LINK_GUEST;
}

static bool valid_state(penalty_ble_link_state_t state) {
    return (unsigned)state <= PENALTY_BLE_LINK_ERROR;
}

size_t penalty_ble_advertisement_encode(
    const penalty_ble_advertisement_t *advertisement,
    uint8_t *output,
    size_t capacity) {
    if (!advertisement || !output || capacity < PENALTY_BLE_ADVERTISEMENT_BYTES ||
        advertisement->protocol_major != PENALTY_BLE_PROTOCOL_MAJOR ||
        advertisement->lobby_code > 9999) return 0;
    output[0] = advertisement->protocol_major;
    output[1] = advertisement->protocol_minor;
    put_u16(output + 2, advertisement->lobby_code);
    put_u32(output + 4, advertisement->offered_capabilities);
    put_u32(output + 8, advertisement->required_capabilities);
    return PENALTY_BLE_ADVERTISEMENT_BYTES;
}

bool penalty_ble_advertisement_decode(const uint8_t *input,
                                      size_t length,
                                      penalty_ble_advertisement_t *advertisement) {
    if (!input || !advertisement || length != PENALTY_BLE_ADVERTISEMENT_BYTES ||
        input[0] != PENALTY_BLE_PROTOCOL_MAJOR) return false;
    *advertisement = (penalty_ble_advertisement_t){
        .protocol_major = input[0],
        .protocol_minor = input[1],
        .lobby_code = get_u16(input + 2),
        .offered_capabilities = get_u32(input + 4),
        .required_capabilities = get_u32(input + 8),
    };
    return advertisement->lobby_code <= 9999 &&
           (advertisement->required_capabilities &
            ~advertisement->offered_capabilities) == 0;
}

size_t penalty_ble_protocol_info_encode(const penalty_ble_protocol_info_t *info,
                                        uint8_t *output,
                                        size_t capacity) {
    if (!info || !output || capacity < PENALTY_BLE_PROTOCOL_INFO_BYTES ||
        info->protocol_major != PENALTY_BLE_PROTOCOL_MAJOR ||
        (info->required_capabilities & ~info->offered_capabilities) != 0) return 0;
    output[0] = info->protocol_major;
    output[1] = info->protocol_minor;
    output[2] = info->snapshot_schema;
    output[3] = 0;
    put_u32(output + 4, info->offered_capabilities);
    put_u32(output + 8, info->required_capabilities);
    put_u64(output + 12, info->boot_instance_id);
    return PENALTY_BLE_PROTOCOL_INFO_BYTES;
}

bool penalty_ble_protocol_info_decode(const uint8_t *input,
                                      size_t length,
                                      penalty_ble_protocol_info_t *info) {
    if (!input || !info || length != PENALTY_BLE_PROTOCOL_INFO_BYTES || input[3] != 0)
        return false;
    *info = (penalty_ble_protocol_info_t){
        .protocol_major = input[0],
        .protocol_minor = input[1],
        .snapshot_schema = input[2],
        .offered_capabilities = get_u32(input + 4),
        .required_capabilities = get_u32(input + 8),
        .boot_instance_id = get_u64(input + 12),
    };
    return (info->required_capabilities & ~info->offered_capabilities) == 0;
}

penalty_ble_negotiate_result_t penalty_ble_protocol_negotiate(
    const penalty_ble_protocol_info_t *local,
    const penalty_ble_protocol_info_t *remote,
    uint32_t *negotiated_capabilities) {
    if (!local || !remote || !negotiated_capabilities ||
        local->protocol_major != remote->protocol_major)
        return PENALTY_BLE_NEGOTIATE_MAJOR_MISMATCH;
    if (local->snapshot_schema != remote->snapshot_schema)
        return PENALTY_BLE_NEGOTIATE_SCHEMA_MISMATCH;
    if ((local->required_capabilities & ~remote->offered_capabilities) != 0 ||
        (remote->required_capabilities & ~local->offered_capabilities) != 0)
        return PENALTY_BLE_NEGOTIATE_MISSING_CAPABILITY;
    *negotiated_capabilities = local->offered_capabilities &
                               remote->offered_capabilities;
    return PENALTY_BLE_NEGOTIATE_OK;
}

size_t penalty_ble_link_snapshot_encode(const penalty_ble_link_snapshot_t *snapshot,
                                        uint8_t *output,
                                        size_t capacity) {
    if (!snapshot || !output || capacity < PENALTY_BLE_LINK_SNAPSHOT_BYTES ||
        snapshot->schema != PENALTY_BLE_SNAPSHOT_SCHEMA ||
        !valid_role(snapshot->role) || !valid_state(snapshot->state) ||
        snapshot->lobby_code > 9999) return 0;
    output[0] = snapshot->schema;
    output[1] = (uint8_t)snapshot->role;
    output[2] = (uint8_t)snapshot->state;
    output[3] = snapshot->security_flags;
    put_u16(output + 4, snapshot->lobby_code);
    put_u16(output + 6, snapshot->epoch);
    put_u32(output + 8, snapshot->tx_sequence);
    put_u32(output + 12, snapshot->rx_sequence);
    put_u64(output + 16, snapshot->boot_instance_id);
    return PENALTY_BLE_LINK_SNAPSHOT_BYTES;
}

bool penalty_ble_link_snapshot_decode(const uint8_t *input,
                                      size_t length,
                                      penalty_ble_link_snapshot_t *snapshot) {
    if (!input || !snapshot || length != PENALTY_BLE_LINK_SNAPSHOT_BYTES ||
        input[0] != PENALTY_BLE_SNAPSHOT_SCHEMA ||
        !valid_role((penalty_ble_link_role_t)input[1]) ||
        !valid_state((penalty_ble_link_state_t)input[2])) return false;
    *snapshot = (penalty_ble_link_snapshot_t){
        .schema = input[0],
        .role = (penalty_ble_link_role_t)input[1],
        .state = (penalty_ble_link_state_t)input[2],
        .security_flags = input[3],
        .lobby_code = get_u16(input + 4),
        .epoch = get_u16(input + 6),
        .tx_sequence = get_u32(input + 8),
        .rx_sequence = get_u32(input + 12),
        .boot_instance_id = get_u64(input + 16),
    };
    return snapshot->lobby_code <= 9999;
}

void penalty_ble_session_core_init(penalty_ble_session_core_t *core,
                                   penalty_ble_link_role_t role,
                                   uint16_t lobby_code,
                                   uint64_t boot_instance_id) {
    memset(core, 0, sizeof(*core));
    core->role = role;
    core->state = PENALTY_BLE_LINK_OFF;
    core->lobby_code = lobby_code;
    core->local_info = (penalty_ble_protocol_info_t){
        .protocol_major = PENALTY_BLE_PROTOCOL_MAJOR,
        .protocol_minor = PENALTY_BLE_PROTOCOL_MINOR,
        .snapshot_schema = PENALTY_BLE_SNAPSHOT_SCHEMA,
        .offered_capabilities = PENALTY_BLE_M2_CAPABILITIES,
        .required_capabilities = PENALTY_BLE_M2_REQUIRED_CAPABILITIES,
        .boot_instance_id = boot_instance_id,
    };
}

bool penalty_ble_session_core_on_synced(penalty_ble_session_core_t *core) {
    if (!core || !valid_role(core->role) || core->lobby_code > 9999 ||
        (core->state != PENALTY_BLE_LINK_OFF &&
         core->state != PENALTY_BLE_LINK_STARTING)) return false;
    core->state = core->role == PENALTY_BLE_LINK_HOST
        ? PENALTY_BLE_LINK_ADVERTISING : PENALTY_BLE_LINK_SCANNING;
    return true;
}

bool penalty_ble_session_core_on_connecting(penalty_ble_session_core_t *core) {
    if (!core || core->role != PENALTY_BLE_LINK_GUEST ||
        core->state != PENALTY_BLE_LINK_SCANNING) return false;
    core->state = PENALTY_BLE_LINK_CONNECTING;
    return true;
}

bool penalty_ble_session_core_on_connected(penalty_ble_session_core_t *core) {
    if (!core || (core->state != PENALTY_BLE_LINK_ADVERTISING &&
                  core->state != PENALTY_BLE_LINK_CONNECTING)) return false;
    core->state = PENALTY_BLE_LINK_SECURING;
    return true;
}

bool penalty_ble_session_core_on_numeric(penalty_ble_session_core_t *core,
                                         uint32_t numeric_value) {
    if (!core || core->state != PENALTY_BLE_LINK_SECURING || numeric_value > 999999)
        return false;
    core->numeric_value = numeric_value;
    core->state = PENALTY_BLE_LINK_NUMERIC_CONFIRM;
    return true;
}

bool penalty_ble_session_core_confirm_numeric(penalty_ble_session_core_t *core,
                                              bool accepted) {
    if (!core || core->state != PENALTY_BLE_LINK_NUMERIC_CONFIRM) return false;
    core->numeric_value = 0;
    core->state = accepted ? PENALTY_BLE_LINK_SECURING : PENALTY_BLE_LINK_REJECTED;
    return true;
}

bool penalty_ble_session_core_on_secured(penalty_ble_session_core_t *core) {
    if (!core || core->state != PENALTY_BLE_LINK_SECURING) return false;
    core->state = core->role == PENALTY_BLE_LINK_HOST
        ? PENALTY_BLE_LINK_BINDING : PENALTY_BLE_LINK_DISCOVERING;
    return true;
}

bool penalty_ble_session_core_begin_binding(penalty_ble_session_core_t *core) {
    if (!core || core->state != PENALTY_BLE_LINK_DISCOVERING) return false;
    core->state = PENALTY_BLE_LINK_BINDING;
    return true;
}

bool penalty_ble_session_core_on_negotiated(
    penalty_ble_session_core_t *core,
    const penalty_ble_protocol_info_t *remote) {
    if (!core || core->state != PENALTY_BLE_LINK_BINDING || !remote) return false;
    uint32_t negotiated = 0;
    if (penalty_ble_protocol_negotiate(&core->local_info, remote, &negotiated) !=
        PENALTY_BLE_NEGOTIATE_OK) return false;
    core->peer_boot_instance_id = remote->boot_instance_id;
    core->negotiated_capabilities = negotiated;
    core->state = PENALTY_BLE_LINK_READY;
    return true;
}

void penalty_ble_session_core_on_disconnected(penalty_ble_session_core_t *core) {
    if (!core) return;
    core->numeric_value = 0;
    core->negotiated_capabilities = 0;
    core->peer_boot_instance_id = 0;
    core->state = PENALTY_BLE_LINK_DISCONNECTED;
}

void penalty_ble_session_core_fail(penalty_ble_session_core_t *core) {
    if (core) core->state = PENALTY_BLE_LINK_ERROR;
}
