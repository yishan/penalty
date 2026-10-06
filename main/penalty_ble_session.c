#include "penalty_ble_session.h"
#include "penalty_ble_reliable.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_hs_mbuf.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

#define SESSION_INTERNAL_QUEUE_LENGTH 16u
#define SESSION_PUBLIC_QUEUE_LENGTH 8u
#define SESSION_TASK_STACK_BYTES 6144u
#define SESSION_STOP_TIMEOUT_MS 2500u
#define SESSION_SCAN_TIMEOUT_MS 60000u
#define SESSION_ADV_TIMEOUT_MS 60000u
#define SESSION_HANDSHAKE_TIMEOUT_MS 45000u
#define SESSION_STOP_SEND_TIMEOUT_MS 100u
#define SESSION_PREFERRED_MTU 128u
#define SESSION_EPOCH 1u /* M2 diagnostic; M4 advances per physical connection. */
#if CONFIG_PENALTY_DEV_BLE_FAULTS
#include "penalty_ble_fault.h"
static penalty_ble_fault_t s_dev_fault; /* BLE worker owns this; no UI access. */
#endif
#define SESSION_INVALID_HANDLE UINT16_MAX

/* ATT Read Response uses one opcode byte: 22 bytes fit even MTU 23. */
_Static_assert(PENALTY_BLE_M4_PROTOCOL_INFO_BYTES <= 23u-1u, "Protocol Info must fit minimum ATT MTU");

static const char *TAG = "penalty_ble";
static const char *DEVICE_NAME = "PenaltyDuel";

/* a48c0001-9b1e-4d7a-bc35-7c0b3a2e6f10 */
static const ble_uuid128_t SERVICE_UUID = BLE_UUID128_INIT(
    0x10, 0x6f, 0x2e, 0x3a, 0x0b, 0x7c, 0x35, 0xbc,
    0x7a, 0x4d, 0x1e, 0x9b, 0x01, 0x00, 0x8c, 0xa4);
static const ble_uuid128_t PROTOCOL_INFO_UUID = BLE_UUID128_INIT(
    0x10, 0x6f, 0x2e, 0x3a, 0x0b, 0x7c, 0x35, 0xbc,
    0x7a, 0x4d, 0x1e, 0x9b, 0x02, 0x00, 0x8c, 0xa4);
static const ble_uuid128_t CONTROL_POINT_UUID = BLE_UUID128_INIT(
    0x10, 0x6f, 0x2e, 0x3a, 0x0b, 0x7c, 0x35, 0xbc,
    0x7a, 0x4d, 0x1e, 0x9b, 0x03, 0x00, 0x8c, 0xa4);
static const ble_uuid128_t EVENT_STREAM_UUID = BLE_UUID128_INIT(
    0x10, 0x6f, 0x2e, 0x3a, 0x0b, 0x7c, 0x35, 0xbc,
    0x7a, 0x4d, 0x1e, 0x9b, 0x04, 0x00, 0x8c, 0xa4);
static const ble_uuid128_t SESSION_STATUS_UUID = BLE_UUID128_INIT(
    0x10, 0x6f, 0x2e, 0x3a, 0x0b, 0x7c, 0x35, 0xbc,
    0x7a, 0x4d, 0x1e, 0x9b, 0x05, 0x00, 0x8c, 0xa4);

typedef enum {
    CHARACTERISTIC_PROTOCOL_INFO = 1,
    CHARACTERISTIC_CONTROL_POINT,
    CHARACTERISTIC_EVENT_STREAM,
    CHARACTERISTIC_SESSION_STATUS,
} characteristic_id_t;

typedef enum {
    INTERNAL_SYNC,
    INTERNAL_ADV_MATCH,
    INTERNAL_CONNECTED,
    INTERNAL_CONNECT_FAILED,
    INTERNAL_DISCONNECTED,
    INTERNAL_NUMERIC,
    INTERNAL_ENCRYPTION,
    INTERNAL_MTU_DONE,
    INTERNAL_MTU_UPDATE,
    INTERNAL_DISCOVERY_COMPLETE,
    INTERNAL_SERVICE,
    INTERNAL_CHARACTERISTIC,
    INTERNAL_DESCRIPTOR,
    INTERNAL_PROTOCOL_INFO,
    INTERNAL_SUBSCRIBE_DONE,
    INTERNAL_WRITE_DONE,
    INTERNAL_INDICATION_DONE,
    INTERNAL_INBOUND_FRAGMENT,
    INTERNAL_SUBSCRIPTION,
    INTERNAL_CONFIRM_NUMERIC,
    INTERNAL_SEND_PING,
    INTERNAL_REQUEST_SNAPSHOT,
    INTERNAL_SELECT_LOBBY,
    INTERNAL_ACK_MESSAGE,
    INTERNAL_MATCH_END,
#if CONFIG_PENALTY_DEV_BLE_FAULTS
    INTERNAL_DEV_DISCONNECT,
#endif
    INTERNAL_STOP,
} internal_event_type_t;

typedef struct {
    internal_event_type_t type;
    int status;
    uint16_t conn_handle;
    uint16_t ack_epoch;
    uint32_t generation;
    union {
        struct {
            ble_addr_t address;
            uint16_t code;
            int8_t rssi;
        } advertisement;
        uint8_t lobby_index;
        uint32_t ack_sequence;
#if CONFIG_PENALTY_DEV_BLE_FAULTS
        struct { unsigned seconds; uint32_t generation; } dev_fault;
#endif
        struct {
            uint32_t value;
            bool accepted;
        } numeric;
        struct {
            bool encrypted;
            bool authenticated;
        } security;
        struct {
            uint16_t mtu;
        } mtu;
        struct {
            uint16_t start_handle;
            uint16_t end_handle;
            bool done;
        } service;
        struct {
            characteristic_id_t id;
            uint16_t def_handle;
            uint16_t value_handle;
            bool done;
        } characteristic;
        struct {
            uint16_t characteristic_handle;
            uint16_t descriptor_handle;
            bool is_cccd;
            bool done;
        } descriptor;
        struct {
            uint16_t length;
            uint8_t bytes[PENALTY_BLE_PROTOCOL_INFO_BYTES + 2];
        } protocol_info;
        struct {
            uint16_t length;
            uint8_t bytes[PENALTY_BLE_MAX_FRAGMENT_BYTES];
        } fragment;
        struct {
            bool enabled;
        } subscription;
        struct {
            uint32_t token;
        } ping;
    } data;
} internal_event_t;

typedef struct {
    uint8_t wire[PENALTY_BLE_MAX_FRAME_BYTES];
    size_t wire_length;
    uint16_t message_id;
    uint8_t fragment_index;
    uint8_t fragment_count;
    bool active;
    bool tracks_pending;
} outbound_frame_t;

typedef struct {
    uint16_t protocol_info;
    uint16_t control_point;
    uint16_t event_stream;
    uint16_t session_status;
    uint16_t event_cccd;
    uint16_t service_start;
    uint16_t service_end;
    uint16_t event_def;
    uint16_t status_def;
} peer_handles_t;

static QueueHandle_t s_internal_queue;
static QueueHandle_t s_public_queue;
static QueueHandle_t s_send_queue;
static bool s_browse_lobbies;
static bool s_m4, s_peer_pinned, s_end_requested;
static atomic_bool s_match_ended;
static ble_addr_t s_peer_address;
static uint64_t s_peer_boot, s_recovery_deadline_ms;
static uint16_t s_epoch, s_connection_count, s_fragment_id;
static uint32_t s_applied_sequence, s_ack_to_send;
static atomic_uint s_connection_generation;
static penalty_ble_reliable_t s_reliable;
static struct {
    ble_addr_t address;
    uint16_t code;
    int8_t rssi;
    uint64_t seen_ms;
} s_lobbies[4];
static uint64_t s_outbound_deadline_ms;
static SemaphoreHandle_t s_host_stopped;
static SemaphoreHandle_t s_worker_stopped;
static TaskHandle_t s_worker_task;
static atomic_bool s_running;
static atomic_bool s_accept_events;
static atomic_bool s_queue_overflow;
static atomic_bool s_public_overflow;
static atomic_bool s_host_stop_requested;
static atomic_bool s_nimble_initialized;
static atomic_bool s_host_started;
static atomic_bool s_worker_started;
static atomic_int s_published_state;
static atomic_uint s_queue_peak;
static atomic_uint s_public_queue_peak, s_send_queue_peak;
static atomic_uint s_peer_event_handle;
static penalty_ble_session_core_t s_core;
static penalty_ble_session_metrics_t s_metrics;
static portMUX_TYPE s_shared_lock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t s_status_bytes[PENALTY_BLE_LINK_SNAPSHOT_BYTES];
static penalty_ble_link_role_t s_role;
static uint16_t s_target_lobby_code;
static uint32_t s_offered_capabilities;
static uint32_t s_required_capabilities;
static uint8_t s_own_addr_type;
static uint16_t s_conn_handle = SESSION_INVALID_HANDLE;
static uint16_t s_protocol_info_value_handle;
static uint16_t s_control_point_value_handle;
static uint16_t s_event_stream_value_handle;
static uint16_t s_session_status_value_handle;
static uint8_t s_protocol_info_bytes[PENALTY_BLE_PROTOCOL_INFO_BYTES + 2];
static bool s_event_subscribed;
static bool s_secure;
static peer_handles_t s_peer_handles;
static penalty_ble_rx_window_t s_rx_window;
static penalty_ble_reassembler_t s_reassembler;
static outbound_frame_t s_outbound;
static uint32_t s_tx_sequence;
static uint16_t s_mtu_payload;
static uint64_t s_handshake_deadline_ms;

static int gap_event(struct ble_gap_event *event, void *arg);
void ble_store_config_init(void);
static void resume_discovery(void);

static void record_queue_peak(QueueHandle_t queue, atomic_uint *maximum) {
    unsigned waiting = (unsigned)uxQueueMessagesWaiting(queue);
    unsigned peak = atomic_load(maximum);
    while (waiting > peak && !atomic_compare_exchange_weak(maximum, &peak, waiting)) {}
}

/* BLE worker only. No logging or heap scans in GAP/input callbacks. */
static void sample_resources(void) {
    uint32_t heap = esp_get_free_heap_size();
    uint32_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    uint32_t stack = uxTaskGetStackHighWaterMark(NULL);
    taskENTER_CRITICAL(&s_shared_lock);
    if (heap < s_metrics.minimum_heap_sampled) s_metrics.minimum_heap_sampled = heap;
    if (largest < s_metrics.minimum_largest_block_sampled)
        s_metrics.minimum_largest_block_sampled = largest;
    s_metrics.worker_stack_high_water = stack;
    taskEXIT_CRITICAL(&s_shared_lock);
}

static bool queue_internal(const internal_event_t *event) {
    if (!atomic_load(&s_accept_events) || !s_internal_queue) return false;
    if (xQueueSend(s_internal_queue, event, 0) == pdTRUE) {
        record_queue_peak(s_internal_queue, &s_queue_peak);
        return true;
    }
    atomic_store(&s_queue_overflow, true);
    return false;
}

static uint64_t now_ms(void) {
    return (uint64_t)esp_timer_get_time() / 1000u;
}

/*
 * Only non-reconstructible events use the public queue; the link state is read
 * through penalty_ble_session_state(). An event is never evicted: a full queue
 * fails the session instead of silently losing a comparison value or result.
 */
static void publish_event(const penalty_ble_session_event_t *event) {
    if (!s_public_queue) return;
    if (xQueueSend(s_public_queue, event, 0) != pdTRUE)
        atomic_store(&s_public_overflow, true);
    else record_queue_peak(s_public_queue, &s_public_queue_peak);
}

static size_t build_status_snapshot(uint8_t *output, size_t capacity) {
    penalty_ble_link_snapshot_t snapshot = {
        .schema = PENALTY_BLE_SNAPSHOT_SCHEMA,
        .role = s_core.role,
        .state = s_core.state,
        .security_flags = (uint8_t)(s_secure
            ? PENALTY_BLE_SECURITY_ENCRYPTED | PENALTY_BLE_SECURITY_AUTHENTICATED
            : 0),
        .lobby_code = s_core.lobby_code,
        .epoch = s_epoch,
        .tx_sequence = s_tx_sequence,
        .rx_sequence = s_rx_window.initialized ? s_rx_window.last_seq : 0,
        .boot_instance_id = s_core.local_info.boot_instance_id,
    };
    return penalty_ble_link_snapshot_encode(&snapshot, output, capacity);
}

static void refresh_status_snapshot(void) {
    uint8_t snapshot[PENALTY_BLE_LINK_SNAPSHOT_BYTES];
    if (build_status_snapshot(snapshot, sizeof(snapshot)) != sizeof(snapshot)) return;
    taskENTER_CRITICAL(&s_shared_lock);
    memcpy(s_status_bytes, snapshot, sizeof(snapshot));
    taskEXIT_CRITICAL(&s_shared_lock);
}

static size_t read_status_snapshot(uint8_t *output, size_t capacity) {
    if (!output || capacity < sizeof(s_status_bytes)) return 0;
    taskENTER_CRITICAL(&s_shared_lock);
    memcpy(output, s_status_bytes, sizeof(s_status_bytes));
    taskEXIT_CRITICAL(&s_shared_lock);
    return sizeof(s_status_bytes);
}

static void publish_state(void) {
    refresh_status_snapshot();
    atomic_store(&s_published_state, s_core.state);
}

static void terminate_connection(int reason) {
    if (s_conn_handle != SESSION_INVALID_HANDLE)
        (void)ble_gap_terminate(s_conn_handle, reason);
}

/* Every failure leaves no live discovery, advertising, or connection behind. */
static void fail_session(int error) {
#if CONFIG_PENALTY_DEV_BLE_FAULTS
    penalty_ble_fault_reset(&s_dev_fault);
#endif
    s_handshake_deadline_ms = 0;
    s_outbound.active = false;
    (void)ble_gap_disc_cancel();
    (void)ble_gap_conn_cancel();
    (void)ble_gap_adv_stop();
    terminate_connection(BLE_ERR_REM_USER_CONN_TERM);
    penalty_ble_session_core_fail(&s_core);
    publish_state();
    const penalty_ble_session_event_t event = {
        .type = PENALTY_BLE_SESSION_EVENT_ERROR,
        .state = s_core.state,
        .error = error,
    };
    publish_event(&event);
}

static bool connection_is_authenticated(uint16_t conn_handle) {
    struct ble_gap_conn_desc description;
    return ble_gap_conn_find(conn_handle, &description) == 0 &&
           description.sec_state.encrypted && description.sec_state.authenticated;
}

static void recover_link(int error) {
    if (!s_m4 || !s_peer_pinned || s_match_ended) { fail_session(error); return; }
    s_outbound.active=false;
    if (!s_recovery_deadline_ms) s_recovery_deadline_ms=now_ms()+30000;
    terminate_connection(BLE_ERR_REM_USER_CONN_TERM);
    s_core.state=PENALTY_BLE_LINK_DISCONNECTED;
    publish_state();
    publish_event(&(penalty_ble_session_event_t){.type=PENALTY_BLE_SESSION_EVENT_DISCONNECTED,
        .state=s_core.state,.error=error});
}

static int gatt_access(uint16_t conn_handle,
                       uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *context,
                       void *arg) {
    (void)attr_handle;
    characteristic_id_t characteristic = (characteristic_id_t)(uintptr_t)arg;
    if (context->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        if (!connection_is_authenticated(conn_handle)) return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
        if (characteristic == CHARACTERISTIC_PROTOCOL_INFO) {
            return os_mbuf_append(context->om, s_protocol_info_bytes,
                                  PENALTY_BLE_PROTOCOL_INFO_BYTES + (s_m4 ? 2 : 0)) == 0
                ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        if (characteristic == CHARACTERISTIC_SESSION_STATUS) {
            uint8_t snapshot[PENALTY_BLE_LINK_SNAPSHOT_BYTES];
            size_t length = read_status_snapshot(snapshot, sizeof(snapshot));
            return length && os_mbuf_append(context->om, snapshot, length) == 0
                ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    }
    if (context->op == BLE_GATT_ACCESS_OP_WRITE_CHR &&
        characteristic == CHARACTERISTIC_CONTROL_POINT) {
        if (!connection_is_authenticated(conn_handle)) return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
        uint16_t length = OS_MBUF_PKTLEN(context->om);
        if (!length || length > PENALTY_BLE_MAX_FRAGMENT_BYTES)
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        internal_event_t event = {
            .type = INTERNAL_INBOUND_FRAGMENT,
            .conn_handle = conn_handle,
        };
        if (ble_hs_mbuf_to_flat(context->om, event.data.fragment.bytes,
                                sizeof(event.data.fragment.bytes),
                                &event.data.fragment.length) != 0)
            return BLE_ATT_ERR_UNLIKELY;
        return queue_internal(&event) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def GATT_SERVICES[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SERVICE_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &PROTOCOL_INFO_UUID.u,
                .access_cb = gatt_access,
                .arg = (void *)(uintptr_t)CHARACTERISTIC_PROTOCOL_INFO,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC |
                         BLE_GATT_CHR_F_READ_AUTHEN,
                .val_handle = &s_protocol_info_value_handle,
            },
            {
                .uuid = &CONTROL_POINT_UUID.u,
                .access_cb = gatt_access,
                .arg = (void *)(uintptr_t)CHARACTERISTIC_CONTROL_POINT,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC |
                         BLE_GATT_CHR_F_WRITE_AUTHEN,
                .val_handle = &s_control_point_value_handle,
            },
            {
                .uuid = &EVENT_STREAM_UUID.u,
                .access_cb = gatt_access,
                .arg = (void *)(uintptr_t)CHARACTERISTIC_EVENT_STREAM,
                .flags = BLE_GATT_CHR_F_INDICATE,
                .val_handle = &s_event_stream_value_handle,
            },
            {
                .uuid = &SESSION_STATUS_UUID.u,
                .access_cb = gatt_access,
                .arg = (void *)(uintptr_t)CHARACTERISTIC_SESSION_STATUS,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC |
                         BLE_GATT_CHR_F_READ_AUTHEN | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_session_status_value_handle,
            },
            {0},
        },
    },
    {0},
};

static int start_advertising(void) {
    struct ble_hs_adv_fields advertising = {0};
    advertising.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    advertising.uuids128 = &SERVICE_UUID;
    advertising.num_uuids128 = 1;
    advertising.uuids128_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&advertising);
    if (rc != 0) return rc;

    uint8_t service_data[16 + PENALTY_BLE_ADVERTISEMENT_BYTES];
    memcpy(service_data, SERVICE_UUID.value, 16);
    const penalty_ble_advertisement_t metadata = {
        .protocol_major = PENALTY_BLE_PROTOCOL_MAJOR,
        .protocol_minor = PENALTY_BLE_PROTOCOL_MINOR,
        .lobby_code = s_target_lobby_code,
        .offered_capabilities = s_offered_capabilities,
        .required_capabilities = s_required_capabilities,
    };
    if (penalty_ble_advertisement_encode(&metadata, service_data + 16,
                                         PENALTY_BLE_ADVERTISEMENT_BYTES) == 0)
        return BLE_HS_EINVAL;
    struct ble_hs_adv_fields response = {0};
    response.svc_data_uuid128 = service_data;
    response.svc_data_uuid128_len = sizeof(service_data);
    rc = ble_gap_adv_rsp_set_fields(&response);
    if (rc != 0) return rc;

    struct ble_gap_adv_params parameters = {0};
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    parameters.itvl_min = 160;
    parameters.itvl_max = 176;
    uint64_t current=now_ms();
    if (s_recovery_deadline_ms && s_recovery_deadline_ms<=current) return BLE_HS_ETIMEOUT;
    uint64_t remaining = s_recovery_deadline_ms ? s_recovery_deadline_ms-current : SESSION_ADV_TIMEOUT_MS;
    return ble_gap_adv_start(s_own_addr_type, NULL, (int32_t)remaining,
                             &parameters, gap_event, NULL);
}

static int start_scanning(void) {
    struct ble_gap_disc_params parameters = {0};
    parameters.passive = 0;
    parameters.filter_duplicates = s_browse_lobbies || s_recovery_deadline_ms ? 0 : 1;
    parameters.filter_policy = 0;
    parameters.limited = 0;
    uint64_t current=now_ms();
    if (s_recovery_deadline_ms && s_recovery_deadline_ms<=current) return BLE_HS_ETIMEOUT;
    uint64_t remaining = s_recovery_deadline_ms ? s_recovery_deadline_ms-current : SESSION_SCAN_TIMEOUT_MS;
    return ble_gap_disc(s_own_addr_type, (int32_t)remaining,
                        &parameters, gap_event, NULL);
}

static bool advertisement_matches(const struct ble_gap_disc_desc *description,
                                   uint16_t *code) {
    struct ble_hs_adv_fields fields;
    if (ble_hs_adv_parse_fields(&fields, description->data,
                                description->length_data) != 0 ||
        !fields.svc_data_uuid128 ||
        fields.svc_data_uuid128_len != 16 + PENALTY_BLE_ADVERTISEMENT_BYTES ||
        memcmp(fields.svc_data_uuid128, SERVICE_UUID.value, 16) != 0)
        return false;
    if (s_peer_pinned && ble_addr_cmp(&description->addr,&s_peer_address)) return false;
    penalty_ble_advertisement_t advertisement;
    if (!penalty_ble_advertisement_decode(fields.svc_data_uuid128 + 16,
                                          PENALTY_BLE_ADVERTISEMENT_BYTES,
                                          &advertisement) ||
        (!s_browse_lobbies && advertisement.lobby_code != s_target_lobby_code))
        return false;
    *code = advertisement.lobby_code;
    return (s_required_capabilities &
            ~advertisement.offered_capabilities) == 0 &&
           (advertisement.required_capabilities &
            ~s_offered_capabilities) == 0;
}

static int gap_event(struct ble_gap_event *event, void *arg) {
    (void)arg;
    internal_event_t internal = {0};
    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        if (s_role == PENALTY_BLE_LINK_GUEST &&
            advertisement_matches(&event->disc, &internal.data.advertisement.code)) {
            internal.type = INTERNAL_ADV_MATCH;
            internal.data.advertisement.address = event->disc.addr;
            internal.data.advertisement.rssi = event->disc.rssi;
            (void)queue_internal(&internal);
        }
        break;
    case BLE_GAP_EVENT_CONNECT:
        internal.type = event->connect.status == 0
            ? INTERNAL_CONNECTED : INTERNAL_CONNECT_FAILED;
        internal.status = event->connect.status;
        internal.conn_handle = event->connect.conn_handle;
        (void)queue_internal(&internal);
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        internal.type = INTERNAL_DISCONNECTED;
        internal.status = event->disconnect.reason;
        internal.conn_handle = event->disconnect.conn.conn_handle;
        (void)queue_internal(&internal);
        break;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
        if (event->passkey.params.action != BLE_SM_IOACT_NUMCMP) {
            internal.type = INTERNAL_CONNECT_FAILED;
            internal.status = BLE_HS_EAUTHEN;
        } else {
            internal.type = INTERNAL_NUMERIC;
            internal.conn_handle = event->passkey.conn_handle;
            internal.data.numeric.value = event->passkey.params.numcmp;
        }
        (void)queue_internal(&internal);
        break;
    case BLE_GAP_EVENT_ENC_CHANGE: {
        struct ble_gap_conn_desc description;
        internal.type = INTERNAL_ENCRYPTION;
        internal.status = event->enc_change.status;
        internal.conn_handle = event->enc_change.conn_handle;
        if (event->enc_change.status == 0 &&
            ble_gap_conn_find(event->enc_change.conn_handle, &description) == 0) {
            internal.data.security.encrypted = description.sec_state.encrypted;
            internal.data.security.authenticated = description.sec_state.authenticated;
        }
        (void)queue_internal(&internal);
        break;
    }
    case BLE_GAP_EVENT_NOTIFY_RX:
        if (event->notify_rx.attr_handle ==
            (uint16_t)atomic_load(&s_peer_event_handle)) {
            uint16_t length = OS_MBUF_PKTLEN(event->notify_rx.om);
            if (length && length <= PENALTY_BLE_MAX_FRAGMENT_BYTES) {
                internal.type = INTERNAL_INBOUND_FRAGMENT;
                internal.conn_handle = event->notify_rx.conn_handle;
                if (ble_hs_mbuf_to_flat(event->notify_rx.om,
                                        internal.data.fragment.bytes,
                                        sizeof(internal.data.fragment.bytes),
                                        &internal.data.fragment.length) == 0)
                    (void)queue_internal(&internal);
            }
        }
        break;
    case BLE_GAP_EVENT_NOTIFY_TX:
        if (event->notify_tx.indication &&
            event->notify_tx.attr_handle == s_event_stream_value_handle &&
            event->notify_tx.status != 0) {
            internal.type = INTERNAL_INDICATION_DONE;
            internal.status = event->notify_tx.status == BLE_HS_EDONE
                ? 0 : event->notify_tx.status;
            internal.conn_handle = event->notify_tx.conn_handle;
            (void)queue_internal(&internal);
        }
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == s_event_stream_value_handle) {
            internal.type = INTERNAL_SUBSCRIPTION;
            internal.conn_handle = event->subscribe.conn_handle;
            internal.data.subscription.enabled = event->subscribe.cur_indicate;
            (void)queue_internal(&internal);
        }
        break;
    case BLE_GAP_EVENT_MTU:
        internal.type = INTERNAL_MTU_UPDATE;
        internal.conn_handle = event->mtu.conn_handle;
        internal.data.mtu.mtu = event->mtu.value;
        (void)queue_internal(&internal);
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        internal.type = INTERNAL_DISCOVERY_COMPLETE;
        internal.status = event->adv_complete.reason;
        (void)queue_internal(&internal);
        break;
    case BLE_GAP_EVENT_DISC_COMPLETE:
        internal.type = INTERNAL_DISCOVERY_COMPLETE;
        internal.status = event->disc_complete.reason;
        (void)queue_internal(&internal);
        break;
    default:
        break;
    }
    return 0;
}

static void on_reset(int reason) {
    internal_event_t event = {.type = INTERNAL_CONNECT_FAILED, .status = reason};
    (void)queue_internal(&event);
}

static void on_sync(void) {
    internal_event_t event = {.type = INTERNAL_SYNC};
    int rc = ble_hs_util_ensure_addr(0);
    if (rc == 0) rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        event.type = INTERNAL_CONNECT_FAILED;
        event.status = rc;
    }
    (void)queue_internal(&event);
}

static int service_discovered(uint16_t conn_handle,
                              const struct ble_gatt_error *error,
                              const struct ble_gatt_svc *service,
                              void *arg) {
    internal_event_t event = {
        .type = INTERNAL_SERVICE,
        .generation = (uint32_t)(uintptr_t)arg,
        .status = error->status,
        .conn_handle = conn_handle,
    };
    if (error->status == 0 && service) {
        event.data.service.start_handle = service->start_handle;
        event.data.service.end_handle = service->end_handle;
    } else if (error->status == BLE_HS_EDONE) {
        event.status = 0;
        event.data.service.done = true;
    }
    (void)queue_internal(&event);
    return 0;
}

static characteristic_id_t characteristic_id(const ble_uuid_t *uuid) {
    if (ble_uuid_cmp(uuid, &PROTOCOL_INFO_UUID.u) == 0)
        return CHARACTERISTIC_PROTOCOL_INFO;
    if (ble_uuid_cmp(uuid, &CONTROL_POINT_UUID.u) == 0)
        return CHARACTERISTIC_CONTROL_POINT;
    if (ble_uuid_cmp(uuid, &EVENT_STREAM_UUID.u) == 0)
        return CHARACTERISTIC_EVENT_STREAM;
    if (ble_uuid_cmp(uuid, &SESSION_STATUS_UUID.u) == 0)
        return CHARACTERISTIC_SESSION_STATUS;
    return 0;
}

static int characteristic_discovered(uint16_t conn_handle,
                                     const struct ble_gatt_error *error,
                                     const struct ble_gatt_chr *characteristic,
                                     void *arg) {
    internal_event_t event = {
        .type = INTERNAL_CHARACTERISTIC,
        .generation = (uint32_t)(uintptr_t)arg,
        .status = error->status,
        .conn_handle = conn_handle,
    };
    if (error->status == 0 && characteristic) {
        event.data.characteristic.id = characteristic_id(&characteristic->uuid.u);
        event.data.characteristic.def_handle = characteristic->def_handle;
        event.data.characteristic.value_handle = characteristic->val_handle;
    } else if (error->status == BLE_HS_EDONE) {
        event.status = 0;
        event.data.characteristic.done = true;
    }
    (void)queue_internal(&event);
    return 0;
}

static int descriptor_discovered(uint16_t conn_handle,
                                 const struct ble_gatt_error *error,
                                 uint16_t characteristic_handle,
                                 const struct ble_gatt_dsc *descriptor,
                                 void *arg) {
    internal_event_t event = {
        .type = INTERNAL_DESCRIPTOR,
        .generation = (uint32_t)(uintptr_t)arg,
        .status = error->status,
        .conn_handle = conn_handle,
    };
    if (error->status == 0 && descriptor) {
        event.data.descriptor.characteristic_handle = characteristic_handle;
        event.data.descriptor.descriptor_handle = descriptor->handle;
        event.data.descriptor.is_cccd = ble_uuid_cmp(
            &descriptor->uuid.u,
            BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16)) == 0;
    } else if (error->status == BLE_HS_EDONE) {
        event.status = 0;
        event.data.descriptor.done = true;
    }
    (void)queue_internal(&event);
    return 0;
}

static int protocol_info_read(uint16_t conn_handle,
                              const struct ble_gatt_error *error,
                              struct ble_gatt_attr *attribute,
                              void *arg) {
    if ((uint32_t)(uintptr_t)arg!=atomic_load(&s_connection_generation)) return 0;
    internal_event_t event = {
        .type = INTERNAL_PROTOCOL_INFO,
        .generation = (uint32_t)(uintptr_t)arg,
        .status = error->status,
        .conn_handle = conn_handle,
    };
    if (error->status == 0 && attribute && attribute->om) {
        uint16_t length = OS_MBUF_PKTLEN(attribute->om);
        if (length == PENALTY_BLE_PROTOCOL_INFO_BYTES + (s_m4 ? 2 : 0) &&
            ble_hs_mbuf_to_flat(attribute->om, event.data.protocol_info.bytes,
                                sizeof(event.data.protocol_info.bytes),
                                &event.data.protocol_info.length) != 0)
            event.status = BLE_HS_EINVAL;
        else if (length != PENALTY_BLE_PROTOCOL_INFO_BYTES + (s_m4 ? 2 : 0))
            event.status = BLE_HS_EINVAL;
    }
    (void)queue_internal(&event);
    return 0;
}

static int subscribe_complete(uint16_t conn_handle,
                              const struct ble_gatt_error *error,
                              struct ble_gatt_attr *attribute,
                              void *arg) {
    (void)attribute;
    internal_event_t event = {
        .type = INTERNAL_SUBSCRIBE_DONE,
        .generation = (uint32_t)(uintptr_t)arg,
        .status = error->status,
        .conn_handle = conn_handle,
    };
    (void)queue_internal(&event);
    return 0;
}

static int write_complete(uint16_t conn_handle,
                          const struct ble_gatt_error *error,
                          struct ble_gatt_attr *attribute,
                          void *arg) {
    (void)attribute;
    internal_event_t event = {
        .type = INTERNAL_WRITE_DONE,
        .generation = (uint32_t)(uintptr_t)arg,
        .status = error->status,
        .conn_handle = conn_handle,
    };
    (void)queue_internal(&event);
    return 0;
}

static int mtu_complete(uint16_t conn_handle,
                        const struct ble_gatt_error *error,
                        uint16_t mtu,
                        void *arg) {
    internal_event_t event = {
        .type = INTERNAL_MTU_DONE,
        .generation = (uint32_t)(uintptr_t)arg,
        .status = error->status,
        .conn_handle = conn_handle,
    };
    event.data.mtu.mtu = mtu;
    (void)queue_internal(&event);
    return 0;
}

static void host_task(void *arg) {
    (void)arg;
    nimble_port_run();
    uint32_t stack = uxTaskGetStackHighWaterMark(NULL);
    taskENTER_CRITICAL(&s_shared_lock);
    s_metrics.host_stack_high_water = stack;
    taskEXIT_CRITICAL(&s_shared_lock);
    /*
     * The FreeRTOS port deinit deletes this task, so it must not run here:
     * signal first, then let penalty_ble_session_stop() delete the parked task.
     */
    if (s_host_stopped) xSemaphoreGive(s_host_stopped);
    vTaskSuspend(NULL);
}

static uint16_t fragment_payload_size(void) {
    uint16_t payload = s_mtu_payload;
    if (payload < PENALTY_BLE_FRAGMENT_HEADER_BYTES + 1) payload = 20;
    if (payload > PENALTY_BLE_MAX_FRAGMENT_BYTES)
        payload = PENALTY_BLE_MAX_FRAGMENT_BYTES;
    return payload;
}

static int send_next_fragment(void) {
    if (!s_outbound.active || s_outbound.fragment_index >= s_outbound.fragment_count)
        return BLE_HS_EINVAL;
    uint8_t fragment[PENALTY_BLE_MAX_FRAGMENT_BYTES];
    size_t length = penalty_ble_make_fragment(
        s_outbound.message_id, s_outbound.wire, s_outbound.wire_length,
        fragment_payload_size(), s_outbound.fragment_index,
        fragment, sizeof(fragment));
    if (!length) return BLE_HS_EINVAL;
    if (s_core.role == PENALTY_BLE_LINK_GUEST) {
        return ble_gattc_write_flat(s_conn_handle, s_peer_handles.control_point,
                                    fragment, length, write_complete, (void *)(uintptr_t)atomic_load(&s_connection_generation));
    }
    if (!s_event_subscribed) return BLE_HS_ENOTCONN;
    struct os_mbuf *buffer = ble_hs_mbuf_from_flat(fragment, (uint16_t)length);
    if (!buffer) return BLE_HS_ENOMEM;
    return ble_gatts_indicate_custom(s_conn_handle, s_event_stream_value_handle,
                                     buffer);
}

static int prepare_message(penalty_ble_message_type_t type,
                           uint8_t flags,
                           const void *payload,
                           uint16_t payload_length) {
    if (s_outbound.active || payload_length > PENALTY_BLE_MAX_PAYLOAD_BYTES ||
        (s_m4 && s_reliable.pending && type!=PENALTY_BLE_TRANSPORT_ACK))
        return BLE_HS_EBUSY;
    if (type!=PENALTY_BLE_TRANSPORT_ACK && s_tx_sequence==UINT32_MAX) return BLE_HS_EINVAL;
    penalty_ble_message_t message = {
        .type = type,
        .flags = flags,
        .epoch = s_epoch,
        .seq = type==PENALTY_BLE_TRANSPORT_ACK ? 0 : ++s_tx_sequence,
        .ack = s_m4 ? s_applied_sequence : (s_rx_window.initialized ? s_rx_window.last_seq : 0),
        .payload_len = payload_length,
    };
    if (payload_length) memcpy(message.payload, payload, payload_length);
    refresh_status_snapshot();
    s_outbound.wire_length = penalty_ble_encode(
        &message, s_outbound.wire, sizeof(s_outbound.wire));
    if (!s_outbound.wire_length) return BLE_HS_EINVAL;
    s_outbound.message_id = ++s_fragment_id;
    s_outbound.tracks_pending = s_m4 && type!=PENALTY_BLE_HELLO &&
        type!=PENALTY_BLE_TRANSPORT_ACK && (flags&PENALTY_BLE_FLAG_ACK_REQUIRED);
    if (s_outbound.tracks_pending && !penalty_ble_reliable_begin(&s_reliable,&message)) return BLE_HS_EBUSY;
    s_outbound.fragment_index = 0;
    s_outbound.fragment_count = penalty_ble_fragment_count(
        s_outbound.wire_length, fragment_payload_size());
    s_outbound.active = s_outbound.fragment_count != 0;
    s_outbound_deadline_ms = now_ms() + 5000;
    if (!s_outbound.active) return BLE_HS_EINVAL;
    int rc = send_next_fragment();
    if (rc != 0) s_outbound.active = false;
    return rc;
}

static void outbound_fragment_done(int status) {
    if (!s_outbound.active) return;
    if (status != 0) {
        s_outbound.active = false;
        recover_link(status);
        return;
    }
    ++s_outbound.fragment_index;
    if (s_outbound.fragment_index >= s_outbound.fragment_count) {
        s_outbound.active = false;
        if (s_outbound.tracks_pending) penalty_ble_reliable_sent(&s_reliable,now_ms());
        return;
    }
    int rc = send_next_fragment();
    if (rc != 0) {
        s_outbound.active = false;
        recover_link(rc);
    }
}

static void set_mtu(uint16_t mtu) {
    if (mtu < 23) mtu = 23;
    s_mtu_payload = mtu - 3;
    taskENTER_CRITICAL(&s_shared_lock);
    s_metrics.negotiated_mtu = mtu;
    taskEXIT_CRITICAL(&s_shared_lock);
}

static void publish_ready(void) {
    s_handshake_deadline_ms = 0;
    if (s_m4) { s_peer_pinned=true; s_peer_boot=s_core.peer_boot_instance_id; s_browse_lobbies=false; }
    uint32_t heap = esp_get_free_heap_size();
    uint32_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    taskENTER_CRITICAL(&s_shared_lock);
    s_metrics.heap_at_ready = heap;
    s_metrics.largest_block_at_ready = largest;
    taskEXIT_CRITICAL(&s_shared_lock);
    publish_state();
    const penalty_ble_session_event_t event = {
        .type = PENALTY_BLE_SESSION_EVENT_READY,
        .state = s_core.state,
        .negotiated_capabilities = s_core.negotiated_capabilities,
    };
    publish_event(&event);
}

static void process_message(const penalty_ble_message_t *message) {
    if (s_m4 && message->type==PENALTY_BLE_TRANSPORT_ACK) {
        if (message->epoch==s_epoch && message->payload_len==0 && message->seq==0)
            (void)penalty_ble_reliable_ack(&s_reliable,message->epoch,message->ack);
        return;
    }
    penalty_ble_rx_result_t accepted = penalty_ble_rx_accept(
        &s_rx_window, message->epoch, message->seq);
    if (accepted == PENALTY_BLE_RX_DUPLICATE || accepted == PENALTY_BLE_RX_STALE) {
        if (s_m4 && message->seq<=s_applied_sequence) s_ack_to_send=s_applied_sequence;
        return;
    }
    if (accepted==PENALTY_BLE_RX_WRONG_EPOCH && s_m4) return;
    if (accepted != PENALTY_BLE_RX_NEW) {
        recover_link(BLE_HS_EBADDATA);
        return;
    }
    refresh_status_snapshot();

    if (message->type == PENALTY_BLE_HELLO) {
        penalty_ble_protocol_info_t remote;
        if (message->payload_len != PENALTY_BLE_PROTOCOL_INFO_BYTES ||
            !penalty_ble_protocol_info_decode(message->payload,
                                              message->payload_len, &remote) ||
            (s_peer_pinned && remote.boot_instance_id!=s_peer_boot) ||
            !penalty_ble_session_core_on_negotiated(&s_core, &remote)) {
            fail_session(BLE_HS_EAUTHEN);
            return;
        }
        if (s_core.role == PENALTY_BLE_LINK_HOST &&
            !(message->flags & PENALTY_BLE_FLAG_RESPONSE)) {
            int rc = prepare_message(PENALTY_BLE_HELLO,
                                     PENALTY_BLE_FLAG_RESPONSE,
                                     s_protocol_info_bytes,
                                     PENALTY_BLE_PROTOCOL_INFO_BYTES);
            if (rc != 0) {
                fail_session(rc);
                return;
            }
        }
        publish_ready();
        return;
    }

    if (s_core.state != PENALTY_BLE_LINK_READY) return;
    if (message->type == PENALTY_BLE_PING) {
        if (message->payload_len != sizeof(uint32_t)) return;
        if (s_core.role == PENALTY_BLE_LINK_HOST &&
            !(message->flags & PENALTY_BLE_FLAG_RESPONSE)) {
            int rc = prepare_message(PENALTY_BLE_PING,
                                     PENALTY_BLE_FLAG_RESPONSE,
                                     message->payload, message->payload_len);
            if (rc != 0) fail_session(rc);
            else publish_event(&(penalty_ble_session_event_t){
                .type = PENALTY_BLE_SESSION_EVENT_PING_RECEIVED,
            });
        } else if (s_core.role == PENALTY_BLE_LINK_GUEST &&
                   (message->flags & PENALTY_BLE_FLAG_RESPONSE)) {
            uint32_t token = (uint32_t)message->payload[0] |
                             (uint32_t)message->payload[1] << 8 |
                             (uint32_t)message->payload[2] << 16 |
                             (uint32_t)message->payload[3] << 24;
            const penalty_ble_session_event_t event = {
                .type = PENALTY_BLE_SESSION_EVENT_PING_REPLY,
                .state = s_core.state,
                .ping_token = token,
            };
            publish_event(&event);
        }
        return;
    }
    if (!s_m4 && message->type == PENALTY_BLE_SNAPSHOT_REQUEST &&
        s_core.role == PENALTY_BLE_LINK_HOST) {
        uint8_t snapshot[PENALTY_BLE_LINK_SNAPSHOT_BYTES];
        size_t length = build_status_snapshot(snapshot, sizeof(snapshot));
        int rc = length ? prepare_message(PENALTY_BLE_SNAPSHOT, 0,
                                          snapshot, (uint16_t)length)
                        : BLE_HS_EINVAL;
        if (rc != 0) fail_session(rc);
        else publish_event(&(penalty_ble_session_event_t){
            .type = PENALTY_BLE_SESSION_EVENT_SNAPSHOT_SENT,
        });
        return;
    }
    if (!s_m4 && message->type == PENALTY_BLE_SNAPSHOT &&
        s_core.role == PENALTY_BLE_LINK_GUEST) {
        penalty_ble_link_snapshot_t snapshot;
        if (!penalty_ble_link_snapshot_decode(message->payload,
                                              message->payload_len, &snapshot))
            return;
        const penalty_ble_session_event_t event = {
            .type = PENALTY_BLE_SESSION_EVENT_SNAPSHOT,
            .state = s_core.state,
            .snapshot = snapshot,
        };
        publish_event(&event);
        return;
    }
    if ((s_m4 || message->type != PENALTY_BLE_SNAPSHOT_REQUEST) &&
        message->type != PENALTY_BLE_PING) {
        const penalty_ble_session_event_t event = {
            .type = PENALTY_BLE_SESSION_EVENT_MESSAGE,
            .state = s_core.state, .message = *message,
        };
        publish_event(&event);
    }
}

static void process_fragment(const internal_event_t *event) {
    uint8_t wire[PENALTY_BLE_MAX_FRAME_BYTES];
    size_t wire_length = 0;
    penalty_ble_reassembly_result_t result = penalty_ble_reassembler_push(
        &s_reassembler, event->data.fragment.bytes,
        event->data.fragment.length, wire, sizeof(wire), &wire_length);
    if (result == PENALTY_BLE_REASSEMBLY_MORE ||
        result == PENALTY_BLE_REASSEMBLY_DUPLICATE) return;
    if (result != PENALTY_BLE_REASSEMBLY_COMPLETE) {
        fail_session(BLE_HS_EBADDATA);
        return;
    }
    penalty_ble_message_t message;
    if (penalty_ble_decode(wire, wire_length, &message) != PENALTY_BLE_DECODE_OK) {
        fail_session(BLE_HS_EBADDATA);
        return;
    }
    process_message(&message);
}

static void reset_peer_handles(void) {
    s_peer_handles = (peer_handles_t){
        .protocol_info = SESSION_INVALID_HANDLE,
        .control_point = SESSION_INVALID_HANDLE,
        .event_stream = SESSION_INVALID_HANDLE,
        .session_status = SESSION_INVALID_HANDLE,
        .event_cccd = SESSION_INVALID_HANDLE,
        .service_start = SESSION_INVALID_HANDLE,
        .service_end = SESSION_INVALID_HANDLE,
        .event_def = SESSION_INVALID_HANDLE,
        .status_def = SESSION_INVALID_HANDLE,
    };
}

static void resume_discovery(void) {
    if (now_ms()>=s_recovery_deadline_ms) { fail_session(BLE_HS_ETIMEOUT); return; }
    s_handshake_deadline_ms=s_recovery_deadline_ms;
#if CONFIG_PENALTY_DEV_BLE_FAULTS
    if (s_dev_fault.active) return; /* Do not extend the original 30 s deadline. */
#endif
    s_browse_lobbies=false;
    s_core.state=s_core.role==PENALTY_BLE_LINK_HOST ? PENALTY_BLE_LINK_ADVERTISING : PENALTY_BLE_LINK_SCANNING;
    s_handshake_deadline_ms=s_recovery_deadline_ms;
    int rc=s_core.role==PENALTY_BLE_LINK_HOST ? start_advertising() : start_scanning();
    if (rc) fail_session(rc); else publish_state();
}

static void handle_internal(const internal_event_t *event) {
    if (event->generation && (event->generation!=atomic_load(&s_connection_generation) || event->conn_handle!=s_conn_handle)) return;
    int rc;
    switch (event->type) {
    case INTERNAL_SYNC:
        if (!penalty_ble_session_core_on_synced(&s_core)) {
            fail_session(BLE_HS_EINVAL);
            return;
        }
        rc = s_core.role == PENALTY_BLE_LINK_HOST
            ? start_advertising() : start_scanning();
        if (rc != 0) fail_session(rc);
        else publish_state();
        break;
    case INTERNAL_ADV_MATCH:
#if CONFIG_PENALTY_DEV_BLE_FAULTS
        if (s_dev_fault.active) break; /* Ignore a queued pre-disconnect report. */
#endif
        if (s_browse_lobbies && s_core.state == PENALTY_BLE_LINK_SCANNING) {
            taskENTER_CRITICAL(&s_shared_lock);
            unsigned slot = 4;
            for (unsigned i = 0; i < 4; ++i) {
                if (s_lobbies[i].seen_ms && memcmp(&s_lobbies[i].address,
                    &event->data.advertisement.address, sizeof(ble_addr_t)) == 0) {
                    slot = i; break;
                }
                if (!s_lobbies[i].seen_ms || now_ms() - s_lobbies[i].seen_ms > 10000)
                    slot = i;
            }
            if (slot < 4) {
                s_lobbies[slot].address = event->data.advertisement.address;
                s_lobbies[slot].code = event->data.advertisement.code;
                s_lobbies[slot].rssi = event->data.advertisement.rssi;
                s_lobbies[slot].seen_ms = now_ms();
            }
            taskEXIT_CRITICAL(&s_shared_lock);
            break;
        }
        if (!penalty_ble_session_core_on_connecting(&s_core)) break;
        (void)ble_gap_disc_cancel();
        publish_state();
        rc = ble_gap_connect(s_own_addr_type, &event->data.advertisement.address,
                             30000, NULL, gap_event, NULL);
        if (rc != 0) fail_session(rc);
        break;
    case INTERNAL_SELECT_LOBBY: {
        unsigned index = event->data.lobby_index;
        if (!s_browse_lobbies || index >= 4 || !s_lobbies[index].seen_ms ||
            now_ms() - s_lobbies[index].seen_ms > 10000 ||
            !penalty_ble_session_core_on_connecting(&s_core)) break;
        s_core.lobby_code = s_target_lobby_code = s_lobbies[index].code;
        (void)ble_gap_disc_cancel();
        publish_state();
        rc = ble_gap_connect(s_own_addr_type, &s_lobbies[index].address,
                             30000, NULL, gap_event, NULL);
        if (rc != 0) fail_session(rc);
        break;
    }
    case INTERNAL_CONNECTED:
        s_conn_handle = event->conn_handle;
#if CONFIG_PENALTY_DEV_BLE_FAULTS
        if (s_dev_fault.active) {
            terminate_connection(BLE_ERR_REM_USER_CONN_TERM);
            break;
        }
#endif
        if (s_m4) {
            struct ble_gap_conn_desc peer;
            if (ble_gap_conn_find(s_conn_handle,&peer) ||
                (s_peer_pinned && ble_addr_cmp(&peer.peer_id_addr,&s_peer_address))) {
                terminate_connection(BLE_ERR_REM_USER_CONN_TERM); break;
            }
            if (!s_peer_pinned) s_peer_address=peer.peer_id_addr;
            if (s_core.role==PENALTY_BLE_LINK_HOST) {
                if (++s_connection_count==0) { fail_session(BLE_HS_ETIMEOUT); break; }
                s_epoch=s_connection_count;
                s_protocol_info_bytes[20]=(uint8_t)s_epoch; s_protocol_info_bytes[21]=s_epoch>>8;
            } else s_epoch=0;
            s_tx_sequence=0; s_applied_sequence=s_ack_to_send=0;
            penalty_ble_reliable_init(&s_reliable);
            penalty_ble_rx_window_init(&s_rx_window,s_epoch);
            penalty_ble_reassembler_init(&s_reassembler);
            reset_peer_handles(); xQueueReset(s_send_queue);
        }
        atomic_fetch_add(&s_connection_generation,1);
        set_mtu(23);
        s_handshake_deadline_ms = s_recovery_deadline_ms ? s_recovery_deadline_ms : now_ms() + SESSION_HANDSHAKE_TIMEOUT_MS;
        if (!penalty_ble_session_core_on_connected(&s_core)) {
            fail_session(BLE_HS_EINVAL);
            break;
        }
        publish_state();
        if (s_core.role == PENALTY_BLE_LINK_GUEST) {
            rc = ble_gap_security_initiate(s_conn_handle);
            if (rc != 0) fail_session(rc);
        }
        break;
    case INTERNAL_CONNECT_FAILED:
        if (s_m4 && s_peer_pinned && s_recovery_deadline_ms) resume_discovery();
        else fail_session(event->status ? event->status : BLE_HS_EUNKNOWN);
        break;
    case INTERNAL_DISCOVERY_COMPLETE:
        /* Only an unfinished advertising or scanning phase can time out. */
        if (s_core.state == PENALTY_BLE_LINK_ADVERTISING ||
            s_core.state == PENALTY_BLE_LINK_SCANNING)
            fail_session(event->status ? event->status : BLE_HS_ETIMEOUT);
        break;
    case INTERNAL_DISCONNECTED: {
        bool recover=s_m4 && s_peer_pinned && !s_match_ended &&
            s_core.state!=PENALTY_BLE_LINK_ERROR && s_core.state!=PENALTY_BLE_LINK_REJECTED;
        s_conn_handle = SESSION_INVALID_HANDLE;
        s_handshake_deadline_ms = 0;
        s_secure = false;
        s_event_subscribed = false;
        s_outbound.active = false;
        penalty_ble_session_core_on_disconnected(&s_core);
        publish_state();
        const penalty_ble_session_event_t disconnected = {
            .type = PENALTY_BLE_SESSION_EVENT_DISCONNECTED,
            .state = s_core.state,
            .error = event->status,
        };
        publish_event(&disconnected);
        if (recover) {
#if CONFIG_PENALTY_DEV_BLE_FAULTS
            penalty_ble_fault_disconnected(&s_dev_fault,now_ms());
            if (s_dev_fault.active)
                ESP_LOGI(TAG,"DEV BLE TEST: real GAP disconnect; hold=%lu ms",(unsigned long)s_dev_fault.hold_ms);
#endif
            s_end_requested=false;
            if (!s_recovery_deadline_ms) s_recovery_deadline_ms=now_ms()+30000;
            penalty_ble_reliable_init(&s_reliable);
            xQueueReset(s_send_queue); resume_discovery();
        }
        break;
    }
    case INTERNAL_NUMERIC:
        if (!penalty_ble_session_core_on_numeric(&s_core,
                                                event->data.numeric.value)) {
            fail_session(BLE_HS_EINVAL);
            break;
        }
        publish_state();
        publish_event(&(penalty_ble_session_event_t){
            .type = PENALTY_BLE_SESSION_EVENT_NUMERIC_COMPARISON,
            .state = s_core.state,
            .numeric_value = event->data.numeric.value,
        });
        break;
    case INTERNAL_CONFIRM_NUMERIC: {
        if (!penalty_ble_session_core_confirm_numeric(
                &s_core, event->data.numeric.accepted)) {
            fail_session(BLE_HS_EINVAL);
            break;
        }
        publish_state();
        struct ble_sm_io response = {
            .action = BLE_SM_IOACT_NUMCMP,
            .numcmp_accept = event->data.numeric.accepted,
        };
        rc = ble_sm_inject_io(s_conn_handle, &response);
        if (rc != 0) fail_session(rc);
        else if (!event->data.numeric.accepted)
            terminate_connection(BLE_ERR_REM_USER_CONN_TERM);
        break;
    }
    case INTERNAL_ENCRYPTION:
        if (event->status != 0 || !event->data.security.encrypted ||
            !event->data.security.authenticated ||
            !penalty_ble_session_core_on_secured(&s_core)) {
            terminate_connection(BLE_ERR_AUTH_FAIL);
            fail_session(event->status ? event->status : BLE_HS_EAUTHEN);
            break;
        }
        s_secure = true;
        publish_state();
        if (s_core.role == PENALTY_BLE_LINK_GUEST) {
            rc = ble_gattc_exchange_mtu(s_conn_handle, mtu_complete, (void *)(uintptr_t)atomic_load(&s_connection_generation));
            if (rc != 0) fail_session(rc);
        }
        break;
    case INTERNAL_MTU_UPDATE:
        set_mtu(event->data.mtu.mtu);
        break;
    case INTERNAL_MTU_DONE:
        set_mtu(event->status == 0 ? event->data.mtu.mtu : 23);
        rc = ble_gattc_disc_svc_by_uuid(s_conn_handle, &SERVICE_UUID.u,
                                        service_discovered, (void *)(uintptr_t)atomic_load(&s_connection_generation));
        if (rc != 0) fail_session(rc);
        break;
    case INTERNAL_SERVICE:
        if (event->status != 0) {
            fail_session(event->status);
        } else if (!event->data.service.done) {
            s_peer_handles.service_start = event->data.service.start_handle;
            s_peer_handles.service_end = event->data.service.end_handle;
        } else if (s_peer_handles.service_start == SESSION_INVALID_HANDLE) {
            fail_session(BLE_HS_ENOENT);
        } else {
            rc = ble_gattc_disc_all_chrs(s_conn_handle,
                                         s_peer_handles.service_start,
                                         s_peer_handles.service_end,
                                         characteristic_discovered, (void *)(uintptr_t)atomic_load(&s_connection_generation));
            if (rc != 0) fail_session(rc);
        }
        break;
    case INTERNAL_CHARACTERISTIC:
        if (event->status != 0) {
            fail_session(event->status);
        } else if (!event->data.characteristic.done) {
            switch (event->data.characteristic.id) {
            case CHARACTERISTIC_PROTOCOL_INFO:
                s_peer_handles.protocol_info = event->data.characteristic.value_handle;
                break;
            case CHARACTERISTIC_CONTROL_POINT:
                s_peer_handles.control_point = event->data.characteristic.value_handle;
                break;
            case CHARACTERISTIC_EVENT_STREAM:
                s_peer_handles.event_def = event->data.characteristic.def_handle;
                s_peer_handles.event_stream = event->data.characteristic.value_handle;
                atomic_store(&s_peer_event_handle,
                             event->data.characteristic.value_handle);
                break;
            case CHARACTERISTIC_SESSION_STATUS:
                s_peer_handles.status_def = event->data.characteristic.def_handle;
                s_peer_handles.session_status = event->data.characteristic.value_handle;
                break;
            default:
                break;
            }
        } else if (s_peer_handles.protocol_info == SESSION_INVALID_HANDLE ||
                   s_peer_handles.control_point == SESSION_INVALID_HANDLE ||
                   s_peer_handles.event_stream == SESSION_INVALID_HANDLE ||
                   s_peer_handles.session_status == SESSION_INVALID_HANDLE ||
                   s_peer_handles.status_def <= s_peer_handles.event_stream) {
            fail_session(BLE_HS_ENOENT);
        } else {
            rc = ble_gattc_disc_all_dscs(s_conn_handle,
                                         s_peer_handles.event_stream,
                                         s_peer_handles.status_def - 1,
                                         descriptor_discovered, (void *)(uintptr_t)atomic_load(&s_connection_generation));
            if (rc != 0) fail_session(rc);
        }
        break;
    case INTERNAL_DESCRIPTOR:
        if (event->status != 0) {
            fail_session(event->status);
        } else if (!event->data.descriptor.done) {
            if (event->data.descriptor.is_cccd &&
                event->data.descriptor.characteristic_handle ==
                    s_peer_handles.event_stream)
                s_peer_handles.event_cccd = event->data.descriptor.descriptor_handle;
        } else if (s_peer_handles.event_cccd == SESSION_INVALID_HANDLE) {
            fail_session(BLE_HS_ENOENT);
        } else {
            rc = ble_gattc_read(s_conn_handle, s_peer_handles.protocol_info,
                protocol_info_read, (void *)(uintptr_t)atomic_load(&s_connection_generation));
            if (rc != 0) fail_session(rc);
        }
        break;
    case INTERNAL_PROTOCOL_INFO: {
        penalty_ble_protocol_info_t remote;
        uint32_t negotiated;
        if (event->status != 0 ||
            !penalty_ble_protocol_info_decode(event->data.protocol_info.bytes,
                                              PENALTY_BLE_PROTOCOL_INFO_BYTES,
                                              &remote) ||
            (s_peer_pinned && remote.boot_instance_id!=s_peer_boot) ||
            penalty_ble_protocol_negotiate(&s_core.local_info, &remote,
                                           &negotiated) !=
                PENALTY_BLE_NEGOTIATE_OK) {
            fail_session(event->status ? event->status : BLE_HS_EAUTHEN);
            break;
        }
        if (s_m4) {
            uint16_t epoch=(uint16_t)event->data.protocol_info.bytes[20] |
                (uint16_t)event->data.protocol_info.bytes[21]<<8;
            if (!epoch || epoch<=s_connection_count) { fail_session(BLE_HS_EAUTHEN); break; }
            s_epoch=s_connection_count=epoch;
            penalty_ble_rx_window_init(&s_rx_window,s_epoch);
        }
        const uint8_t enable_indication[2] = {2, 0};
        rc = ble_gattc_write_flat(s_conn_handle, s_peer_handles.event_cccd,
                                  enable_indication, sizeof(enable_indication),
                                  subscribe_complete, (void *)(uintptr_t)atomic_load(&s_connection_generation));
        if (rc != 0) fail_session(rc);
        break;
    }
    case INTERNAL_SUBSCRIBE_DONE:
        if (event->status != 0 ||
            !penalty_ble_session_core_begin_binding(&s_core)) {
            fail_session(event->status ? event->status : BLE_HS_EINVAL);
            break;
        }
        publish_state();
        rc = prepare_message(PENALTY_BLE_HELLO, PENALTY_BLE_FLAG_ACK_REQUIRED,
                             s_protocol_info_bytes, PENALTY_BLE_PROTOCOL_INFO_BYTES);
        if (rc != 0) fail_session(rc);
        break;
    case INTERNAL_WRITE_DONE:
    case INTERNAL_INDICATION_DONE:
        outbound_fragment_done(event->status);
        break;
    case INTERNAL_INBOUND_FRAGMENT:
        process_fragment(event);
        break;
    case INTERNAL_SUBSCRIPTION:
        s_event_subscribed = event->data.subscription.enabled;
        break;
    case INTERNAL_SEND_PING: {
        uint8_t token[4] = {
            (uint8_t)event->data.ping.token,
            (uint8_t)(event->data.ping.token >> 8),
            (uint8_t)(event->data.ping.token >> 16),
            (uint8_t)(event->data.ping.token >> 24),
        };
        if (s_core.role != PENALTY_BLE_LINK_GUEST ||
            s_core.state != PENALTY_BLE_LINK_READY) break;
        rc = prepare_message(PENALTY_BLE_PING, PENALTY_BLE_FLAG_ACK_REQUIRED,
                             token, sizeof(token));
        if (rc != 0) fail_session(rc);
        break;
    }
    case INTERNAL_REQUEST_SNAPSHOT:
        if (s_core.role == PENALTY_BLE_LINK_GUEST &&
            s_core.state == PENALTY_BLE_LINK_READY) {
            rc = prepare_message(PENALTY_BLE_SNAPSHOT_REQUEST,
                                 PENALTY_BLE_FLAG_ACK_REQUIRED, NULL, 0);
            if (rc != 0) fail_session(rc);
        }
        break;
    case INTERNAL_STOP:
        break;
    case INTERNAL_ACK_MESSAGE:
        if (s_m4 && event->ack_epoch==s_epoch && s_core.state==PENALTY_BLE_LINK_READY && event->data.ack_sequence>s_applied_sequence) {
            s_applied_sequence=event->data.ack_sequence; s_ack_to_send=s_applied_sequence;
            s_recovery_deadline_ms=0; /* Snapshot handshake progressed on this link. */
        }
        break;
    case INTERNAL_MATCH_END:
#if CONFIG_PENALTY_DEV_BLE_FAULTS
        penalty_ble_fault_reset(&s_dev_fault);
#endif
        s_end_requested=true;
        if (s_core.state!=PENALTY_BLE_LINK_READY) {
            s_match_ended=true; s_recovery_deadline_ms=s_handshake_deadline_ms=0;
            (void)ble_gap_disc_cancel(); (void)ble_gap_conn_cancel(); (void)ble_gap_adv_stop();
            terminate_connection(BLE_ERR_REM_USER_CONN_TERM);
        }
        break;
#if CONFIG_PENALTY_DEV_BLE_FAULTS
    case INTERNAL_DEV_DISCONNECT:
        /* These checks are performed by the owner, not from the input task. */
        if (!s_m4 || !s_peer_pinned || !s_secure || s_match_ended || s_end_requested ||
            s_core.state!=PENALTY_BLE_LINK_READY || s_conn_handle==SESSION_INVALID_HANDLE ||
            event->data.dev_fault.generation!=atomic_load(&s_connection_generation) ||
            !penalty_ble_fault_arm(&s_dev_fault,event->data.dev_fault.seconds)) break;
        rc=ble_gap_terminate(s_conn_handle,BLE_ERR_REM_USER_CONN_TERM);
        if (rc) {
            penalty_ble_fault_reset(&s_dev_fault);
            ESP_LOGW(TAG,"DEV BLE TEST: terminate failed: %d",rc);
        }
        /* Only the real GAP disconnect callback can pause the match. */
        break;
#endif
    }
}

static int retry_pending(void) {
    s_outbound.wire_length=penalty_ble_encode(&s_reliable.message,s_outbound.wire,sizeof(s_outbound.wire));
    if (!s_outbound.wire_length) return BLE_HS_EINVAL;
    s_outbound.message_id=++s_fragment_id;
    s_outbound.fragment_index=0;
    s_outbound.fragment_count=penalty_ble_fragment_count(s_outbound.wire_length,fragment_payload_size());
    s_outbound.tracks_pending=true;
    s_outbound.active=s_outbound.fragment_count!=0;
    s_outbound_deadline_ms=now_ms()+5000;
    return s_outbound.active ? send_next_fragment() : BLE_HS_EINVAL;
}

static void worker_task(void *arg) {
    (void)arg;
    internal_event_t event;
    uint64_t next_sample = 0;
    for (;;) {
        if (now_ms() >= next_sample) {
            sample_resources();
            next_sample = now_ms() + 1000;
        }
        if (xQueueReceive(s_internal_queue, &event, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (event.type == INTERNAL_STOP) break;
            handle_internal(&event);
        }
        if (atomic_exchange(&s_queue_overflow, false) ||
            atomic_exchange(&s_public_overflow, false))
            fail_session(BLE_HS_ENOMEM);
        if (s_handshake_deadline_ms && now_ms() >= s_handshake_deadline_ms)
            fail_session(BLE_HS_ETIMEOUT);
#if CONFIG_PENALTY_DEV_BLE_FAULTS
        if (penalty_ble_fault_release(&s_dev_fault,now_ms()) &&
            s_m4 && s_peer_pinned && !s_match_ended &&
            s_core.state==PENALTY_BLE_LINK_DISCONNECTED && s_recovery_deadline_ms) {
            ESP_LOGI(TAG,"DEV BLE TEST: hold complete; resume normal discovery");
            resume_discovery();
        }
#endif
        if (s_outbound.active && now_ms() >= s_outbound_deadline_ms)
            recover_link(BLE_HS_ETIMEOUT);
        if (!s_outbound.active && s_core.state == PENALTY_BLE_LINK_READY) {
            if (s_m4 && s_ack_to_send) {
                int rc=prepare_message(PENALTY_BLE_TRANSPORT_ACK,0,NULL,0);
                s_ack_to_send=0;
                if (rc) recover_link(rc);
                continue;
            }
            penalty_ble_reliable_result_t retry=penalty_ble_reliable_tick(&s_reliable,now_ms());
            if (retry==PENALTY_BLE_RELIABLE_EXPIRED) { recover_link(BLE_HS_ETIMEOUT); continue; }
            if (retry==PENALTY_BLE_RELIABLE_RETRY) {
                int rc=retry_pending(); if (rc) recover_link(rc);
                continue;
            }
            if (s_end_requested && !s_reliable.pending && !uxQueueMessagesWaiting(s_send_queue)) {
                s_match_ended=true; s_recovery_deadline_ms=0;
            }
            penalty_ble_message_t message;
            if (!s_reliable.pending && xQueueReceive(s_send_queue, &message, 0) == pdTRUE) {
                int rc = prepare_message(message.type, message.flags,
                                         message.payload, message.payload_len);
                if (rc != 0) fail_session(rc);
            }
        }
    }
    sample_resources();
    taskENTER_CRITICAL(&s_shared_lock);
    s_metrics.internal_queue_peak = (uint16_t)atomic_load(&s_queue_peak);
    s_metrics.public_queue_peak = (uint16_t)atomic_load(&s_public_queue_peak);
    s_metrics.send_queue_peak = (uint16_t)atomic_load(&s_send_queue_peak);
    taskEXIT_CRITICAL(&s_shared_lock);
    if (s_worker_stopped) xSemaphoreGive(s_worker_stopped);
    vTaskDelete(NULL);
}

static int initialize_gatt(void) {
    ble_svc_gap_init();
    ble_svc_gatt_init();
    int rc = ble_gatts_count_cfg(GATT_SERVICES);
    if (rc == 0) rc = ble_gatts_add_svcs(GATT_SERVICES);
    return rc;
}

static void release_session_resources(void) {
    if (s_internal_queue) vQueueDelete(s_internal_queue);
    if (s_public_queue) vQueueDelete(s_public_queue);
    if (s_send_queue) vQueueDelete(s_send_queue);
    if (s_host_stopped) vSemaphoreDelete(s_host_stopped);
    if (s_worker_stopped) vSemaphoreDelete(s_worker_stopped);
    s_internal_queue = NULL;
    s_public_queue = NULL;
    s_send_queue = NULL;
    s_host_stopped = NULL;
    s_worker_stopped = NULL;
    s_worker_task = NULL;
}

static esp_err_t abort_start(esp_err_t error) {
    atomic_store(&s_accept_events, false);
    if (atomic_exchange(&s_nimble_initialized, false))
        (void)nimble_port_deinit();
    release_session_resources();
    s_core.state = PENALTY_BLE_LINK_OFF;
    atomic_store(&s_published_state, s_core.state);
    atomic_store(&s_running, false);
    return error;
}

esp_err_t penalty_ble_session_start(const penalty_ble_session_config_t *config) {
    if (!config || (config->role != PENALTY_BLE_LINK_HOST &&
                    config->role != PENALTY_BLE_LINK_GUEST) ||
        config->lobby_code > 9999)
        return ESP_ERR_INVALID_ARG;
    if (atomic_exchange(&s_running, true)) return ESP_ERR_INVALID_STATE;
#if CONFIG_PENALTY_DEV_BLE_FAULTS
    penalty_ble_fault_reset(&s_dev_fault);
    ESP_LOGW(TAG,"DEV BLE TEST enabled; not a release build");
#endif

    memset(&s_metrics, 0, sizeof(s_metrics));
    s_metrics.heap_before_start = esp_get_free_heap_size();
    s_metrics.minimum_heap_sampled = s_metrics.heap_before_start;
    s_metrics.minimum_largest_block_sampled = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    s_conn_handle = SESSION_INVALID_HANDLE;
    s_event_subscribed = false;
    s_secure = false;
    s_tx_sequence = 0;
    s_m4=config->m4_match;
    s_peer_pinned=s_end_requested=false; s_match_ended=false;
    memset(&s_peer_address,0,sizeof(s_peer_address));
    s_peer_boot=s_recovery_deadline_ms=0;
    s_epoch=SESSION_EPOCH; s_connection_count=s_fragment_id=0;
    s_applied_sequence=s_ack_to_send=0;
    atomic_store(&s_connection_generation,0);
    penalty_ble_reliable_init(&s_reliable);
    s_mtu_payload = 20;
    s_role = config->role;
    s_browse_lobbies = config->browse_lobbies;
    memset(s_lobbies, 0, sizeof(s_lobbies));
    s_target_lobby_code = config->lobby_code;
    atomic_store(&s_queue_peak, 0);
    atomic_store(&s_public_queue_peak, 0);
    atomic_store(&s_send_queue_peak, 0);
    atomic_store(&s_peer_event_handle, SESSION_INVALID_HANDLE);
    atomic_store(&s_nimble_initialized, false);
    atomic_store(&s_host_started, false);
    atomic_store(&s_host_stop_requested, false);
    atomic_store(&s_worker_started, false);
    atomic_store(&s_public_overflow, false);
    s_handshake_deadline_ms = 0;
    memset(&s_outbound, 0, sizeof(s_outbound));
    reset_peer_handles();
    penalty_ble_rx_window_init(&s_rx_window, SESSION_EPOCH);
    penalty_ble_reassembler_init(&s_reassembler);
    penalty_ble_session_core_init(&s_core, config->role, config->lobby_code,
                                  config->boot_instance_id);
    if (config->m3_loop) {
        s_core.local_info.offered_capabilities |= PENALTY_BLE_CAP_M3_LOOP_V1;
        s_core.local_info.required_capabilities |= PENALTY_BLE_CAP_M3_LOOP_V1;
    }
    if (s_m4) {
        uint32_t capabilities=PENALTY_BLE_CAP_M4_MATCH_V1 | PENALTY_BLE_CAP_DUEL_V1 |
            PENALTY_BLE_CAP_COMMIT_REVEAL_V1 | PENALTY_BLE_CAP_RECONNECT_V1;
        s_core.local_info.offered_capabilities |= capabilities;
        s_core.local_info.required_capabilities |= capabilities;
    }
    s_offered_capabilities = s_core.local_info.offered_capabilities;
    s_required_capabilities = s_core.local_info.required_capabilities;
    s_core.state = PENALTY_BLE_LINK_STARTING;
    atomic_store(&s_published_state, s_core.state);
    atomic_store(&s_queue_overflow, false);
    refresh_status_snapshot();

    s_internal_queue = xQueueCreate(SESSION_INTERNAL_QUEUE_LENGTH,
                                    sizeof(internal_event_t));
    s_public_queue = xQueueCreate(SESSION_PUBLIC_QUEUE_LENGTH,
                                  sizeof(penalty_ble_session_event_t));
    s_send_queue = xQueueCreate(4, sizeof(penalty_ble_message_t));
    s_host_stopped = xSemaphoreCreateBinary();
    s_worker_stopped = xSemaphoreCreateBinary();
    if (!s_internal_queue || !s_public_queue || !s_send_queue || !s_host_stopped ||
        !s_worker_stopped) {
        return abort_start(ESP_ERR_NO_MEM);
    }
    atomic_store(&s_accept_events, true);

    esp_err_t error = nvs_flash_init();
    if (error != ESP_OK) {
        return abort_start(error);
    }
    error = nimble_port_init();
    if (error != ESP_OK) {
        return abort_start(error);
    }
    atomic_store(&s_nimble_initialized, true);

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_YES_NO;
    /* One session-only bond permits authenticated reconnection without NVS. */
#if CONFIG_BT_NIMBLE_NVS_PERSIST
#error Penalty match keys must not persist across boots
#endif
    ble_store_config_init();
    ble_hs_cfg.sm_bonding = s_m4 ? 1 : 0;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = s_m4 ? BLE_SM_PAIR_KEY_DIST_ENC : 0;
    ble_hs_cfg.sm_their_key_dist = s_m4 ? BLE_SM_PAIR_KEY_DIST_ENC : 0;
    int rc = initialize_gatt();
    if (rc == 0) rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc == 0) rc = ble_att_set_preferred_mtu(SESSION_PREFERRED_MTU);
    if (rc != 0 || penalty_ble_protocol_info_encode(
            &s_core.local_info, s_protocol_info_bytes,
            sizeof(s_protocol_info_bytes)) == 0) {
        return abort_start(ESP_FAIL);
    }

    if (xTaskCreate(worker_task, "penalty_ble", SESSION_TASK_STACK_BYTES,
                    NULL, 5, &s_worker_task) != pdPASS) {
        s_worker_task = NULL;
        return abort_start(ESP_ERR_NO_MEM);
    }
    atomic_store(&s_worker_started, true);
    atomic_store(&s_host_started, true);
    nimble_port_freertos_init(host_task);
    publish_state();
    return ESP_OK;
}

esp_err_t penalty_ble_session_stop(void) {
    if (!atomic_load(&s_running)) return ESP_OK;
    atomic_store(&s_accept_events, false);

    /* Stop the worker first so no task issues NimBLE calls during shutdown. */
    if (s_internal_queue && atomic_load(&s_worker_started)) {
        const internal_event_t stop = {.type = INTERNAL_STOP};
        if (xQueueSendToFront(s_internal_queue, &stop,
                              pdMS_TO_TICKS(SESSION_STOP_SEND_TIMEOUT_MS)) != pdTRUE ||
            !s_worker_stopped || xSemaphoreTake(
                s_worker_stopped, pdMS_TO_TICKS(SESSION_STOP_TIMEOUT_MS)) != pdTRUE)
            return ESP_ERR_TIMEOUT;
        atomic_store(&s_worker_started, false);
    }
    s_core.state = PENALTY_BLE_LINK_STOPPING;
#if CONFIG_PENALTY_DEV_BLE_FAULTS
    penalty_ble_fault_reset(&s_dev_fault); /* Worker has stopped; safe to clear. */
#endif
    atomic_store(&s_published_state, s_core.state);

    if (atomic_load(&s_nimble_initialized) && atomic_load(&s_host_started)) {
        /* A retry waits for the stop already requested instead of repeating it. */
        if (!atomic_load(&s_host_stop_requested)) {
            /* Delete only this match's peer, from the nonpersistent store. */
            if (s_m4 && s_peer_pinned && ble_store_util_delete_peer(&s_peer_address)!=0)
                return ESP_FAIL;
            (void)ble_gap_disc_cancel();
            (void)ble_gap_conn_cancel();
            (void)ble_gap_adv_stop();
            if (s_conn_handle != SESSION_INVALID_HANDLE)
                (void)ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            int rc = nimble_port_stop();
            if (rc != 0) {
                ESP_LOGE(TAG, "host stop failed: %d", rc);
                return ESP_FAIL;
            }
            atomic_store(&s_host_stop_requested, true);
        }
        if (!s_host_stopped || xSemaphoreTake(
                s_host_stopped, pdMS_TO_TICKS(SESSION_STOP_TIMEOUT_MS)) != pdTRUE)
            return ESP_ERR_TIMEOUT;
        /* Deletes the host task parked in host_task(). */
        nimble_port_freertos_deinit();
        atomic_store(&s_host_started, false);
        atomic_store(&s_host_stop_requested, false);
    }

    if (atomic_exchange(&s_nimble_initialized, false))
        (void)nimble_port_deinit();
    release_session_resources();
    s_conn_handle = SESSION_INVALID_HANDLE;
    s_core.state = PENALTY_BLE_LINK_OFF;
    atomic_store(&s_published_state, s_core.state);
    atomic_store(&s_running, false);
    uint32_t heap_after_stop = esp_get_free_heap_size();
    taskENTER_CRITICAL(&s_shared_lock);
    s_metrics.heap_after_stop = heap_after_stop;
    taskEXIT_CRITICAL(&s_shared_lock);
    ESP_LOGI(TAG, "stopped; worker stack remaining=%u bytes, queue peak=%u",
             (unsigned)s_metrics.worker_stack_high_water,
             (unsigned)s_metrics.internal_queue_peak);
    ESP_LOGI(TAG, "resources heap before/ready/after=%lu/%lu/%lu sampled min=%lu largest=%lu host stack=%lu bytes queues public/send=%u/%u",
             (unsigned long)s_metrics.heap_before_start, (unsigned long)s_metrics.heap_at_ready,
             (unsigned long)s_metrics.heap_after_stop, (unsigned long)s_metrics.minimum_heap_sampled,
             (unsigned long)s_metrics.minimum_largest_block_sampled,
             (unsigned long)s_metrics.host_stack_high_water,
             (unsigned)s_metrics.public_queue_peak, (unsigned)s_metrics.send_queue_peak);
    return ESP_OK;
}

static esp_err_t send_command(const internal_event_t *event) {
    if (!atomic_load(&s_running) || !atomic_load(&s_accept_events) ||
        !s_internal_queue)
        return ESP_ERR_INVALID_STATE;
    return xQueueSend(s_internal_queue, event, 0) == pdTRUE
        ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t penalty_ble_session_confirm_numeric(bool accepted) {
    internal_event_t event = {.type = INTERNAL_CONFIRM_NUMERIC};
    event.data.numeric.accepted = accepted;
    return send_command(&event);
}

esp_err_t penalty_ble_session_send_ping(uint32_t token) {
    internal_event_t event = {.type = INTERNAL_SEND_PING};
    event.data.ping.token = token;
    return send_command(&event);
}

esp_err_t penalty_ble_session_request_snapshot(void) {
    const internal_event_t event = {.type = INTERNAL_REQUEST_SNAPSHOT};
    return send_command(&event);
}

esp_err_t penalty_ble_session_ack_message(uint16_t epoch, uint32_t sequence) {
    internal_event_t event={.type=INTERNAL_ACK_MESSAGE,.ack_epoch=epoch};
    event.data.ack_sequence=sequence;
    return send_command(&event);
}

esp_err_t penalty_ble_session_end_match(void) {
    const internal_event_t event={.type=INTERNAL_MATCH_END};
    return send_command(&event);
}
bool penalty_ble_session_match_ended(void) { return atomic_load(&s_match_ended); }

#if CONFIG_PENALTY_DEV_BLE_FAULTS
esp_err_t penalty_ble_session_dev_disconnect(unsigned seconds) {
    if (seconds!=10 && seconds!=35) return ESP_ERR_INVALID_ARG;
    if (penalty_ble_session_state()!=PENALTY_BLE_LINK_READY)
        return ESP_ERR_INVALID_STATE;
    internal_event_t event={.type=INTERNAL_DEV_DISCONNECT};
    event.data.dev_fault.seconds=seconds;
    event.data.dev_fault.generation=atomic_load(&s_connection_generation);
    return send_command(&event);
}
#endif

esp_err_t penalty_ble_session_send_message(const penalty_ble_message_t *message) {
    if (!message || message->payload_len > PENALTY_BLE_MAX_PAYLOAD_BYTES ||
        !atomic_load(&s_running) || !atomic_load(&s_accept_events) || !s_send_queue ||
        penalty_ble_session_state() != PENALTY_BLE_LINK_READY)
        return ESP_ERR_INVALID_STATE;
    if (xQueueSend(s_send_queue, message, 0) != pdTRUE) return ESP_ERR_NO_MEM;
    record_queue_peak(s_send_queue, &s_send_queue_peak);
    return ESP_OK;
}

size_t penalty_ble_session_get_lobbies(penalty_ble_lobby_t *lobbies, size_t capacity) {
    size_t count = 0;
    uint64_t now = now_ms();
    taskENTER_CRITICAL(&s_shared_lock);
    for (unsigned i = 0; i < 4 && count < capacity; ++i) {
        if (s_lobbies[i].seen_ms && now - s_lobbies[i].seen_ms <= 10000)
            lobbies[count++] = (penalty_ble_lobby_t){s_lobbies[i].code, s_lobbies[i].rssi, i};
    }
    taskEXIT_CRITICAL(&s_shared_lock);
    return count;
}

esp_err_t penalty_ble_session_select_lobby(uint8_t index) {
    internal_event_t event = {.type = INTERNAL_SELECT_LOBBY};
    event.data.lobby_index = index;
    return send_command(&event);
}

bool penalty_ble_session_poll_event(penalty_ble_session_event_t *event,
                                    uint32_t timeout_ms) {
    if (!event || !s_public_queue) return false;
    return xQueueReceive(s_public_queue, event, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

penalty_ble_link_state_t penalty_ble_session_state(void) {
    return (penalty_ble_link_state_t)atomic_load(&s_published_state);
}

bool penalty_ble_session_running(void) {
    return atomic_load(&s_running);
}

void penalty_ble_session_get_metrics(penalty_ble_session_metrics_t *metrics) {
    if (!metrics) return;
    taskENTER_CRITICAL(&s_shared_lock);
    *metrics = s_metrics;
    taskEXIT_CRITICAL(&s_shared_lock);
}
