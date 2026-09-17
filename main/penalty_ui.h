#pragma once

#include "lvgl.h"
#include "penalty_model.h"

/* All calls require LVGL context/lock. This renderer has no hardware dependency
 * so the exact device UI can also be rendered by the host LVGL smoke test. */
bool penalty_ui_create(void);
void penalty_ui_update(const penalty_model_t *model, uint64_t now_ms, int battery,
                       bool sound_available, bool input_reset);
void penalty_ui_destroy(void);
