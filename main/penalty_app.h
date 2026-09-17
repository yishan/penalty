#pragma once

#include "bsp_button.h"
#include "penalty_model.h"
#include <stdbool.h>
#include <stdint.h>

void penalty_app_set_initial_language(penalty_language_t language);
void penalty_app_enter(void);
esp_err_t penalty_app_start(void);
esp_err_t penalty_app_stop(void);
void penalty_app_exit(void);
/* Lifecycle task; returns true to ask that task to stop and exit the page. */
bool penalty_app_key(bsp_btn_t btn, bsp_btn_ev_t event, uint64_t at_ms, bool input_lost);
