#include "penalty_multiplayer.h"
#include "penalty_multiplayer_core.h"
#include "penalty_ble_session.h"
#include "penalty_app.h"
#include "penalty_i18n.h"
#include "penalty_ui.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

typedef struct { bsp_btn_t button; bsp_btn_ev_t event; uint64_t at; bool lost; } input_t;
typedef struct {
    penalty_model_t model;
    penalty_mp_phase_t phase;
    penalty_ble_link_state_t link;
    penalty_ble_lobby_t lobbies[4];
    uint8_t lobby_count, selection;
    uint16_t code;
    uint32_t numeric;
    uint32_t goals[2], display_round, sudden_round, recovery_seconds;
    penalty_duel_match_result_t match_result;
    penalty_duel_outcome_t outcome;
    int battery, error;
    bool host, ready, have_numeric, settings_sent, settings_confirmed, next_ready, paired;
#if CONFIG_PENALTY_DEV_BLE_FAULTS
    bool dev_menu, dev_down_pending, dev_ok_pending;
#endif
} view_t;

static penalty_mp_core_t s_core; /* Owned by multiplayer worker only. */
_Static_assert(sizeof(penalty_mp_core_t) <= 2048, "Multiplayer core RAM budget");
static view_t s_view;
static portMUX_TYPE s_view_lock = portMUX_INITIALIZER_UNLOCKED;
static lv_timer_t *s_timer;
static QueueHandle_t s_input;
static SemaphoreHandle_t s_stopped;
static TaskHandle_t s_worker;
static atomic_bool s_stop, s_input_overflow;
static atomic_bool s_number_visible;
static atomic_uint s_visible_number;
static atomic_uint s_input_peak, s_input_drops;
static uint32_t s_entry_heap;
static bool s_host, s_muted, s_created;
static penalty_language_t s_language;
static penalty_difficulty_t s_difficulty;

static uint64_t now_ms(void) { return (uint64_t)esp_timer_get_time() / 1000; }
static const char *text(penalty_text_id_t id) { return penalty_text(s_language, id); }

static void render(lv_timer_t *timer) {
    (void)timer;
    view_t v;
    taskENTER_CRITICAL(&s_view_lock);
    v = s_view;
    taskEXIT_CRITICAL(&s_view_lock);
    char header[96], heading[96], detail[256], footer[160];
    snprintf(header, sizeof(header), "%s", text(PENALTY_TEXT_MP_HEADER));
    snprintf(heading, sizeof(heading), text(PENALTY_TEXT_MP_LOBBY_FMT), v.code);
    snprintf(detail, sizeof(detail), "%s", text(PENALTY_TEXT_MP_CONNECT));
    snprintf(footer, sizeof(footer), "%s", text(PENALTY_TEXT_HOLD_COVER));
    bool field = false, menu = false;
    atomic_store(&s_number_visible, false);
    if (v.phase == PENALTY_MP_RECONNECT) {
        snprintf(heading,sizeof(heading),text(PENALTY_TEXT_MP_RECOVERY_FMT),v.recovery_seconds);
        snprintf(detail,sizeof(detail),"%s",text(PENALTY_TEXT_MP_RECOVERY_HINT));
        snprintf(footer,sizeof(footer),"%s",text(PENALTY_TEXT_MP_CANCEL));
    } else if (v.phase == PENALTY_MP_DONE) {
        unsigned you=v.host ? 0 : 1;
        bool won=v.match_result==(v.host ? PENALTY_DUEL_HOST_WIN : PENALTY_DUEL_GUEST_WIN);
        snprintf(heading,sizeof(heading),"%s",text(won ? PENALTY_TEXT_MP_WON : PENALTY_TEXT_MP_LOST));
        char stage[64];
        if (v.sudden_round) snprintf(stage,sizeof(stage),text(PENALTY_TEXT_MP_SUDDEN_FMT),v.sudden_round);
        else snprintf(stage,sizeof(stage),"%s",text(PENALTY_TEXT_MP_REGULAR));
        snprintf(detail,sizeof(detail),text(PENALTY_TEXT_MP_SCORE_FMT),v.display_round,v.goals[you],v.goals[1-you]);
        size_t used=strlen(detail);
        snprintf(detail+used,sizeof(detail)-used,"\n\n%s",stage);
        snprintf(footer,sizeof(footer),"%s",text(PENALTY_TEXT_MP_DONE));
    } else if (v.error || v.phase == PENALTY_MP_FAILED || v.link == PENALTY_BLE_LINK_ERROR ||
        v.link == PENALTY_BLE_LINK_REJECTED || v.link == PENALTY_BLE_LINK_DISCONNECTED) {
        snprintf(heading, sizeof(heading), "%s", text(PENALTY_TEXT_MP_DISCONNECTED));
        if (v.phase==PENALTY_MP_FAILED && v.match_result==PENALTY_DUEL_NO_CONTEST)
            snprintf(detail,sizeof(detail),"%s",text(PENALTY_TEXT_MP_NO_CONTEST));
        else snprintf(detail, sizeof(detail), text(PENALTY_TEXT_MP_ERROR_FMT), v.error);
        snprintf(footer, sizeof(footer), "%s", text(PENALTY_TEXT_MP_RETRY));
    } else if (v.link == PENALTY_BLE_LINK_NUMERIC_CONFIRM && v.have_numeric) {
        snprintf(heading, sizeof(heading), text(PENALTY_TEXT_MP_COMPARE_FMT),
                 (unsigned long)v.numeric);
        snprintf(detail, sizeof(detail), "%s", text(PENALTY_TEXT_MP_COMPARE_HINT));
        snprintf(footer, sizeof(footer), "%s", text(PENALTY_TEXT_MP_ACCEPT_HINT));
        atomic_store(&s_visible_number, v.numeric);
        atomic_store(&s_number_visible, true);
    } else if (!v.ready) {
        atomic_store(&s_number_visible, false);
        if (!v.host && v.link == PENALTY_BLE_LINK_SCANNING) {
            menu = v.lobby_count != 0;
            snprintf(heading, sizeof(heading), "%s", text(PENALTY_TEXT_MP_SCAN));
            size_t used = 0;
            detail[0] = 0;
            for (unsigned i = 0; i < v.lobby_count; ++i) {
                int n = snprintf(detail + used, sizeof(detail) - used, "%s%s%04u%s",
                    i ? "\n" : "", i == v.selection ? PENALTY_MENU_SELECTED_PREFIX : "", v.lobbies[i].code,
                    i == v.selection ? PENALTY_MENU_SELECTED_SUFFIX : "");
                if (n < 0 || (size_t)n >= sizeof(detail) - used) break;
                used += n;
            }
            if (!v.lobby_count) snprintf(detail, sizeof(detail), "%s", text(PENALTY_TEXT_MP_CONNECT));
            snprintf(footer, sizeof(footer), "%s", text(PENALTY_TEXT_MENU_FOOTER));
        } else if (v.host && v.link == PENALTY_BLE_LINK_ADVERTISING) {
            snprintf(detail, sizeof(detail), "%s", text(PENALTY_TEXT_MP_JOIN));
        }
    } else if (v.phase == PENALTY_MP_SETTINGS) {
        snprintf(heading, sizeof(heading), "%s", text(PENALTY_TEXT_MP_DIFFICULTY));
        snprintf(detail, sizeof(detail), "%s\n\n%s",
            penalty_difficulty_text_localized(s_language, v.model.difficulty),
            v.host ? (v.settings_sent ? text(PENALTY_TEXT_MP_HOST_WAIT) :
                text(PENALTY_TEXT_SETTINGS_FOOTER)) : !v.settings_sent ?
                text(PENALTY_TEXT_MP_GUEST_WAIT) : v.settings_confirmed ?
                text(PENALTY_TEXT_MP_WAIT_COIN) : text(PENALTY_TEXT_MP_CONFIRM_DIFF));
    } else if (v.phase == PENALTY_MP_COIN || v.phase == PENALTY_MP_COIN_SPIN) {
        snprintf(heading, sizeof(heading), "%s", text(PENALTY_TEXT_COIN_HEADING));
        snprintf(detail, sizeof(detail), "%s", v.phase == PENALTY_MP_COIN_SPIN
            ? text(PENALTY_TEXT_COIN_SPIN) : v.host ? text(PENALTY_TEXT_COIN_READY)
            : text(PENALTY_TEXT_MP_WAIT_COIN));
        field = v.phase == PENALTY_MP_COIN_SPIN;
    } else {
        unsigned you = v.host ? 0 : 1;
        snprintf(header, sizeof(header), text(PENALTY_TEXT_MP_SCORE_FMT),
                 v.display_round, v.goals[you], v.goals[1 - you]);
        field = true;
        bool keeper = v.model.turn_role == PENALTY_ROLE_KEEPER;
        snprintf(detail, sizeof(detail), "%s", text(keeper ? PENALTY_TEXT_MP_KEEP : PENALTY_TEXT_MP_SHOOT));
        if (v.phase == PENALTY_MP_AIM) {
            snprintf(footer, sizeof(footer), text(keeper ? PENALTY_TEXT_AIM_KEEPER_FMT : PENALTY_TEXT_AIM_FMT),
                     penalty_direction_text(s_language, v.model.current.direction));
        } else if (v.phase == PENALTY_MP_CHARGE) {
            snprintf(footer, sizeof(footer), text(keeper ? PENALTY_TEXT_CHARGE_KEEPER_FMT : PENALTY_TEXT_CHARGE_FMT),
                     v.model.current.power);
        } else if (v.phase == PENALTY_MP_WAIT) {
            snprintf(detail, sizeof(detail), "%s", text(PENALTY_TEXT_MP_WAIT_ACTION));
            footer[0] = 0;
        } else if (v.phase == PENALTY_MP_ANIMATION) {
            snprintf(detail, sizeof(detail), "%s", text(keeper ? PENALTY_TEXT_FLIGHT_KEEPER : PENALTY_TEXT_FLIGHT));
            footer[0] = 0;
        } else {
            snprintf(detail, sizeof(detail), "%s", v.outcome == PENALTY_DUEL_MISS_ACCURACY
                ? text(PENALTY_TEXT_MP_MISS)
                : penalty_outcome_text_localized(s_language, v.model.current.outcome));
            snprintf(footer, sizeof(footer), "%s", text(v.phase == PENALTY_MP_DONE
                ? PENALTY_TEXT_MP_DONE : v.next_ready ? PENALTY_TEXT_MP_NEXT_WAIT : PENALTY_TEXT_MP_RESULT_NEXT));
        }
    }
#if CONFIG_PENALTY_DEV_BLE_FAULTS
    if (v.dev_menu) {
        snprintf(header,sizeof(header),"DEV BLE TEST");
        snprintf(heading,sizeof(heading),"Disconnect test");
        snprintf(detail,sizeof(detail),"OK: disconnect 10s\nDOWN: disconnect 35s\nUP: back");
        snprintf(footer,sizeof(footer),"Keep both devices powered");
        field=false;
        menu=false;
    }
#endif
    if (menu)
        penalty_ui_menu_panel(&v.model, now_ms(), v.battery, penalty_app_audio_available(),
                              header, heading, detail, footer);
    else
        penalty_ui_panel(&v.model, now_ms(), v.battery, penalty_app_audio_available(),
                         header, heading, detail, footer, field);
}

static void publish(view_t *v, uint64_t now) {
    v->phase = s_core.phase;
    v->settings_sent = s_core.settings_sent;
    v->settings_confirmed = s_core.settings_confirmed;
    v->next_ready = s_core.next_ready[s_core.peer];
    v->display_round = penalty_round_for_kick(s_core.duel.round_id);
    v->outcome = s_core.duel.round_result.outcome;
    v->match_result=s_core.duel.match_result;
    v->sudden_round=s_core.duel.sudden_death ? s_core.duel.sudden_death_round : 0;
    v->recovery_seconds=s_core.recovery_deadline>now ? (s_core.recovery_deadline-now+999)/1000 : 0;
    memcpy(v->goals, s_core.duel.goals, sizeof(v->goals));
    penalty_mp_project(&s_core, &v->model, s_language, s_muted, now);
    taskENTER_CRITICAL(&s_view_lock);
    s_view = *v;
    taskEXIT_CRITICAL(&s_view_lock);
}

#if CONFIG_PENALTY_DEV_BLE_FAULTS
static bool dev_test_available(const view_t *v) {
    return v->ready && !v->error && (s_core.phase==PENALTY_MP_AIM ||
        s_core.phase==PENALTY_MP_WAIT || s_core.phase==PENALTY_MP_RESULT);
}

/* Worker-only UI action. DOWN is deferred until CLICK in these idle phases,
 * so opening with DOWN LONG never changes an unlocked target. */
static bool dev_test_input(view_t *v,input_t *input) {
    if (!dev_test_available(v)) { v->dev_menu=v->dev_down_pending=v->dev_ok_pending=false; return false; }
    if (input->button==BSP_BTN_DOWN && input->event==BSP_BTN_PRESS) {
        v->dev_down_pending=true;
        return true;
    }
    if (input->button==BSP_BTN_DOWN && input->event==BSP_BTN_LONG) {
        v->dev_menu=true; v->dev_down_pending=false;
        return true;
    }
    if (input->button==BSP_BTN_DOWN && input->event==BSP_BTN_CLICK) {
        if (!v->dev_down_pending) return true;
        v->dev_down_pending=false;
        input->event=BSP_BTN_PRESS;
    }
    if (!v->dev_menu) return false;
    if (input->button==BSP_BTN_OK && input->event==BSP_BTN_PRESS) {
        v->dev_ok_pending=true;
        return true;
    }
    if (input->button==BSP_BTN_OK && input->event==BSP_BTN_CLICK) {
        if (!v->dev_ok_pending) return true;
        v->dev_ok_pending=false;
        input->event=BSP_BTN_PRESS;
    }
    if (input->event==BSP_BTN_PRESS) {
        unsigned seconds=input->button==BSP_BTN_OK ? 10 : input->button==BSP_BTN_DOWN ? 35 : 0;
        if (seconds) {
            esp_err_t error=penalty_ble_session_dev_disconnect(seconds);
            if (error!=ESP_OK) ESP_LOGW("penalty_mp","DEV BLE TEST request rejected: %s",esp_err_to_name(error));
        }
        v->dev_menu=v->dev_ok_pending=v->dev_down_pending=false;
    }
    return true; /* Test-panel actions never enter the gameplay model. */
}
#endif

static esp_err_t begin(view_t *v) {
    esp_err_t error = penalty_ble_session_stop();
    if (error != ESP_OK) return error;
    memset(v, 0, sizeof(*v));
    v->host = s_host;
    v->battery = -1;
    v->code = s_host ? esp_random() % 10000 : 0;
    uint8_t id[16];
    esp_fill_random(id, sizeof(id));
    penalty_mp_init(&s_core, s_host ? PENALTY_DUEL_HOST : PENALTY_DUEL_GUEST,
                    s_difficulty, id, esp_random(), now_ms());
    s_core.random_fn = esp_random;
    atomic_store(&s_number_visible, false);
    const penalty_ble_session_config_t config = {
        .role = s_host ? PENALTY_BLE_LINK_HOST : PENALTY_BLE_LINK_GUEST,
        .lobby_code = v->code,
        .boot_instance_id = ((uint64_t)esp_random() << 32) | esp_random(),
        .browse_lobbies = !s_host,
        .m4_match = true,
    };
    return penalty_ble_session_start(&config);
}

static void worker(void *arg) {
    (void)arg;
    view_t v = {0};
    v.error = begin(&v);
    uint64_t next_battery = 0, next_sample = 0, last_action = now_ms();
    uint32_t minimum_heap = esp_get_free_heap_size();
    uint32_t minimum_largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    uint64_t maximum_input_age = 0;
    bool dimmed = false, kick_sound = false, ending=false;
    penalty_mp_phase_t last_phase = s_core.phase;
    while (!atomic_load(&s_stop)) {
        input_t input;
        if (xQueueReceive(s_input, &input, pdMS_TO_TICKS(20)) == pdTRUE) {
            uint64_t age = now_ms() >= input.at ? now_ms() - input.at : 0;
            if (age > maximum_input_age) maximum_input_age = age;
            last_action = now_ms();
            if (dimmed) { bsp_display_backlight(100); dimmed = false; }
            else if (input.lost || (input.event == BSP_BTN_PRESS &&
                now_ms() > input.at + 100)) {
                /* Drop uncertain input. Only an uncommitted charge resets. */
                if (s_core.phase == PENALTY_MP_CHARGE) s_core.phase = PENALTY_MP_AIM;
#if CONFIG_PENALTY_DEV_BLE_FAULTS
                v.dev_menu=v.dev_down_pending=v.dev_ok_pending=false;
#endif
#if CONFIG_PENALTY_DEV_BLE_FAULTS
            } else if (dev_test_input(&v,&input)) {
                /* Network work is queued to the BLE owner; never blocks input. */
#endif
            } else if (v.link == PENALTY_BLE_LINK_NUMERIC_CONFIRM) {
                if (input.button == BSP_BTN_OK && input.event == BSP_BTN_CLICK &&
                    atomic_load(&s_number_visible) &&
                    atomic_load(&s_visible_number) == v.numeric)
                    v.error = penalty_ble_session_confirm_numeric(true);
                else if (input.button == BSP_BTN_DOWN && input.event == BSP_BTN_PRESS)
                    v.error = penalty_ble_session_confirm_numeric(false);
            } else if (input.event == BSP_BTN_PRESS) {
                if (s_core.phase==PENALTY_MP_RECONNECT) {
                    if (input.button==BSP_BTN_DOWN) penalty_mp_cancel(&s_core);
                } else if (s_core.phase==PENALTY_MP_DONE && input.button==BSP_BTN_OK) {
                    v.error=begin(&v); ending=false;
                } else if (v.error || v.link == PENALTY_BLE_LINK_DISCONNECTED ||
                    v.link == PENALTY_BLE_LINK_ERROR || s_core.phase == PENALTY_MP_FAILED) {
                    if (input.button == BSP_BTN_OK) v.error = begin(&v);
                } else if (!v.ready && !s_host && v.link == PENALTY_BLE_LINK_SCANNING) {
                    if (v.lobby_count && input.button == BSP_BTN_OK) {
                        v.code = v.lobbies[v.selection].code;
                        v.error = penalty_ble_session_select_lobby(v.lobbies[v.selection].index);
                    } else if (v.lobby_count) v.selection = (v.selection +
                        (input.button == BSP_BTN_UP ? v.lobby_count - 1 : 1)) % v.lobby_count;
                } else if (v.ready) penalty_mp_input(&s_core,
                    input.button == BSP_BTN_UP ? PENALTY_INPUT_UP :
                    input.button == BSP_BTN_DOWN ? PENALTY_INPUT_DOWN : PENALTY_INPUT_OK, input.at);
            }
        }
        if (atomic_exchange(&s_input_overflow, false) && s_core.phase == PENALTY_MP_CHARGE)
            s_core.phase = PENALTY_MP_AIM;
        penalty_ble_session_event_t event;
        while (penalty_ble_session_poll_event(&event, 0)) {
            if (event.type == PENALTY_BLE_SESSION_EVENT_NUMERIC_COMPARISON) {
                v.numeric = event.numeric_value; v.have_numeric = true;
            } else if (event.type == PENALTY_BLE_SESSION_EVENT_READY) {
                v.ready=v.paired=true;
                penalty_mp_link_ready(&s_core,now_ms());
            }
            else if (event.type == PENALTY_BLE_SESSION_EVENT_MESSAGE && v.ready) {
                penalty_mp_packet_t packet = {.type = event.message.type,
                                               .length = event.message.payload_len};
                memcpy(packet.bytes, event.message.payload, packet.length);
                if (!penalty_mp_receive(&s_core, &packet, now_ms())) v.error = ESP_ERR_INVALID_RESPONSE;
                else v.error=penalty_ble_session_ack_message(event.message.epoch,event.message.seq);
            } else if (event.type == PENALTY_BLE_SESSION_EVENT_DISCONNECTED && v.paired) {
                v.ready=false;
                if (s_core.phase!=PENALTY_MP_DONE || !penalty_ble_session_match_ended()) {
                    penalty_mp_pause(&s_core,now_ms()); ending=false;
                }
            } else if (event.type == PENALTY_BLE_SESSION_EVENT_ERROR ||
                       event.type == PENALTY_BLE_SESSION_EVENT_DISCONNECTED) {
                v.error = event.error ? event.error : ESP_FAIL;
                v.ready = false;
                if (s_core.phase==PENALTY_MP_RECONNECT) {
                    penalty_mp_tick(&s_core,s_core.recovery_deadline);
                } else {
                    penalty_duel_abort(&s_core.duel, PENALTY_DUEL_PROTOCOL_FAILED);
                    s_core.phase = PENALTY_MP_FAILED;
                }
            }
        }
        v.link = penalty_ble_session_state();
        if (!v.ready && !v.paired && !s_host && v.link == PENALTY_BLE_LINK_SCANNING) {
            v.lobby_count = penalty_ble_session_get_lobbies(v.lobbies, 4);
            if (v.selection >= v.lobby_count) v.selection = 0;
        }
        penalty_mp_tick(&s_core, now_ms());
#if CONFIG_PENALTY_DEV_BLE_FAULTS
        if (!dev_test_available(&v)) v.dev_menu=v.dev_down_pending=v.dev_ok_pending=false;
#endif
        if (v.ready && !v.error) {
            penalty_mp_packet_t packet;
            while (penalty_mp_pop(&s_core, &packet)) {
                penalty_ble_message_t message = {.type = packet.type,
                    .flags = PENALTY_BLE_FLAG_ACK_REQUIRED, .payload_len = packet.length};
                memcpy(message.payload, packet.bytes, packet.length);
                esp_err_t error = penalty_ble_session_send_message(&message);
                if (error != ESP_OK) { v.error = error; break; }
            }
        }
        if (!ending && (s_core.phase==PENALTY_MP_DONE || s_core.phase==PENALTY_MP_FAILED)) {
            (void)penalty_ble_session_end_match(); ending=true;
        }
        if (s_core.phase != last_phase) {
            if (s_core.phase == PENALTY_MP_COIN_SPIN) penalty_app_sound(PENALTY_SFX_COIN_SPIN);
            if (last_phase == PENALTY_MP_COIN_SPIN) penalty_app_sound(PENALTY_SFX_COIN_LAND);
            if (s_core.phase == PENALTY_MP_CHARGE) penalty_app_sound(PENALTY_SFX_CHARGE);
            if (s_core.phase == PENALTY_MP_ANIMATION) kick_sound = false;
            if (last_phase == PENALTY_MP_ANIMATION &&
                (s_core.phase == PENALTY_MP_RESULT || s_core.phase == PENALTY_MP_DONE))
                penalty_app_sound(s_core.duel.round_result.outcome == PENALTY_DUEL_GOAL
                    ? PENALTY_SFX_GOAL : s_core.duel.round_result.outcome == PENALTY_DUEL_SAVE
                    ? PENALTY_SFX_SAVE : PENALTY_SFX_MISS);
            last_phase = s_core.phase;
        }
        if (s_core.phase == PENALTY_MP_ANIMATION && !kick_sound &&
            now_ms() >= s_core.since_ms + PENALTY_AUDIO_CONTACT_MS) {
            penalty_app_sound(PENALTY_SFX_KICK); kick_sound = true;
        }
        if (now_ms() >= next_battery) {
            v.battery = bsp_battery_soc(); next_battery = now_ms() + 10000;
        }
        if (now_ms() >= next_sample) {
            uint32_t heap = esp_get_free_heap_size();
            uint32_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
            if (heap < minimum_heap) minimum_heap = heap;
            if (largest < minimum_largest) minimum_largest = largest;
            next_sample = now_ms() + 1000;
        }
        if (!dimmed && now_ms() - last_action >= 60000 &&
            s_core.phase != PENALTY_MP_CHARGE && s_core.phase != PENALTY_MP_ANIMATION) {
            bsp_display_backlight(15); dimmed = true;
        }
        publish(&v, now_ms());
    }
    /* Only a requested page exit cancels an unfinished match. */
    if (atomic_load(&s_stop) && s_core.phase!=PENALTY_MP_DONE && v.paired && v.ready) {
        penalty_mp_cancel(&s_core);
        penalty_mp_packet_t packet;
        if (penalty_mp_pop(&s_core,&packet)) {
            penalty_ble_message_t message={.type=packet.type,.flags=PENALTY_BLE_FLAG_ACK_REQUIRED,.payload_len=packet.length};
            memcpy(message.payload,packet.bytes,packet.length);
            if (penalty_ble_session_send_message(&message)==ESP_OK) {
                (void)penalty_ble_session_end_match();
                uint64_t deadline=now_ms()+400;
                while (!penalty_ble_session_match_ended() && now_ms()<deadline) vTaskDelay(pdMS_TO_TICKS(20));
            }
        }
    }
    ESP_LOGI("penalty_mp", "resources entry=%lu sampled min heap/largest=%lu/%lu boot min=%lu worker stack=%u bytes input peak/drops=%u/%u max age=%llu ms",
        (unsigned long)s_entry_heap, (unsigned long)minimum_heap, (unsigned long)minimum_largest,
        (unsigned long)esp_get_minimum_free_heap_size(), (unsigned)uxTaskGetStackHighWaterMark(NULL),
        atomic_load(&s_input_peak), atomic_load(&s_input_drops), (unsigned long long)maximum_input_age);
    xSemaphoreGive(s_stopped);
    vTaskDelete(NULL);
}

bool penalty_multiplayer_enter(bool host, const penalty_model_t *context) {
    s_entry_heap = esp_get_free_heap_size();
    s_host = host; s_muted = context->muted; s_language = context->language;
    s_difficulty = context->difficulty;
    memset(&s_view, 0, sizeof(s_view));
    s_view.host = host; s_view.battery = -1;
    s_view.model.language = s_language; s_view.model.state = PENALTY_SETTINGS;
    s_created = penalty_ui_create();
    if (!s_created) return false;
    s_timer = lv_timer_create(render, 50, NULL);
    if (!s_timer) { penalty_ui_destroy(); s_created = false; return false; }
    lv_timer_pause(s_timer);
    render(NULL);
    return true;
}
esp_err_t penalty_multiplayer_start(void) {
    if (!s_created || !s_timer || s_worker) return ESP_ERR_INVALID_STATE;
    s_input = xQueueCreate(8, sizeof(input_t));
    s_stopped = xSemaphoreCreateBinary();
    atomic_store(&s_stop, false);
    atomic_store(&s_input_overflow, false);
    atomic_store(&s_input_peak, 0);
    atomic_store(&s_input_drops, 0);
    if (!s_input || !s_stopped) return ESP_ERR_NO_MEM;
    (void)penalty_app_audio_start(s_muted);
    if (xTaskCreate(worker, "penalty_mp", 6144, NULL, 4, &s_worker) != pdPASS) {
        s_worker = NULL; return ESP_ERR_NO_MEM;
    }
    if (!bsp_lvgl_lock(500)) return ESP_ERR_TIMEOUT;
    lv_timer_resume(s_timer);
    bsp_lvgl_unlock();
    return ESP_OK;
}
esp_err_t penalty_multiplayer_stop(void) {
    if (!bsp_lvgl_lock(500)) return ESP_ERR_TIMEOUT;
    if (s_timer) lv_timer_pause(s_timer);
    bsp_lvgl_unlock();
    atomic_store(&s_stop, true);
    if (s_worker) {
        if (xSemaphoreTake(s_stopped, pdMS_TO_TICKS(6500)) != pdTRUE) return ESP_ERR_TIMEOUT;
        s_worker = NULL;
    }
    esp_err_t error = penalty_ble_session_stop();
    if (error != ESP_OK) return error;
    error = penalty_app_audio_stop();
    if (error != ESP_OK) return error;
    if (s_input) vQueueDelete(s_input);
    if (s_stopped) vSemaphoreDelete(s_stopped);
    s_input = NULL; s_stopped = NULL;
    return ESP_OK;
}
void penalty_multiplayer_exit(void) {
    if (s_timer) lv_timer_delete(s_timer);
    s_timer = NULL;
    if (s_created) penalty_ui_destroy();
    s_created = false;
    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory); /* Shell holds the LVGL lock through page teardown. */
    ESP_LOGI("penalty_mp", "exit heap=%lu largest=%lu LVGL free/largest=%u/%u bytes",
        (unsigned long)esp_get_free_heap_size(),
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
        (unsigned)memory.free_size, (unsigned)memory.free_biggest_size);
    bsp_display_backlight(100);
}
void penalty_multiplayer_key(bsp_btn_t btn, bsp_btn_ev_t event, uint64_t at, bool lost) {
    if (!s_input || atomic_load(&s_stop)) return;
    const input_t input = {btn, event, at, lost};
    if (xQueueSend(s_input, &input, 0) != pdTRUE) {
        atomic_store(&s_input_overflow, true);
        atomic_fetch_add(&s_input_drops, 1);
    } else {
        unsigned waiting = (unsigned)uxQueueMessagesWaiting(s_input);
        unsigned peak = atomic_load(&s_input_peak);
        while (waiting > peak && !atomic_compare_exchange_weak(&s_input_peak, &peak, waiting)) {}
    }
}
