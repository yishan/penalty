idf_component_register(
    SRCS "main.c"
         "demo_navigation.c"
         "../assets/images/penalty/cover_rgb565.c"
         "penalty_model.c"
         "penalty_audio.c"
         "penalty_copy.c"
         "penalty_i18n.c"
         "penalty_preferences.c"
         "penalty_ui.c"
         "penalty_scene.c"
         "../assets/images/penalty/gameplay-v2/gameplay_assets.c"
         "../assets/fonts/penalty/penalty_font_zh_14.c"
         "../assets/fonts/penalty/penalty_font_zh_20.c"
         "../assets/music/penalty/penalty_sfx_pcm.c"
         "penalty_app.c"
    INCLUDE_DIRS "." "../assets/fonts/penalty"
    REQUIRES bsp esp_hw_support esp_timer launcher_contract nvs_flash
)
