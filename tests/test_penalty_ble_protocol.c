#include "penalty_ble_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void codec_round_trip_and_validation(void) {
    penalty_ble_message_t message = {
        .type = PENALTY_BLE_ACTION_COMMIT,
        .flags = PENALTY_BLE_FLAG_ACK_REQUIRED,
        .epoch = 7,
        .seq = 0x01020304,
        .ack = 0x05060708,
        .payload_len = 5,
        .payload = {9, 8, 7, 6, 5},
    };
    uint8_t wire[PENALTY_BLE_MAX_FRAME_BYTES];
    size_t wire_len = penalty_ble_encode(&message, wire, sizeof(wire));
    assert(wire_len == PENALTY_BLE_HEADER_BYTES + message.payload_len);

    penalty_ble_message_t decoded;
    assert(penalty_ble_decode(wire, wire_len, &decoded) == PENALTY_BLE_DECODE_OK);
    assert(decoded.type == message.type && decoded.flags == message.flags);
    assert(decoded.epoch == message.epoch && decoded.seq == message.seq);
    assert(decoded.ack == message.ack && decoded.payload_len == message.payload_len);
    assert(memcmp(decoded.payload, message.payload, message.payload_len) == 0);

    wire[2] = PENALTY_BLE_PROTOCOL_MAJOR + 1;
    assert(penalty_ble_decode(wire, wire_len, &decoded) ==
           PENALTY_BLE_DECODE_UNSUPPORTED_VERSION);
    wire[2] = PENALTY_BLE_PROTOCOL_MAJOR;
    wire[wire_len - 1] ^= 0x80;
    assert(penalty_ble_decode(wire, wire_len, &decoded) == PENALTY_BLE_DECODE_BAD_CRC);
    assert(penalty_ble_decode(wire, PENALTY_BLE_HEADER_BYTES - 1, &decoded) ==
           PENALTY_BLE_DECODE_BAD_LENGTH);
}

static void sequence_window_is_idempotent(void) {
    penalty_ble_rx_window_t window;
    penalty_ble_rx_window_init(&window, 42);
    assert(penalty_ble_rx_accept(&window, 42, 10) == PENALTY_BLE_RX_NEW);
    assert(penalty_ble_rx_accept(&window, 42, 10) == PENALTY_BLE_RX_DUPLICATE);
    assert(penalty_ble_rx_accept(&window, 42, 9) == PENALTY_BLE_RX_STALE);
    assert(penalty_ble_rx_accept(&window, 41, 11) == PENALTY_BLE_RX_WRONG_EPOCH);
    assert(penalty_ble_rx_accept(&window, 42, 12) == PENALTY_BLE_RX_GAP);
    assert(window.last_seq == 10);
    assert(penalty_ble_rx_accept(&window, 42, 11) == PENALTY_BLE_RX_NEW);
    assert(penalty_ble_rx_accept(&window, 42, 12) == PENALTY_BLE_RX_NEW);

    penalty_ble_rx_window_init(&window, 42);
    assert(penalty_ble_rx_accept(&window, 42, UINT32_MAX) == PENALTY_BLE_RX_NEW);
    assert(penalty_ble_rx_accept(&window, 42, 0) == PENALTY_BLE_RX_NEW);
}

static void fragmentation_reassembles_once(void) {
    penalty_ble_message_t message = {
        .type = PENALTY_BLE_SNAPSHOT,
        .epoch = 3,
        .seq = 12,
        .ack = 11,
        .payload_len = PENALTY_BLE_MAX_PAYLOAD_BYTES,
    };
    for (size_t i = 0; i < message.payload_len; ++i) message.payload[i] = (uint8_t)i;
    uint8_t wire[PENALTY_BLE_MAX_FRAME_BYTES];
    size_t wire_len = penalty_ble_encode(&message, wire, sizeof(wire));
    assert(wire_len == sizeof(wire));

    uint8_t fragment[PENALTY_BLE_MAX_FRAGMENT_BYTES];
    penalty_ble_reassembler_t reassembler;
    penalty_ble_reassembler_init(&reassembler);
    uint8_t rebuilt[PENALTY_BLE_MAX_FRAME_BYTES];
    size_t rebuilt_len = 0;
    const size_t mtu_payload = 20;
    uint8_t count = penalty_ble_fragment_count(wire_len, mtu_payload);
    assert(count > 1);
    for (uint8_t index = 0; index < count; ++index) {
        size_t fragment_len = penalty_ble_make_fragment(
            0x1234, wire, wire_len, mtu_payload, index, fragment, sizeof(fragment));
        assert(fragment_len > PENALTY_BLE_FRAGMENT_HEADER_BYTES);
        penalty_ble_reassembly_result_t result = penalty_ble_reassembler_push(
            &reassembler, fragment, fragment_len, rebuilt, sizeof(rebuilt), &rebuilt_len);
        assert(result == (index + 1 == count
            ? PENALTY_BLE_REASSEMBLY_COMPLETE : PENALTY_BLE_REASSEMBLY_MORE));
    }
    assert(rebuilt_len == wire_len && memcmp(rebuilt, wire, wire_len) == 0);

    size_t last_len = penalty_ble_make_fragment(
        0x1234, wire, wire_len, mtu_payload, count - 1, fragment, sizeof(fragment));
    assert(penalty_ble_reassembler_push(&reassembler, fragment, last_len,
               rebuilt, sizeof(rebuilt), &rebuilt_len) == PENALTY_BLE_REASSEMBLY_DUPLICATE);

    penalty_ble_reassembler_init(&reassembler);
    size_t second_len = penalty_ble_make_fragment(
        0x9999, wire, wire_len, mtu_payload, 1, fragment, sizeof(fragment));
    assert(penalty_ble_reassembler_push(&reassembler, fragment, second_len,
               rebuilt, sizeof(rebuilt), &rebuilt_len) == PENALTY_BLE_REASSEMBLY_ERROR);
}

int main(void) {
    codec_round_trip_and_validation();
    sequence_window_is_idempotent();
    fragmentation_reassembles_once();
    puts("penalty BLE protocol tests passed");
    return 0;
}
