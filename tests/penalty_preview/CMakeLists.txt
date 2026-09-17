cmake_minimum_required(VERSION 3.16)
project(penalty_preview C)
set(CMAKE_C_STANDARD 11)
if(NOT EXISTS "${LVGL_DIR}/lvgl.h")
    message(FATAL_ERROR "Set LVGL_DIR to the pinned LVGL 9.5.0 source directory")
endif()
file(GLOB_RECURSE LVGL_SOURCES "${LVGL_DIR}/src/*.c")
add_library(lvgl_host STATIC ${LVGL_SOURCES})
target_include_directories(lvgl_host PUBLIC "${LVGL_DIR}")
target_compile_definitions(lvgl_host PUBLIC LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h")
add_executable(penalty_preview preview.c ../../main/penalty_ui.c ../../main/penalty_model.c
               ../../main/penalty_scene.c ../../main/penalty_copy.c ../../main/penalty_i18n.c
               ../../assets/fonts/penalty/penalty_font_zh_14.c
               ../../assets/fonts/penalty/penalty_font_zh_20.c
               ../../assets/images/penalty/gameplay-v2/gameplay_assets.c
               ../../assets/images/penalty/cover_rgb565.c
               ../../components/bsp/src/bsp_display_rounding.c)
target_include_directories(penalty_preview PRIVATE ../../main ../../components/bsp/src
                           ../../assets/fonts/penalty)
target_compile_options(penalty_preview PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(penalty_preview PRIVATE lvgl_host m)
