#include "penalty_ble_diag.h"

#include "penalty_ble_session.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#define DIAG_INK 0x0B211F
#define DIAG_WHITE 0xF2F0D9
#define DIAG_DIM 0xA2BC9E
#define DIAG_LIME 0xD4F45B
#define DIAG_WORKER_STACK 4096u
#define DIAG_STOP_TIMEOUT_MS 6500u

typedef enum {
    DIAG_CMD_START_HOST,
    DIAG_CMD_START_GUEST,
    DIAG_CMD_ACCEPT_NUMERIC,
    DIAG_CMD_REJECT_NUMERIC,
    DIAG_CMD_STOP,
} diag_command_t;

static lv_obj_t *s_screen;
static lv_obj_t *s_header;
static lv_obj_t *s_battery;
static lv_obj_t *s_heading;
static lv_obj_t *s_status;
static lv_obj_t *s_detail;
static lv_obj_t *s_footer;
static lv_timer_t *s_timer;
static QueueHandle_t s_commands;
static SemaphoreHandle_t s_worker_stopped;
static TaskHandle_t s_worker;
static atomic_bool s_started;
static atomic_bool s_dirty;
static atomic_bool s_have_numeric;
static atomic_bool s_ping_ok;
static atomic_bool s_snapshot_ok;
static atomic_bool s_health_started;
/* A local health-check failure stays visible over the polled link state. */
static atomic_bool s_local_failed;
static atomic_int s_role;
static atomic_int s_link_state;
static atomic_int s_error;
static atomic_int s_battery_percent;
static atomic_uint s_numeric;
static atomic_uint s_ping_token;
static atomic_uint s_mtu;
static atomic_uint s_heap_at_ready;

static uint64_t now_ms(void) {
    return (uint64_t)esp_timer_get_time() / 1000;
}

static void mark_dirty(void) {
    atomic_store(&s_dirty, true);
}

static void reset_view(penalty_ble_link_role_t role) {
    atomic_store(&s_role, role);
    atomic_store(&s_link_state, PENALTY_BLE_LINK_STARTING);
    atomic_store(&s_error, 0);
    atomic_store(&s_numeric, 0);
    atomic_store(&s_ping_token, 0);
    atomic_store(&s_mtu, 0);
    atomic_store(&s_heap_at_ready, 0);
    atomic_store(&s_have_numeric, false);
    atomic_store(&s_ping_ok, false);
    atomic_store(&s_snapshot_ok, false);
    atomic_store(&s_health_started, false);
    atomic_store(&s_local_failed, false);
    mark_dirty();
}

static void set_error(esp_err_t error) {
    atomic_store(&s_local_failed, true);
    atomic_store(&s_error, error);
    atomic_store(&s_link_state, PENALTY_BLE_LINK_ERROR);
    mark_dirty();
}

static void run_guest_health_check(void) {
    uint32_t token = esp_random();
    atomic_store(&s_ping_token, token);
    esp_err_t error = penalty_ble_session_send_ping(token);
    if (error != ESP_OK) set_error(error);
}

static void refresh_link_state(void) {
    if (atomic_load(&s_local_failed)) return;
    int state = penalty_ble_session_running()
        ? (int)penalty_ble_session_state() : (int)PENALTY_BLE_LINK_OFF;
    if (atomic_exchange(&s_link_state, state) != state) mark_dirty();
}

static void process_event(const penalty_ble_session_event_t *event) {
    switch (event->type) {
    case PENALTY_BLE_SESSION_EVENT_NUMERIC_COMPARISON:
        atomic_store(&s_numeric, event->numeric_value);
        atomic_store(&s_have_numeric, true);
        break;
    case PENALTY_BLE_SESSION_EVENT_READY: {
        penalty_ble_session_metrics_t metrics;
        penalty_ble_session_get_metrics(&metrics);
        atomic_store(&s_mtu, metrics.negotiated_mtu);
        atomic_store(&s_heap_at_ready, metrics.heap_at_ready);
        if (atomic_load(&s_role) == PENALTY_BLE_LINK_GUEST &&
            !atomic_exchange(&s_health_started, true))
            run_guest_health_check();
        break;
    }
    case PENALTY_BLE_SESSION_EVENT_PING_REPLY:
        if (event->ping_token == atomic_load(&s_ping_token)) {
            atomic_store(&s_ping_ok, true);
            esp_err_t error = penalty_ble_session_request_snapshot();
            if (error != ESP_OK) set_error(error);
        } else {
            set_error(ESP_ERR_INVALID_RESPONSE);
        }
        break;
    case PENALTY_BLE_SESSION_EVENT_PING_RECEIVED:
        atomic_store(&s_ping_ok, true);
        break;
    case PENALTY_BLE_SESSION_EVENT_SNAPSHOT_SENT:
        atomic_store(&s_snapshot_ok, true);
        break;
    case PENALTY_BLE_SESSION_EVENT_SNAPSHOT:
        if (event->snapshot.schema == PENALTY_BLE_SNAPSHOT_SCHEMA &&
            event->snapshot.role == PENALTY_BLE_LINK_HOST &&
            event->snapshot.lobby_code == PENALTY_BLE_DIAG_LOBBY_CODE &&
            (event->snapshot.security_flags &
             (PENALTY_BLE_SECURITY_ENCRYPTED |
              PENALTY_BLE_SECURITY_AUTHENTICATED)) ==
             (PENALTY_BLE_SECURITY_ENCRYPTED |
              PENALTY_BLE_SECURITY_AUTHENTICATED)) {
            atomic_store(&s_snapshot_ok, true);
        } else {
            set_error(ESP_ERR_INVALID_RESPONSE);
        }
        break;
    case PENALTY_BLE_SESSION_EVENT_ERROR:
        atomic_store(&s_error, event->error ? event->error : ESP_FAIL);
        break;
    default:
        break;
    }
    mark_dirty();
}

static void start_session(penalty_ble_link_role_t role) {
    if (penalty_ble_session_running()) {
        esp_err_t stop_error = penalty_ble_session_stop();
        if (stop_error != ESP_OK) {
            set_error(stop_error);
            return;
        }
    }
    reset_view(role);
    const penalty_ble_session_config_t config = {
        .role = role,
        .lobby_code = PENALTY_BLE_DIAG_LOBBY_CODE,
        .boot_instance_id = ((uint64_t)esp_random() << 32) | esp_random(),
    };
    esp_err_t error = penalty_ble_session_start(&config);
    if (error != ESP_OK) set_error(error);
}

static void worker_task(void *argument) {
    (void)argument;
    uint64_t next_battery = 0;
    bool stop = false;
    while (!stop) {
        diag_command_t command;
        if (xQueueReceive(s_commands, &command, pdMS_TO_TICKS(25)) == pdTRUE) {
            switch (command) {
            case DIAG_CMD_START_HOST:
                start_session(PENALTY_BLE_LINK_HOST);
                break;
            case DIAG_CMD_START_GUEST:
                start_session(PENALTY_BLE_LINK_GUEST);
                break;
            case DIAG_CMD_ACCEPT_NUMERIC:
                if (penalty_ble_session_confirm_numeric(true) != ESP_OK)
                    set_error(ESP_ERR_INVALID_STATE);
                break;
            case DIAG_CMD_REJECT_NUMERIC:
                if (penalty_ble_session_confirm_numeric(false) != ESP_OK)
                    set_error(ESP_ERR_INVALID_STATE);
                break;
            case DIAG_CMD_STOP:
                stop = true;
                break;
            }
        }
        penalty_ble_session_event_t event;
        while (!stop && penalty_ble_session_poll_event(&event, 0))
            process_event(&event);
        if (!stop) refresh_link_state();
        if (now_ms() >= next_battery) {
            atomic_store(&s_battery_percent, bsp_battery_soc());
            next_battery = now_ms() + 10000;
            mark_dirty();
        }
    }
    /* penalty_ble_diag_stop() owns the BLE stop so its failure stays retryable. */
    xSemaphoreGive(s_worker_stopped);
    vTaskDelete(NULL);
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font,
                            uint32_t color) {
    lv_obj_t *label = lv_label_create(parent);
    if (!label) return NULL;
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    return label;
}

static void place(lv_obj_t *object, int x, int y, int width) {
    lv_obj_set_pos(object, x, y);
    lv_obj_set_width(object, width);
}

static void set_text(lv_obj_t *object, const char *text) {
    if (strcmp(lv_label_get_text(object), text) != 0)
        lv_label_set_text(object, text);
}

static const char *state_text(penalty_ble_link_state_t state) {
    static const char *const names[] = {
        "SELECT ROLE", "STARTING", "ADVERTISING", "SCANNING", "CONNECTING",
        "SECURING", "COMPARE CODE", "DISCOVERING", "BINDING", "READY",
        "DISCONNECTED", "REJECTED", "STOPPING", "ERROR",
    };
    return (unsigned)state < sizeof(names) / sizeof(names[0]) ? names[state] : "UNKNOWN";
}

static void render(lv_timer_t *timer) {
    (void)timer;
    if (!s_screen || !atomic_exchange(&s_dirty, false)) return;
    penalty_ble_link_state_t state =
        (penalty_ble_link_state_t)atomic_load(&s_link_state);
    penalty_ble_link_role_t role =
        (penalty_ble_link_role_t)atomic_load(&s_role);
    char text[160];
    int battery = atomic_load(&s_battery_percent);
    snprintf(text, sizeof(text), battery >= 0 ? "%d%%" : "--%%", battery);
    set_text(s_battery, text);
    snprintf(text, sizeof(text), "LOBBY %04u", PENALTY_BLE_DIAG_LOBBY_CODE);
    set_text(s_heading, text);
    if (state == PENALTY_BLE_LINK_OFF) {
        set_text(s_status, "UP: CREATE\nDOWN: JOIN");
        set_text(s_detail, "BLE starts only after role selection");
        set_text(s_footer, "HOLD OK: COVER");
        return;
    }
    if (state == PENALTY_BLE_LINK_NUMERIC_CONFIRM &&
        atomic_load(&s_have_numeric)) {
        snprintf(text, sizeof(text), "COMPARE\n%06lu",
                 (unsigned long)atomic_load(&s_numeric));
        set_text(s_status, text);
        set_text(s_detail, "Confirm the same number on both devices");
        set_text(s_footer, "OK: MATCH   DOWN: REJECT");
        return;
    }
    if (state == PENALTY_BLE_LINK_READY) {
        bool ping = atomic_load(&s_ping_ok);
        bool snapshot = atomic_load(&s_snapshot_ok);
        if (role == PENALTY_BLE_LINK_HOST) {
            set_text(s_status, "CREATE / SECURE LINK READY");
            set_text(s_detail, ping && snapshot ? "JOIN health check PASS"
                : "Waiting for JOIN health check");
        } else {
            snprintf(text, sizeof(text), "JOIN / PING %s\nSNAPSHOT %s",
                     ping ? "PASS" : "WAIT", snapshot ? "PASS" : "WAIT");
            set_text(s_status, text);
            snprintf(text, sizeof(text), "MTU %u   HEAP %luK",
                     atomic_load(&s_mtu),
                     (unsigned long)(atomic_load(&s_heap_at_ready) / 1024));
            set_text(s_detail, text);
        }
        set_text(s_footer, "HOLD OK: COVER");
        return;
    }
    if (state == PENALTY_BLE_LINK_ERROR) {
        snprintf(text, sizeof(text), "ERROR %d", atomic_load(&s_error));
        set_text(s_status, text);
        set_text(s_detail, "UP: RETRY CREATE   DOWN: RETRY JOIN");
        set_text(s_footer, "HOLD OK: COVER");
        return;
    }
    snprintf(text, sizeof(text), "%s / %s",
             role == PENALTY_BLE_LINK_HOST ? "CREATE" : "JOIN",
             state_text(state));
    set_text(s_status, text);
    set_text(s_detail, state == PENALTY_BLE_LINK_DISCONNECTED
        ? "UP: RETRY CREATE   DOWN: RETRY JOIN"
        : "Keep both devices nearby");
    set_text(s_footer, "HOLD OK: COVER");
}

bool penalty_ble_diag_enter(void) {
    if (s_screen) return true;
    s_screen = lv_obj_create(NULL);
    if (!s_screen) return false;
    lv_obj_remove_style_all(s_screen);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(DIAG_INK), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    s_header = make_label(s_screen, &lv_font_montserrat_14, DIAG_DIM);
    s_battery = make_label(s_screen, &lv_font_montserrat_14, DIAG_WHITE);
    s_heading = make_label(s_screen, &lv_font_montserrat_20, DIAG_LIME);
    s_status = make_label(s_screen, &lv_font_montserrat_20, DIAG_WHITE);
    s_detail = make_label(s_screen, &lv_font_montserrat_14, DIAG_DIM);
    s_footer = make_label(s_screen, &lv_font_montserrat_14, DIAG_WHITE);
    if (!s_header || !s_battery || !s_heading || !s_status || !s_detail || !s_footer) {
        penalty_ble_diag_exit();
        return false;
    }
    lv_obj_set_style_text_align(s_header, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    place(s_header, 16, 12, 160);
    place(s_battery, 177, 12, 47);
    place(s_heading, 16, 58, 208);
    place(s_status, 16, 118, 208);
    place(s_detail, 16, 204, 208);
    place(s_footer, 16, 286, 208);
    set_text(s_header, "PENALTY / M2 LINK TEST");
    atomic_store(&s_role, PENALTY_BLE_LINK_HOST);
    atomic_store(&s_link_state, PENALTY_BLE_LINK_OFF);
    atomic_store(&s_error, 0);
    atomic_store(&s_battery_percent, -1);
    atomic_store(&s_dirty, true);
    s_timer = lv_timer_create(render, 100, NULL);
    if (!s_timer) {
        penalty_ble_diag_exit();
        return false;
    }
    lv_timer_pause(s_timer);
    lv_screen_load(s_screen);
    render(NULL);
    return true;
}

esp_err_t penalty_ble_diag_start(void) {
    if (!s_screen || !s_timer) return ESP_ERR_INVALID_STATE;
    if (atomic_exchange(&s_started, true)) return ESP_ERR_INVALID_STATE;
    if (!bsp_lvgl_lock(500)) {
        atomic_store(&s_started, false);
        return ESP_ERR_TIMEOUT;
    }
    lv_timer_resume(s_timer);
    bsp_lvgl_unlock();
    s_commands = xQueueCreate(6, sizeof(diag_command_t));
    s_worker_stopped = xSemaphoreCreateBinary();
    if (!s_commands || !s_worker_stopped ||
        xTaskCreate(worker_task, "penalty_ble_diag", DIAG_WORKER_STACK,
                    NULL, 4, &s_worker) != pdPASS) {
        if (s_commands) vQueueDelete(s_commands);
        if (s_worker_stopped) vSemaphoreDelete(s_worker_stopped);
        s_commands = NULL;
        s_worker_stopped = NULL;
        s_worker = NULL;
        atomic_store(&s_started, false);
        if (bsp_lvgl_lock(500)) {
            lv_timer_pause(s_timer);
            bsp_lvgl_unlock();
        }
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t penalty_ble_diag_stop(void) {
    if (!atomic_load(&s_started)) return penalty_ble_session_stop();
    if (!bsp_lvgl_lock(500)) return ESP_ERR_TIMEOUT;
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    bsp_lvgl_unlock();
    const diag_command_t stop = DIAG_CMD_STOP;
    if (xQueueSendToFront(s_commands, &stop, pdMS_TO_TICKS(100)) != pdTRUE)
        return ESP_ERR_NO_MEM;
    if (xSemaphoreTake(s_worker_stopped,
                       pdMS_TO_TICKS(DIAG_STOP_TIMEOUT_MS)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    s_worker = NULL;
    vQueueDelete(s_commands);
    vSemaphoreDelete(s_worker_stopped);
    s_commands = NULL;
    s_worker_stopped = NULL;
    atomic_store(&s_started, false);
    return penalty_ble_session_stop();
}

void penalty_ble_diag_exit(void) {
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    if (s_screen) {
        lv_obj_delete(s_screen);
        s_screen = NULL;
    }
    s_header = s_battery = s_heading = s_status = s_detail = s_footer = NULL;
}

void penalty_ble_diag_key(bsp_btn_t button, bsp_btn_ev_t event) {
    if (!s_commands) return;
    penalty_ble_link_state_t state =
        (penalty_ble_link_state_t)atomic_load(&s_link_state);
    diag_command_t command;
    if (state == PENALTY_BLE_LINK_NUMERIC_CONFIRM) {
        /* Confirm only after a short click so HOLD OK can leave without
         * accepting a pairing value first. */
        /* Never accept a value that was not shown on screen. */
        if (button == BSP_BTN_OK && event == BSP_BTN_CLICK &&
            atomic_load(&s_have_numeric))
            command = DIAG_CMD_ACCEPT_NUMERIC;
        else if (button == BSP_BTN_DOWN && event == BSP_BTN_PRESS)
            command = DIAG_CMD_REJECT_NUMERIC;
        else return;
    } else if (state == PENALTY_BLE_LINK_OFF ||
               state == PENALTY_BLE_LINK_ERROR ||
               state == PENALTY_BLE_LINK_DISCONNECTED ||
               state == PENALTY_BLE_LINK_REJECTED) {
        if (event != BSP_BTN_PRESS) return;
        if (button == BSP_BTN_UP) command = DIAG_CMD_START_HOST;
        else if (button == BSP_BTN_DOWN) command = DIAG_CMD_START_GUEST;
        else return;
    } else {
        return;
    }
    (void)xQueueSend(s_commands, &command, 0);
}
