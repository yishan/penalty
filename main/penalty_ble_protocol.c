#include "penalty_ble_protocol.h"

#include <limits.h>
#include <string.h>

#define FRAME_MAGIC_0 0x50u
#define FRAME_MAGIC_1 0x44u
#define FRAGMENT_MAGIC_0 0xd5u
#define FRAGMENT_MAGIC_1 0xf1u

static void put_u16(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

static uint16_t get_u16(const uint8_t *input) {
    return (uint16_t)input[0] | (uint16_t)input[1] << 8;
}

static void put_u32(uint8_t *output, uint32_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
    output[2] = (uint8_t)(value >> 16);
    output[3] = (uint8_t)(value >> 24);
}

static uint32_t get_u32(const uint8_t *input) {
    return (uint32_t)input[0] | (uint32_t)input[1] << 8 |
           (uint32_t)input[2] << 16 | (uint32_t)input[3] << 24;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t length) {
    while (length--) {
        crc ^= *data++;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return crc;
}

static uint32_t frame_crc(const uint8_t *wire, uint16_t payload_len) {
    uint32_t crc = crc32_update(UINT32_MAX, wire, 18);
    crc = crc32_update(crc, wire + PENALTY_BLE_HEADER_BYTES, payload_len);
    return crc ^ UINT32_MAX;
}

static bool valid_type(uint8_t type) {
    return type >= PENALTY_BLE_HELLO && type <= PENALTY_BLE_TRANSPORT_ACK;
}

size_t penalty_ble_encode(const penalty_ble_message_t *message,
                          uint8_t *output,
                          size_t capacity) {
    if (!message || !output || !valid_type((uint8_t)message->type) ||
        message->payload_len > PENALTY_BLE_MAX_PAYLOAD_BYTES) return 0;
    size_t wire_len = PENALTY_BLE_HEADER_BYTES + message->payload_len;
    if (capacity < wire_len) return 0;
    output[0] = FRAME_MAGIC_0;
    output[1] = FRAME_MAGIC_1;
    output[2] = PENALTY_BLE_PROTOCOL_MAJOR;
    output[3] = PENALTY_BLE_PROTOCOL_MINOR;
    output[4] = (uint8_t)message->type;
    output[5] = message->flags;
    put_u16(output + 6, message->epoch);
    put_u32(output + 8, message->seq);
    put_u32(output + 12, message->ack);
    put_u16(output + 16, message->payload_len);
    memset(output + 18, 0, 4);
    memcpy(output + PENALTY_BLE_HEADER_BYTES, message->payload, message->payload_len);
    put_u32(output + 18, frame_crc(output, message->payload_len));
    return wire_len;
}

penalty_ble_decode_result_t penalty_ble_decode(const uint8_t *wire,
                                               size_t wire_len,
                                               penalty_ble_message_t *message) {
    if (!wire || !message || wire_len < PENALTY_BLE_HEADER_BYTES)
        return PENALTY_BLE_DECODE_BAD_LENGTH;
    if (wire[0] != FRAME_MAGIC_0 || wire[1] != FRAME_MAGIC_1)
        return PENALTY_BLE_DECODE_BAD_MAGIC;
    if (wire[2] != PENALTY_BLE_PROTOCOL_MAJOR)
        return PENALTY_BLE_DECODE_UNSUPPORTED_VERSION;
    if (!valid_type(wire[4])) return PENALTY_BLE_DECODE_BAD_TYPE;
    uint16_t payload_len = get_u16(wire + 16);
    if (payload_len > PENALTY_BLE_MAX_PAYLOAD_BYTES ||
        wire_len != PENALTY_BLE_HEADER_BYTES + payload_len)
        return PENALTY_BLE_DECODE_BAD_LENGTH;
    if (get_u32(wire + 18) != frame_crc(wire, payload_len))
        return PENALTY_BLE_DECODE_BAD_CRC;
    memset(message, 0, sizeof(*message));
    message->type = (penalty_ble_message_type_t)wire[4];
    message->flags = wire[5];
    message->epoch = get_u16(wire + 6);
    message->seq = get_u32(wire + 8);
    message->ack = get_u32(wire + 12);
    message->payload_len = payload_len;
    memcpy(message->payload, wire + PENALTY_BLE_HEADER_BYTES, payload_len);
    return PENALTY_BLE_DECODE_OK;
}

void penalty_ble_rx_window_init(penalty_ble_rx_window_t *window, uint16_t epoch) {
    *window = (penalty_ble_rx_window_t){.epoch = epoch};
}

penalty_ble_rx_result_t penalty_ble_rx_accept(penalty_ble_rx_window_t *window,
                                              uint16_t epoch,
                                              uint32_t seq) {
    if (epoch != window->epoch) return PENALTY_BLE_RX_WRONG_EPOCH;
    if (!window->initialized) {
        window->initialized = true;
        window->last_seq = seq;
        return PENALTY_BLE_RX_NEW;
    }
    int32_t delta = (int32_t)(seq - window->last_seq);
    if (delta == 0) return PENALTY_BLE_RX_DUPLICATE;
    if (delta < 0) return PENALTY_BLE_RX_STALE;
    if (delta > 1) return PENALTY_BLE_RX_GAP;
    window->last_seq = seq;
    return PENALTY_BLE_RX_NEW;
}

uint8_t penalty_ble_fragment_count(size_t frame_len, size_t mtu_payload) {
    if (!frame_len || frame_len > PENALTY_BLE_MAX_FRAME_BYTES ||
        mtu_payload <= PENALTY_BLE_FRAGMENT_HEADER_BYTES ||
        mtu_payload > PENALTY_BLE_MAX_FRAGMENT_BYTES) return 0;
    size_t data_bytes = mtu_payload - PENALTY_BLE_FRAGMENT_HEADER_BYTES;
    size_t count = (frame_len + data_bytes - 1) / data_bytes;
    return count <= UINT8_MAX ? (uint8_t)count : 0;
}

size_t penalty_ble_make_fragment(uint16_t message_id,
                                 const uint8_t *frame,
                                 size_t frame_len,
                                 size_t mtu_payload,
                                 uint8_t fragment_index,
                                 uint8_t *output,
                                 size_t capacity) {
    uint8_t count = penalty_ble_fragment_count(frame_len, mtu_payload);
    if (!frame || !output || !count || fragment_index >= count || capacity < mtu_payload)
        return 0;
    size_t data_bytes = mtu_payload - PENALTY_BLE_FRAGMENT_HEADER_BYTES;
    size_t offset = (size_t)fragment_index * data_bytes;
    size_t remaining = frame_len - offset;
    size_t take = remaining < data_bytes ? remaining : data_bytes;
    output[0] = FRAGMENT_MAGIC_0;
    output[1] = FRAGMENT_MAGIC_1;
    put_u16(output + 2, message_id);
    output[4] = fragment_index;
    output[5] = count;
    put_u16(output + 6, (uint16_t)frame_len);
    memcpy(output + PENALTY_BLE_FRAGMENT_HEADER_BYTES, frame + offset, take);
    return PENALTY_BLE_FRAGMENT_HEADER_BYTES + take;
}

void penalty_ble_reassembler_init(penalty_ble_reassembler_t *reassembler) {
    memset(reassembler, 0, sizeof(*reassembler));
}

penalty_ble_reassembly_result_t penalty_ble_reassembler_push(
    penalty_ble_reassembler_t *reassembler,
    const uint8_t *fragment,
    size_t fragment_len,
    uint8_t *output,
    size_t capacity,
    size_t *output_len) {
    if (!reassembler || !fragment || !output || !output_len ||
        fragment_len <= PENALTY_BLE_FRAGMENT_HEADER_BYTES ||
        fragment_len > PENALTY_BLE_MAX_FRAGMENT_BYTES ||
        fragment[0] != FRAGMENT_MAGIC_0 || fragment[1] != FRAGMENT_MAGIC_1)
        return PENALTY_BLE_REASSEMBLY_ERROR;
    uint16_t message_id = get_u16(fragment + 2);
    uint8_t index = fragment[4], count = fragment[5];
    uint16_t total_len = get_u16(fragment + 6);
    if (!count || index >= count || !total_len ||
        total_len > PENALTY_BLE_MAX_FRAME_BYTES || capacity < total_len)
        return PENALTY_BLE_REASSEMBLY_ERROR;
    if (reassembler->complete && reassembler->message_id == message_id)
        return PENALTY_BLE_REASSEMBLY_DUPLICATE;
    if (index == 0) {
        penalty_ble_reassembler_init(reassembler);
        reassembler->active = true;
        reassembler->message_id = message_id;
        reassembler->total_len = total_len;
        reassembler->fragment_count = count;
    }
    if (!reassembler->active || reassembler->message_id != message_id ||
        reassembler->total_len != total_len || reassembler->fragment_count != count)
        return PENALTY_BLE_REASSEMBLY_ERROR;
    if (index < reassembler->next_index) return PENALTY_BLE_REASSEMBLY_DUPLICATE;
    if (index != reassembler->next_index) return PENALTY_BLE_REASSEMBLY_ERROR;
    size_t data_len = fragment_len - PENALTY_BLE_FRAGMENT_HEADER_BYTES;
    if ((size_t)reassembler->received + data_len > total_len)
        return PENALTY_BLE_REASSEMBLY_ERROR;
    memcpy(reassembler->data + reassembler->received,
           fragment + PENALTY_BLE_FRAGMENT_HEADER_BYTES, data_len);
    reassembler->received += (uint16_t)data_len;
    ++reassembler->next_index;
    if (reassembler->next_index != count) return PENALTY_BLE_REASSEMBLY_MORE;
    if (reassembler->received != total_len) return PENALTY_BLE_REASSEMBLY_ERROR;
    memcpy(output, reassembler->data, total_len);
    *output_len = total_len;
    reassembler->active = false;
    reassembler->complete = true;
    return PENALTY_BLE_REASSEMBLY_COMPLETE;
}
