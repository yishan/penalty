#pragma once

#include "penalty_ble_session_core.h"

#include "esp_err.h"
#include "sdkconfig.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * Events that cannot be reconstructed later. Link state is polled through
 * penalty_ble_session_state() instead of being queued.
 */
typedef enum {
    PENALTY_BLE_SESSION_EVENT_NUMERIC_COMPARISON,
    PENALTY_BLE_SESSION_EVENT_READY,
    PENALTY_BLE_SESSION_EVENT_PING_REPLY,
    PENALTY_BLE_SESSION_EVENT_SNAPSHOT,
    PENALTY_BLE_SESSION_EVENT_DISCONNECTED,
    PENALTY_BLE_SESSION_EVENT_ERROR,
    PENALTY_BLE_SESSION_EVENT_MESSAGE,
    PENALTY_BLE_SESSION_EVENT_PING_RECEIVED,
    PENALTY_BLE_SESSION_EVENT_SNAPSHOT_SENT,
} penalty_ble_session_event_type_t;

typedef struct {
    penalty_ble_session_event_type_t type;
    penalty_ble_link_state_t state;
    int error;
    uint32_t numeric_value;
    uint32_t ping_token;
    uint32_t negotiated_capabilities;
    penalty_ble_link_snapshot_t snapshot;
    penalty_ble_message_t message;
} penalty_ble_session_event_t;

typedef struct {
    penalty_ble_link_role_t role;
    uint16_t lobby_code;
    uint64_t boot_instance_id;
    bool browse_lobbies; /* Guest waits for explicit selection before connecting. */
    bool m3_loop; /* Require the same experimental gameplay codec, not M2 diagnostics. */
    bool m4_match; /* Full match/recovery codec; RAM-only bonding, original peer only. */
} penalty_ble_session_config_t;

typedef struct {
    uint16_t code;
    int8_t rssi;
    uint8_t index;
} penalty_ble_lobby_t;

typedef struct {
    uint32_t heap_before_start;
    uint32_t heap_at_ready;
    uint32_t largest_block_at_ready;
    /* Sampled once per second, not an allocator-level session low-water mark. */
    uint32_t minimum_heap_sampled;
    uint32_t minimum_largest_block_sampled;
    uint32_t heap_after_stop;
    /* ESP-IDF reports stack high-water marks in bytes, not words. */
    uint32_t worker_stack_high_water;
    uint32_t host_stack_high_water;
    uint16_t negotiated_mtu;
    uint16_t internal_queue_peak;
    uint16_t public_queue_peak;
    uint16_t send_queue_peak;
} penalty_ble_session_metrics_t;

/*
 * Starts one bounded NimBLE session. It does not create UI objects and must be
 * called only after the multiplayer path has been selected.
 */
esp_err_t penalty_ble_session_start(const penalty_ble_session_config_t *config);

/*
 * Stops the session worker, then GAP activity, the connection, and NimBLE host.
 * A failure leaves the session running and retryable; call it again until it
 * returns ESP_OK.
 */
esp_err_t penalty_ble_session_stop(void);

esp_err_t penalty_ble_session_confirm_numeric(bool accepted);
esp_err_t penalty_ble_session_send_ping(uint32_t token);
esp_err_t penalty_ble_session_request_snapshot(void);
esp_err_t penalty_ble_session_send_message(const penalty_ble_message_t *message);
/* Call only after the multiplayer core has accepted this epoch's message. */
esp_err_t penalty_ble_session_ack_message(uint16_t epoch, uint32_t sequence);
esp_err_t penalty_ble_session_end_match(void);
bool penalty_ble_session_match_ended(void);
#if CONFIG_PENALTY_DEV_BLE_FAULTS
/* Queue-only; the BLE worker validates and terminates the actual connection.
 * Accepts 10 or 35 seconds, only on a pinned authenticated M4 match. */
esp_err_t penalty_ble_session_dev_disconnect(unsigned seconds);
#endif
size_t penalty_ble_session_get_lobbies(penalty_ble_lobby_t *lobbies, size_t capacity);
esp_err_t penalty_ble_session_select_lobby(uint8_t index);
bool penalty_ble_session_poll_event(penalty_ble_session_event_t *event,
                                    uint32_t timeout_ms);
penalty_ble_link_state_t penalty_ble_session_state(void);
bool penalty_ble_session_running(void);
void penalty_ble_session_get_metrics(penalty_ble_session_metrics_t *metrics);
