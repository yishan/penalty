#include "penalty_app.h"
#include "penalty_audio.h"
#include "penalty_i18n.h"
#include "penalty_model.h"
#include "penalty_preferences.h"
#include "penalty_ui.h"
#include "../assets/music/penalty/penalty_sfx_pcm.h"
#include "../assets/fonts/penalty/penalty_fonts.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdatomic.h>

static const char *TAG = "penalty";
static penalty_model_t s_model;
static lv_timer_t *s_timer;
static lv_obj_t *s_error_screen;
static QueueHandle_t s_sound_queue;
static SemaphoreHandle_t s_stopped;
static TaskHandle_t s_worker;
static atomic_bool s_stop, s_muted, s_audio_ok, s_pending_reset;
static atomic_bool s_language_save_pending;
static atomic_int s_language_to_save;
static atomic_int s_battery;
static bool s_ready, s_input_reset, s_dimmed;
static penalty_language_t s_initial_language = PENALTY_DEFAULT_LANGUAGE;
static penalty_audio_tracker_t s_audio_tracker;
static uint64_t s_last_action;
static uint32_t s_max_update_us, s_max_input_age_ms;

typedef struct { penalty_sfx_event_t event; uint64_t at_ms; } sound_t;

static uint64_t now_ms(void) { return (uint64_t)esp_timer_get_time() / 1000; }

static void save_pending_language(void) {
    if (!atomic_exchange(&s_language_save_pending, false)) return;
    penalty_language_t language = (penalty_language_t)atomic_load(&s_language_to_save);
    esp_err_t err = penalty_preferences_save_language(language);
    if (err != ESP_OK) ESP_LOGW(TAG, "Language preference save failed: %s", esp_err_to_name(err));
}

static void send_sound(penalty_sfx_event_t event) {
    penalty_sfx_cue_t cue;
    if (!s_sound_queue || atomic_load(&s_stop) || !penalty_audio_cue(event, &cue)) return;
    if (atomic_load(&s_muted) && !cue.audible_when_muted) return;
    const sound_t sound = {event, now_ms()};
    if (xQueueSend(s_sound_queue, &sound, 0) == pdTRUE) return;
    if (cue.priority >= PENALTY_SFX_PRIORITY_ACTION) {
        xQueueReset(s_sound_queue);
        (void)xQueueSend(s_sound_queue, &sound, 0);
    }
}

static bool sound_cancelled(const penalty_sfx_cue_t *cue) {
    return atomic_load(&s_stop) || (atomic_load(&s_muted) && !cue->audible_when_muted);
}

static bool write_pcm(const int16_t *pcm, size_t sample_count,
                      const penalty_sfx_cue_t *cue) {
    for (size_t offset = 0; offset < sample_count && !sound_cancelled(cue); offset += 160) {
        size_t count = sample_count - offset < 160 ? sample_count - offset : 160;
        if (bsp_audio_write(pcm + offset, count * sizeof(*pcm)) != ESP_OK) return false;
    }
    return !sound_cancelled(cue);
}

static bool play_tone(uint16_t hz, uint16_t ms, const penalty_sfx_cue_t *cue) {
    int16_t samples[160];
    uint32_t phase = 0;
    unsigned total = ms * 16;
    for (unsigned offset = 0; offset < total && !sound_cancelled(cue); offset += 160) {
        unsigned count = total - offset < 160 ? total - offset : 160;
        for (unsigned i = 0; i < count; ++i) {
            unsigned pos = offset + i;
            unsigned envelope = pos < 160 ? pos : total - pos < 160 ? total - pos : 160;
            phase = (phase + hz) % 16000;
            samples[i] = (int16_t)((phase < 8000 ? 2500 : -2500) * (int)envelope / 160);
        }
        if (bsp_audio_write(samples, count * sizeof(samples[0])) != ESP_OK) return false;
    }
    return !sound_cancelled(cue);
}

static bool play_silence(uint16_t ms, const penalty_sfx_cue_t *cue) {
    int16_t samples[160] = {0};
    unsigned total = ms * 16;
    for (unsigned offset = 0; offset < total && !sound_cancelled(cue); offset += 160) {
        unsigned count = total - offset < 160 ? total - offset : 160;
        if (bsp_audio_write(samples, count * sizeof(samples[0])) != ESP_OK) return false;
    }
    return !sound_cancelled(cue);
}

static bool play_segment(const penalty_sfx_segment_t *segment,
                         const penalty_sfx_cue_t *cue) {
    switch (segment->kind) {
    case PENALTY_SFX_SEGMENT_TONE:
        return play_tone(segment->hz, segment->ms, cue);
    case PENALTY_SFX_SEGMENT_SILENCE:
        return play_silence(segment->ms, cue);
    case PENALTY_SFX_SEGMENT_KICK_PCM:
        return write_pcm(penalty_sfx_kick_pcm, penalty_sfx_kick_pcm_samples, cue);
    case PENALTY_SFX_SEGMENT_SAVE_PCM:
        return write_pcm(penalty_sfx_save_pcm, penalty_sfx_save_pcm_samples, cue);
    case PENALTY_SFX_SEGMENT_GOAL_PCM:
        return write_pcm(penalty_sfx_goal_pcm, penalty_sfx_goal_pcm_samples, cue);
    default:
        return true;
    }
}

static void play_sound(const sound_t *sound, const penalty_sfx_cue_t *cue) {
    for (unsigned i = 0; i < cue->segment_count && !sound_cancelled(cue); ++i) {
        if (!play_segment(&cue->segments[i], cue)) {
            if (!sound_cancelled(cue)) {
                atomic_store(&s_audio_ok, false);
                ESP_LOGW(TAG, "Audio PCM write failed; continuing silently");
            }
            return;
        }
    }
    /* End on silence so a DMA tail never holds the last nonzero sample. */
    if (!sound_cancelled(cue) && !play_silence(10, cue)) {
        atomic_store(&s_audio_ok, false);
        ESP_LOGW(TAG, "Audio silence-tail write failed; continuing silently");
    }
    (void)sound;
}

static void service_task(void *arg) {
    (void)arg;
    esp_err_t format_err = bsp_audio_set_format(16000, 16, 1);
    esp_err_t volume_err = format_err == ESP_OK
        ? bsp_audio_set_volume(PENALTY_AUDIO_VOLUME_PERCENT) : format_err;
    atomic_store(&s_audio_ok, format_err == ESP_OK && volume_err == ESP_OK);
    if (format_err != ESP_OK)
        ESP_LOGW(TAG, "Audio format open failed: %s", esp_err_to_name(format_err));
    else if (volume_err != ESP_OK)
        ESP_LOGW(TAG, "Audio volume setup failed: %s", esp_err_to_name(volume_err));
    uint64_t next_battery = 0;
    while (!atomic_load(&s_stop)) {
        save_pending_language();
        if (now_ms() >= next_battery) {
            atomic_store(&s_battery, bsp_battery_soc());
            next_battery = now_ms() + 10000;
        }
        sound_t sound;
        if (xQueueReceive(s_sound_queue, &sound, pdMS_TO_TICKS(100)) == pdTRUE) {
            penalty_sfx_cue_t cue;
            if (penalty_audio_cue(sound.event, &cue) && !atomic_load(&s_stop) &&
                atomic_load(&s_audio_ok) && now_ms() - sound.at_ms <= cue.stale_ms &&
                (!atomic_load(&s_muted) || cue.audible_when_muted))
                play_sound(&sound, &cue);
        }
    }
    save_pending_language();
    ESP_LOGI(TAG, "Worker stack remaining: %u", (unsigned)uxTaskGetStackHighWaterMark(NULL));
    xSemaphoreGive(s_stopped);
    vTaskDelete(NULL); // no access to page-owned data after the handshake
}

static void update(uint64_t now, const penalty_model_t *before) {
    penalty_sfx_event_t events[3];
    size_t count = penalty_audio_events(&s_audio_tracker, before, &s_model, now,
                                        events, sizeof(events) / sizeof(events[0]));
    for (size_t i = 0; i < count; ++i) send_sound(events[i]);
    atomic_store(&s_muted, s_model.muted);
    penalty_ui_update(&s_model, now, atomic_load(&s_battery), atomic_load(&s_audio_ok), s_input_reset);
}

static void tick(lv_timer_t *timer) {
    (void)timer;
    if (!s_ready) return;
    uint64_t start = (uint64_t)esp_timer_get_time();
    uint64_t now = start / 1000;
    penalty_model_t before = s_model;
    if (atomic_exchange(&s_pending_reset, false)) {
        penalty_model_resync(&s_model, now);
        s_input_reset = true;
    }
    // Allow up to 100 ms for an already timestamped press to traverse the
    // input task. The model still judges the 4000 ms deadline by event time.
    uint64_t tick_at = now;
    if (s_model.state == PENALTY_CHARGE && now >= s_model.since_ms + PENALTY_TIMEOUT_MS &&
        now < s_model.since_ms + PENALTY_TIMEOUT_MS + 100)
        tick_at = s_model.since_ms + PENALTY_TIMEOUT_MS - 1;
    penalty_model_tick(&s_model, tick_at);
    update(now, &before);
    if (!s_dimmed && now - s_last_action >= 60000 &&
        s_model.state != PENALTY_CHARGE && s_model.state != PENALTY_FLIGHT) {
        bsp_display_backlight(15);
        s_dimmed = true;
    }
    uint32_t spent = (uint32_t)((uint64_t)esp_timer_get_time() - start);
    if (spent > s_max_update_us) s_max_update_us = spent;
}

void penalty_app_set_initial_language(penalty_language_t language) {
    s_initial_language = (unsigned)language < PENALTY_LANGUAGE_COUNT
        ? language : PENALTY_DEFAULT_LANGUAGE;
}

void penalty_app_enter(void) {
    penalty_model_init(&s_model, esp_random(), now_ms());
    penalty_model_set_language(&s_model, s_initial_language);
    s_last_action = now_ms();
    s_dimmed = s_input_reset = false;
    s_max_update_us = s_max_input_age_ms = 0;
    atomic_store(&s_battery, -1);
    atomic_store(&s_audio_ok, false);
    atomic_store(&s_muted, false);
    atomic_store(&s_stop, false);
    atomic_store(&s_pending_reset, false);
    atomic_store(&s_language_save_pending, false);
    atomic_store(&s_language_to_save, s_initial_language);
    s_audio_tracker = (penalty_audio_tracker_t){0};
    s_ready = penalty_ui_create();
    if (s_ready) {
        penalty_ui_update(&s_model, now_ms(), -1, false, false);
        s_timer = lv_timer_create(tick, 50, NULL);
        s_ready = s_timer != NULL;
        if (s_timer) lv_timer_pause(s_timer); // start() publishes worker resources first
    }
    if (!s_ready) {
        penalty_ui_destroy();
        s_error_screen = lv_obj_create(NULL);
        lv_obj_t *label = lv_label_create(s_error_screen);
        lv_obj_set_style_text_font(label, s_initial_language == PENALTY_LANGUAGE_ZH_CN
            ? &penalty_font_zh_14 : &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(label, penalty_text(s_initial_language, PENALTY_TEXT_ERROR_UNAVAILABLE));
        lv_obj_center(label);
        lv_screen_load(s_error_screen);
    }
    ESP_LOGI(TAG, "Enter heap=%u largest=%u", (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

esp_err_t penalty_app_start(void) {
    if (!s_ready) return ESP_ERR_NO_MEM;
    s_sound_queue = xQueueCreate(4, sizeof(sound_t));
    s_stopped = xSemaphoreCreateBinary();
    if (!s_sound_queue || !s_stopped ||
        xTaskCreate(service_task, "penalty_service", 3072, NULL, 4, &s_worker) != pdPASS) {
        if (s_sound_queue) vQueueDelete(s_sound_queue);
        if (s_stopped) vSemaphoreDelete(s_stopped);
        s_sound_queue = NULL; s_stopped = NULL; s_worker = NULL;
        ESP_LOGW(TAG, "No service worker; play remains available without sound/battery");
    }
    if (!bsp_lvgl_lock(500)) return ESP_ERR_TIMEOUT;
    lv_timer_resume(s_timer);
    bsp_lvgl_unlock();
    return ESP_OK;
}

esp_err_t penalty_app_stop(void) {
    // Prevent timers from producing more sound before asking the worker to stop.
    if (!bsp_lvgl_lock(500)) return ESP_ERR_TIMEOUT;
    s_ready = false;
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    bsp_lvgl_unlock();
    atomic_store(&s_stop, true);
    if (s_worker) {
        const sound_t wake = {0};
        xQueueReset(s_sound_queue);
        (void)xQueueSend(s_sound_queue, &wake, 0);
        if (xSemaphoreTake(s_stopped, pdMS_TO_TICKS(2000)) != pdTRUE) {
            ESP_LOGE(TAG, "Worker stop timed out; retain UI and retry long OK");
            return ESP_ERR_TIMEOUT;
        }
        s_worker = NULL;
    }
    if (!bsp_lvgl_lock(500)) return ESP_ERR_TIMEOUT;
    esp_err_t err = bsp_lvgl_set_landscape(false);
    if (err == ESP_OK) lv_obj_add_flag(lv_screen_active(), LV_OBJ_FLAG_HIDDEN);
    bsp_lvgl_unlock();
    if (err != ESP_OK) return err;
    if (s_sound_queue) { vQueueDelete(s_sound_queue); s_sound_queue = NULL; }
    if (s_stopped) { vSemaphoreDelete(s_stopped); s_stopped = NULL; }
    return ESP_OK;
}

void penalty_app_exit(void) {
    penalty_ui_destroy();
    if (s_error_screen) { lv_obj_delete(s_error_screen); s_error_screen = NULL; }
    bsp_display_backlight(100);
    ESP_LOGI(TAG, "Exit heap=%u largest=%u min_heap=%u max_update_us=%u max_input_age_ms=%u",
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
             (unsigned)esp_get_minimum_free_heap_size(),
             (unsigned)s_max_update_us, (unsigned)s_max_input_age_ms);
}

bool penalty_app_key(bsp_btn_t btn, bsp_btn_ev_t event, uint64_t at, bool lost) {
    if (!bsp_lvgl_lock(100)) {
        atomic_store(&s_pending_reset, true);
        return false;
    }
    uint64_t now = now_ms();
    if (!s_ready) { bsp_lvgl_unlock(); return true; }
    if (event == BSP_BTN_PRESS && now >= at && now - at > s_max_input_age_ms)
        s_max_input_age_ms = (uint32_t)(now - at);
    if (lost || (event == BSP_BTN_PRESS && now > at && now - at > 100)) {
        penalty_model_resync(&s_model, now);
        s_input_reset = true;
    } else if (event == BSP_BTN_PRESS) {
        s_last_action = now;
        if (s_dimmed) {
            bsp_display_backlight(100); s_dimmed = false;
            // Wake-up is not also a shot.
        } else {
            penalty_model_t before = s_model;
            penalty_input_t input = btn == BSP_BTN_UP ? PENALTY_INPUT_UP :
                btn == BSP_BTN_DOWN ? PENALTY_INPUT_DOWN : PENALTY_INPUT_OK;
            penalty_model_input(&s_model, input, at);
            if (before.language != s_model.language) {
                s_initial_language = s_model.language;
                atomic_store(&s_language_to_save, s_model.language);
                atomic_store(&s_language_save_pending, true);
            }
            s_input_reset = false;
            update(now, &before);
        }
    }
    bool exit = s_model.state == PENALTY_EXITING;
    bsp_lvgl_unlock();
    return exit;
}
