#pragma once

#include "penalty_model.h"

#include <stdint.h>

penalty_language_t penalty_preferences_decode_language(uint8_t value);
uint8_t penalty_preferences_encode_language(penalty_language_t language);

#ifdef ESP_PLATFORM
#include "esp_err.h"

esp_err_t penalty_preferences_init(void);
penalty_language_t penalty_preferences_load_language(void);
esp_err_t penalty_preferences_save_language(penalty_language_t language);
#endif

