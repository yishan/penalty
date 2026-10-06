/* Standalone Penalty shell: portrait cover -> portrait game -> cover. */
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_pins.h"
#include "demo_navigation.h"
#include "penalty_app.h"
#include "penalty_ble_diag.h"
#include "penalty_multiplayer.h"
#include "penalty_preferences.h"
#include "launcher_contract.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>

static const char *TAG = "penalty_shell";
typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t event;
    uint64_t at_ms;
    unsigned epoch;
} input_event_t;

static QueueHandle_t s_input_queue;
static atomic_bool s_input_ready, s_input_lost;
static atomic_uint s_input_epoch;
/* Only the serialized button callback accesses these originating epochs. */
static unsigned s_press_epoch[BSP_BTN_COUNT];
static bool s_on_cover = true;
static bool s_ble_diag;
static bool s_multiplayer;

static void return_to_cover(void) {
    esp_err_t err = s_ble_diag ? penalty_ble_diag_stop() :
        s_multiplayer ? penalty_multiplayer_stop() : penalty_app_stop();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Stop failed: %s; hold OK to retry", esp_err_to_name(err));
        return;
    }
    if (!bsp_lvgl_lock(500)) return;
    if (s_ble_diag) penalty_ble_diag_exit();
    else if (s_multiplayer) penalty_multiplayer_exit();
    else penalty_app_exit();
    penalty_app_enter();
    atomic_fetch_add(&s_input_epoch, 1);
    atomic_store(&s_input_lost, false);
    bsp_lvgl_unlock();
    err = penalty_app_start();
    if (err != ESP_OK) ESP_LOGE(TAG, "Cover restart: %s", esp_err_to_name(err));
    else {
        s_ble_diag = false;
        s_multiplayer = false;
        s_on_cover = true;
    }
}

static void enter_multiplayer(bool host, const penalty_model_t *context) {
    esp_err_t error = penalty_app_stop();
    if (error != ESP_OK) { ESP_LOGE(TAG, "Multiplayer entry stop: %s", esp_err_to_name(error)); return; }
    if (!bsp_lvgl_lock(500)) return;
    penalty_app_exit();
    bool entered = penalty_multiplayer_enter(host, context);
    bsp_lvgl_unlock();
    s_multiplayer = true;
    s_on_cover = false;
    atomic_fetch_add(&s_input_epoch, 1);
    atomic_store(&s_input_lost, false);
    error = entered ? penalty_multiplayer_start() : ESP_ERR_NO_MEM;
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Multiplayer entry: %s", esp_err_to_name(error));
        return_to_cover();
    }
}

static void enter_ble_diag(void) {
    esp_err_t err = penalty_app_stop();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Stop before BLE diagnostics failed: %s", esp_err_to_name(err));
        return;
    }
    bool entered = false;
    if (bsp_lvgl_lock(500)) {
        penalty_app_exit();
        entered = penalty_ble_diag_enter();
        bsp_lvgl_unlock();
    }
    if (entered) err = penalty_ble_diag_start();
    else err = ESP_ERR_NO_MEM;
    if (err == ESP_OK) {
        s_ble_diag = true;
        s_on_cover = false;
        atomic_fetch_add(&s_input_epoch, 1);
        atomic_store(&s_input_lost, false);
        return;
    }
    ESP_LOGE(TAG, "BLE diagnostics unavailable: %s", esp_err_to_name(err));
    if (bsp_lvgl_lock(500)) {
        penalty_ble_diag_exit();
        penalty_app_enter();
        bsp_lvgl_unlock();
    }
    err = penalty_app_start();
    if (err != ESP_OK) ESP_LOGE(TAG, "Cover recovery failed: %s", esp_err_to_name(err));
    s_ble_diag = false;
    s_on_cover = true;
}

static void input_task(void *arg) {
    (void)arg;
    input_event_t input;
    for (;;) {
        if (xQueueReceive(s_input_queue, &input, portMAX_DELAY) != pdTRUE) continue;
        if (input.epoch != atomic_load(&s_input_epoch)) continue;
        if (input.btn == BSP_BTN_UP && input.event == BSP_BTN_LONG) {
            esp_err_t err = launcher_contract_return_to_factory();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Return to Play Library failed: %s",
                         esp_err_to_name(err));
            }
            continue;
        }
        if (s_on_cover && input.btn == BSP_BTN_DOWN &&
            input.event == BSP_BTN_LONG) {
            enter_ble_diag();
            continue;
        }
        if (s_ble_diag) {
            if (input.btn == BSP_BTN_OK && input.event == BSP_BTN_LONG)
                return_to_cover();
            else
                penalty_ble_diag_key(input.btn, input.event);
            continue;
        }
        if (s_multiplayer) {
            if (input.btn == BSP_BTN_OK && input.event == BSP_BTN_LONG)
                return_to_cover();
            else penalty_multiplayer_key(input.btn, input.event, input.at_ms,
                                          atomic_exchange(&s_input_lost, false));
            continue;
        }
        if (s_on_cover && input.btn == BSP_BTN_OK) {
            if (input.event == BSP_BTN_PRESS) {
                continue;
            }
            if (input.event == BSP_BTN_LONG) {
                continue;
            }
            if (input.event == BSP_BTN_CLICK) {
                input.event = BSP_BTN_PRESS;
                /* CLICK includes short/double-click classification delay.
                 * Keep its timestamp: the physical PRESS is deliberately
                 * deferred here, not stalled in the game's input queue. */
                s_on_cover = false;
            }
        } else if (input.btn == BSP_BTN_OK && input.event == BSP_BTN_LONG) {
            return_to_cover();
            continue;
        }
        if (penalty_app_key(input.btn, input.event, input.at_ms,
                            atomic_exchange(&s_input_lost, false))) {
            return_to_cover();
        } else {
            penalty_model_t context;
            int request = penalty_app_take_multiplayer_request(&context);
            if (request) enter_multiplayer(request == 1, &context);
        }
    }
}

static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    if (!atomic_load(&s_input_ready) || (unsigned)btn >= BSP_BTN_COUNT) return;
    unsigned epoch = demo_navigation_event_epoch(atomic_load(&s_input_epoch),
                                                 &s_press_epoch[btn], ev == BSP_BTN_PRESS);
    const input_event_t input = {btn, ev, (uint64_t)esp_timer_get_time() / 1000, epoch};
    if (xQueueSend(s_input_queue, &input, 0) != pdTRUE) atomic_store(&s_input_lost, true);
}

void app_main(void) {
    ESP_LOGI(TAG, "Penalty portrait-cover firmware");
    esp_err_t preferences_err = penalty_preferences_init();
    if (preferences_err != ESP_OK)
        ESP_LOGW(TAG, "Language preferences unavailable: %s", esp_err_to_name(preferences_err));
    penalty_app_set_initial_language(penalty_preferences_load_language());
    bsp_i2c_init();
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "Display unavailable");
        return;
    }
    if (!bsp_lvgl_lock(1000)) return;
    penalty_app_enter();
    bsp_display_backlight(100);
    bsp_lvgl_unlock();

    /* Show the cover before optional peripheral setup, never a demo launcher. */
    if (bsp_audio_init() != ESP_OK) ESP_LOGW(TAG, "Audio unavailable; silent play");
    if (bsp_battery_init() != ESP_OK) ESP_LOGW(TAG, "Battery reading unavailable");
    esp_err_t err = penalty_app_start();
    if (err != ESP_OK) ESP_LOGW(TAG, "Game start: %s", esp_err_to_name(err));

    s_input_queue = xQueueCreate(8, sizeof(input_event_t));
    if (!s_input_queue) {
        ESP_LOGE(TAG, "No input queue; restart the device");
        return;
    }
    TaskHandle_t task = NULL;
    if (xTaskCreate(input_task, "penalty_input", 4096, NULL, 5, &task) != pdPASS) {
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
        ESP_LOGE(TAG, "No input task; restart the device");
        return;
    }
    err = bsp_button_init(on_key, NULL);
    if (err != ESP_OK) {
        vTaskDelete(task);
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
        ESP_LOGE(TAG, "Buttons unavailable: %s", esp_err_to_name(err));
        return;
    }
    atomic_store(&s_input_ready, true);
}
