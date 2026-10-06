#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PENALTY_BLE_PROTOCOL_MAJOR 1
#define PENALTY_BLE_PROTOCOL_MINOR 0
#define PENALTY_BLE_HEADER_BYTES 22
#define PENALTY_BLE_MAX_PAYLOAD_BYTES 234
#define PENALTY_BLE_MAX_FRAME_BYTES \
    (PENALTY_BLE_HEADER_BYTES + PENALTY_BLE_MAX_PAYLOAD_BYTES)
#define PENALTY_BLE_FRAGMENT_HEADER_BYTES 8
#define PENALTY_BLE_MAX_FRAGMENT_BYTES 64

#define PENALTY_BLE_FLAG_ACK_REQUIRED 0x01u
#define PENALTY_BLE_FLAG_RESPONSE 0x02u

typedef enum {
    PENALTY_BLE_HELLO = 1,
    PENALTY_BLE_LOBBY_SETTINGS,
    PENALTY_BLE_COIN_RESULT,
    PENALTY_BLE_ROUND_OPEN,
    PENALTY_BLE_ACTION_COMMIT,
    PENALTY_BLE_REVEAL_REQUEST,
    PENALTY_BLE_ACTION_REVEAL,
    PENALTY_BLE_ROUND_RESULT,
    PENALTY_BLE_SNAPSHOT_REQUEST,
    PENALTY_BLE_SNAPSHOT,
    PENALTY_BLE_ANIMATION_START,
    PENALTY_BLE_PING,
    PENALTY_BLE_ABORT,
    PENALTY_BLE_RESUME,
    PENALTY_BLE_STATE_ACK,
    PENALTY_BLE_TRANSPORT_ACK,
} penalty_ble_message_type_t;

typedef struct {
    penalty_ble_message_type_t type;
    uint8_t flags;
    uint16_t epoch;
    uint32_t seq;
    uint32_t ack;
    uint16_t payload_len;
    uint8_t payload[PENALTY_BLE_MAX_PAYLOAD_BYTES];
} penalty_ble_message_t;

typedef enum {
    PENALTY_BLE_DECODE_OK,
    PENALTY_BLE_DECODE_BAD_LENGTH,
    PENALTY_BLE_DECODE_BAD_MAGIC,
    PENALTY_BLE_DECODE_UNSUPPORTED_VERSION,
    PENALTY_BLE_DECODE_BAD_TYPE,
    PENALTY_BLE_DECODE_BAD_CRC,
} penalty_ble_decode_result_t;

typedef struct {
    uint16_t epoch;
    uint32_t last_seq;
    bool initialized;
} penalty_ble_rx_window_t;

typedef enum {
    PENALTY_BLE_RX_NEW,
    PENALTY_BLE_RX_DUPLICATE,
    PENALTY_BLE_RX_STALE,
    PENALTY_BLE_RX_GAP,
    PENALTY_BLE_RX_WRONG_EPOCH,
} penalty_ble_rx_result_t;

typedef struct {
    uint16_t message_id;
    uint16_t total_len;
    uint16_t received;
    uint8_t fragment_count;
    uint8_t next_index;
    bool active;
    bool complete;
    uint8_t data[PENALTY_BLE_MAX_FRAME_BYTES];
} penalty_ble_reassembler_t;

typedef enum {
    PENALTY_BLE_REASSEMBLY_MORE,
    PENALTY_BLE_REASSEMBLY_COMPLETE,
    PENALTY_BLE_REASSEMBLY_DUPLICATE,
    PENALTY_BLE_REASSEMBLY_ERROR,
} penalty_ble_reassembly_result_t;

size_t penalty_ble_encode(const penalty_ble_message_t *message,
                          uint8_t *output,
                          size_t capacity);
penalty_ble_decode_result_t penalty_ble_decode(const uint8_t *wire,
                                               size_t wire_len,
                                               penalty_ble_message_t *message);
void penalty_ble_rx_window_init(penalty_ble_rx_window_t *window, uint16_t epoch);
penalty_ble_rx_result_t penalty_ble_rx_accept(penalty_ble_rx_window_t *window,
                                              uint16_t epoch,
                                              uint32_t seq);
uint8_t penalty_ble_fragment_count(size_t frame_len, size_t mtu_payload);
size_t penalty_ble_make_fragment(uint16_t message_id,
                                 const uint8_t *frame,
                                 size_t frame_len,
                                 size_t mtu_payload,
                                 uint8_t fragment_index,
                                 uint8_t *output,
                                 size_t capacity);
void penalty_ble_reassembler_init(penalty_ble_reassembler_t *reassembler);
penalty_ble_reassembly_result_t penalty_ble_reassembler_push(
    penalty_ble_reassembler_t *reassembler,
    const uint8_t *fragment,
    size_t fragment_len,
    uint8_t *output,
    size_t capacity,
    size_t *output_len);
