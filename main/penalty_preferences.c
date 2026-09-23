#include "penalty_preferences.h"

penalty_language_t penalty_preferences_decode_language(uint8_t value) {
    return value < PENALTY_LANGUAGE_COUNT
        ? (penalty_language_t)value : PENALTY_DEFAULT_LANGUAGE;
}

uint8_t penalty_preferences_encode_language(penalty_language_t language) {
    return (unsigned)language < PENALTY_LANGUAGE_COUNT
        ? (uint8_t)language : (uint8_t)PENALTY_DEFAULT_LANGUAGE;
}

#ifdef ESP_PLATFORM
#include "nvs.h"
#include "nvs_flash.h"

static bool s_nvs_ready;

esp_err_t penalty_preferences_init(void) {
    if (s_nvs_ready) return ESP_OK;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_OK) s_nvs_ready = true;
    return err;
}

penalty_language_t penalty_preferences_load_language(void) {
    if (!s_nvs_ready) return PENALTY_DEFAULT_LANGUAGE;
    nvs_handle_t handle = 0;
    if (nvs_open("penalty", NVS_READONLY, &handle) != ESP_OK) return PENALTY_DEFAULT_LANGUAGE;
    uint8_t value = PENALTY_DEFAULT_LANGUAGE;
    esp_err_t err = nvs_get_u8(handle, "language", &value);
    nvs_close(handle);
    return err == ESP_OK ? penalty_preferences_decode_language(value) : PENALTY_DEFAULT_LANGUAGE;
}

esp_err_t penalty_preferences_save_language(penalty_language_t language) {
    if (!s_nvs_ready) return ESP_ERR_INVALID_STATE;
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open("penalty", NVS_READWRITE, &handle);
    if (err == ESP_OK)
        err = nvs_set_u8(handle, "language", penalty_preferences_encode_language(language));
    if (err == ESP_OK) err = nvs_commit(handle);
    if (handle) nvs_close(handle);
    return err;
}
#endif
