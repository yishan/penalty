#pragma once

#include "bsp_button.h"
#include "esp_err.h"

#include <stdbool.h>

#define PENALTY_BLE_DIAG_LOBBY_CODE 4072u

/* Temporary M2 hardware-validation page. All enter/exit calls require the
 * LVGL lock. Network work is owned by the diagnostic worker task. */
bool penalty_ble_diag_enter(void);
esp_err_t penalty_ble_diag_start(void);
esp_err_t penalty_ble_diag_stop(void);
void penalty_ble_diag_exit(void);
void penalty_ble_diag_key(bsp_btn_t button, bsp_btn_ev_t event);
