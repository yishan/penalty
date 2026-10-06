#pragma once

#include "bsp_button.h"
#include "penalty_model.h"

/* UI enter/exit require LVGL lock. Start/stop are shell-task operations. */
bool penalty_multiplayer_enter(bool host, const penalty_model_t *context);
esp_err_t penalty_multiplayer_start(void);
esp_err_t penalty_multiplayer_stop(void);
void penalty_multiplayer_exit(void);
void penalty_multiplayer_key(bsp_btn_t btn, bsp_btn_ev_t event, uint64_t at, bool lost);
