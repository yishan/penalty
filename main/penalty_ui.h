#pragma once

#include "lvgl.h"
#include "penalty_model.h"

#define PENALTY_MENU_SELECTED_PREFIX ">> "
#define PENALTY_MENU_SELECTED_SUFFIX " <<"

/* All calls require LVGL context/lock. This renderer has no hardware dependency
 * so the exact device UI can also be rendered by the host LVGL smoke test. */
bool penalty_ui_create(void);
void penalty_ui_update(const penalty_model_t *model, uint64_t now_ms, int battery,
                       bool sound_available, bool input_reset);
void penalty_ui_destroy(void);
void penalty_ui_panel(const penalty_model_t *view, uint64_t now, int battery,
                      bool audio, const char *header, const char *heading,
                      const char *detail, const char *footer, bool field);
/* Four selectable rows, 40-pixel pitch; not for connection/status copy. */
void penalty_ui_menu_panel(const penalty_model_t *view, uint64_t now, int battery,
                          bool audio, const char *header, const char *heading,
                          const char *detail, const char *footer);
