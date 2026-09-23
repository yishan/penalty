#pragma once

#include "penalty_model.h"

typedef struct { int16_t x, y; uint8_t frame; } penalty_sprite_t;
typedef struct {
    penalty_sprite_t striker, keeper;
    int16_t ball_x, ball_y, shadow_y, target_x, target_y;
    uint8_t ball_frame;
    bool ball_visible, target_visible, blue_striker;
} penalty_scene_t;

/* Native background coordinates, independent of LVGL and the device clock.
 * The first 100 ms of the existing 600 ms flight budget are kick/contact. */
penalty_scene_t penalty_scene_at(const penalty_model_t *model, uint64_t now_ms);
