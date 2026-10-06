#include "penalty_ble_session_core.h"

#include <assert.h>
#include <stdio.h>

static penalty_ble_protocol_info_t local_info(uint64_t boot_instance_id) {
    return (penalty_ble_protocol_info_t){
        .protocol_major = PENALTY_BLE_PROTOCOL_MAJOR,
        .protocol_minor = PENALTY_BLE_PROTOCOL_MINOR,
        .snapshot_schema = PENALTY_BLE_SNAPSHOT_SCHEMA,
        .offered_capabilities = PENALTY_BLE_M2_CAPABILITIES,
        .required_capabilities = PENALTY_BLE_M2_REQUIRED_CAPABILITIES,
        .boot_instance_id = boot_instance_id,
    };
}

static void advertisement_round_trips(void) {
    penalty_ble_advertisement_t advertisement = {
        .protocol_major = PENALTY_BLE_PROTOCOL_MAJOR,
        .protocol_minor = PENALTY_BLE_PROTOCOL_MINOR,
        .lobby_code = 4072,
        .offered_capabilities = PENALTY_BLE_M2_CAPABILITIES,
        .required_capabilities = PENALTY_BLE_M2_REQUIRED_CAPABILITIES,
    };
    uint8_t bytes[PENALTY_BLE_ADVERTISEMENT_BYTES];
    assert(penalty_ble_advertisement_encode(&advertisement, bytes, sizeof(bytes)) ==
           sizeof(bytes));

    penalty_ble_advertisement_t decoded;
    assert(penalty_ble_advertisement_decode(bytes, sizeof(bytes), &decoded));
    assert(decoded.protocol_major == advertisement.protocol_major);
    assert(decoded.protocol_minor == advertisement.protocol_minor);
    assert(decoded.lobby_code == advertisement.lobby_code);
    assert(decoded.offered_capabilities == advertisement.offered_capabilities);
    assert(decoded.required_capabilities == advertisement.required_capabilities);

    bytes[0] = PENALTY_BLE_PROTOCOL_MAJOR + 1;
    assert(!penalty_ble_advertisement_decode(bytes, sizeof(bytes), &decoded));
    assert(!penalty_ble_advertisement_decode(bytes, sizeof(bytes) - 1, &decoded));
}

static void protocol_info_is_explicit_and_negotiated(void) {
    penalty_ble_protocol_info_t info = local_info(UINT64_C(0x0102030405060708));
    uint8_t bytes[PENALTY_BLE_PROTOCOL_INFO_BYTES];
    assert(penalty_ble_protocol_info_encode(&info, bytes, sizeof(bytes)) == sizeof(bytes));

    penalty_ble_protocol_info_t decoded;
    assert(penalty_ble_protocol_info_decode(bytes, sizeof(bytes), &decoded));
    assert(decoded.protocol_major == info.protocol_major);
    assert(decoded.protocol_minor == info.protocol_minor);
    assert(decoded.snapshot_schema == info.snapshot_schema);
    assert(decoded.offered_capabilities == info.offered_capabilities);
    assert(decoded.required_capabilities == info.required_capabilities);
    assert(decoded.boot_instance_id == info.boot_instance_id);

    uint32_t negotiated = 0;
    assert(penalty_ble_protocol_negotiate(&info, &decoded, &negotiated) ==
           PENALTY_BLE_NEGOTIATE_OK);
    assert(negotiated == PENALTY_BLE_M2_CAPABILITIES);

    decoded.protocol_major++;
    assert(penalty_ble_protocol_negotiate(&info, &decoded, &negotiated) ==
           PENALTY_BLE_NEGOTIATE_MAJOR_MISMATCH);
    decoded = info;
    decoded.snapshot_schema++;
    assert(penalty_ble_protocol_negotiate(&info, &decoded, &negotiated) ==
           PENALTY_BLE_NEGOTIATE_SCHEMA_MISMATCH);
    decoded = info;
    decoded.offered_capabilities &= ~PENALTY_BLE_CAP_SC_NUMERIC_COMPARE;
    assert(penalty_ble_protocol_negotiate(&info, &decoded, &negotiated) ==
           PENALTY_BLE_NEGOTIATE_MISSING_CAPABILITY);
}

static void secure_state_machine_requires_user_confirmation(void) {
    penalty_ble_session_core_t host;
    penalty_ble_session_core_init(&host, PENALTY_BLE_LINK_HOST, 4072,
                                  UINT64_C(0x1111111111111111));
    assert(host.state == PENALTY_BLE_LINK_OFF);
    assert(penalty_ble_session_core_on_synced(&host));
    assert(host.state == PENALTY_BLE_LINK_ADVERTISING);
    assert(penalty_ble_session_core_on_connected(&host));
    assert(host.state == PENALTY_BLE_LINK_SECURING);
    assert(penalty_ble_session_core_on_numeric(&host, 42));
    assert(host.state == PENALTY_BLE_LINK_NUMERIC_CONFIRM);
    assert(host.numeric_value == 42);
    assert(penalty_ble_session_core_confirm_numeric(&host, true));
    assert(host.state == PENALTY_BLE_LINK_SECURING);
    assert(penalty_ble_session_core_on_secured(&host));
    assert(host.state == PENALTY_BLE_LINK_BINDING);

    penalty_ble_protocol_info_t peer = local_info(UINT64_C(0x2222222222222222));
    assert(penalty_ble_session_core_on_negotiated(&host, &peer));
    assert(host.state == PENALTY_BLE_LINK_READY);
    assert(host.peer_boot_instance_id == peer.boot_instance_id);
    assert(host.negotiated_capabilities == PENALTY_BLE_M2_CAPABILITIES);
    penalty_ble_session_core_on_disconnected(&host);
    assert(host.state == PENALTY_BLE_LINK_DISCONNECTED);

    penalty_ble_session_core_t guest;
    penalty_ble_session_core_init(&guest, PENALTY_BLE_LINK_GUEST, 4072, 9);
    assert(penalty_ble_session_core_on_synced(&guest));
    assert(guest.state == PENALTY_BLE_LINK_SCANNING);
    assert(penalty_ble_session_core_on_connecting(&guest));
    assert(guest.state == PENALTY_BLE_LINK_CONNECTING);
    assert(penalty_ble_session_core_on_connected(&guest));
    assert(penalty_ble_session_core_on_numeric(&guest, 999999));
    assert(penalty_ble_session_core_confirm_numeric(&guest, false));
    assert(guest.state == PENALTY_BLE_LINK_REJECTED);
}

static void snapshot_round_trips_without_padding(void) {
    penalty_ble_link_snapshot_t snapshot = {
        .schema = PENALTY_BLE_SNAPSHOT_SCHEMA,
        .role = PENALTY_BLE_LINK_HOST,
        .state = PENALTY_BLE_LINK_READY,
        .security_flags = PENALTY_BLE_SECURITY_ENCRYPTED |
                          PENALTY_BLE_SECURITY_AUTHENTICATED,
        .lobby_code = 4072,
        .epoch = 3,
        .tx_sequence = 19,
        .rx_sequence = 17,
        .boot_instance_id = UINT64_C(0x8877665544332211),
    };
    uint8_t bytes[PENALTY_BLE_LINK_SNAPSHOT_BYTES];
    assert(penalty_ble_link_snapshot_encode(&snapshot, bytes, sizeof(bytes)) ==
           sizeof(bytes));
    penalty_ble_link_snapshot_t decoded;
    assert(penalty_ble_link_snapshot_decode(bytes, sizeof(bytes), &decoded));
    assert(decoded.schema == snapshot.schema);
    assert(decoded.role == snapshot.role);
    assert(decoded.state == snapshot.state);
    assert(decoded.security_flags == snapshot.security_flags);
    assert(decoded.lobby_code == snapshot.lobby_code);
    assert(decoded.epoch == snapshot.epoch);
    assert(decoded.tx_sequence == snapshot.tx_sequence);
    assert(decoded.rx_sequence == snapshot.rx_sequence);
    assert(decoded.boot_instance_id == snapshot.boot_instance_id);
}

int main(void) {
    advertisement_round_trips();
    protocol_info_is_explicit_and_negotiated();
    secure_state_machine_requires_user_confirmation();
    snapshot_round_trips_without_padding();
    puts("penalty BLE session core tests passed");
    return 0;
}
