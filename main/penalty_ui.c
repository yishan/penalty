#include "penalty_ui.h"
#include "penalty_copy.h"
#include "penalty_i18n.h"
#include "penalty_scene.h"
#include "../assets/fonts/penalty/penalty_fonts.h"
#include "../assets/images/penalty/gameplay-v2/gameplay_assets.h"

#include <stdio.h>
#include <string.h>

#define INK 0x0B211F
#define WHITE 0xF2F0D9
#define DIM 0xA2BC9E
#define LIME 0xD4F45B
#define RED 0xED705B
#define GREEN 0x86CF70
#define DARK_GREEN 0x125132
#define COVER_GREEN 0x093A20
#define METER_WIDTH 192

static lv_obj_t *s_screen, *s_field, *s_meter;
static lv_obj_t *s_cover;
extern const uint16_t penalty_cover_pixels[240 * 320];
static const lv_image_dsc_t s_cover_image = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
               .w = 240, .h = 320, .stride = 240 * 2},
    .data_size = 240 * 320 * 2,
    .data = (const uint8_t *)penalty_cover_pixels,
};
/* A read-only view into the existing cover, excluding its final grass row.
 * Keep the source stride: the crop is not a separate packed image. */
static const lv_image_dsc_t s_cover_mark = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
               .w = 128, .h = 19, .stride = 240 * 2},
    .data_size = 19 * 240 * 2,
    .data = (const uint8_t *)(penalty_cover_pixels + 232 * 240 + 56),
};
static lv_obj_t *s_header, *s_battery, *s_heading, *s_status, *s_footer, *s_help;
static penalty_model_t s_view;
static penalty_scene_t s_scene;
static bool s_drawn;
static uint8_t s_coin_frame;

static bool shot_success(const penalty_model_t *model, const penalty_shot_t *shot) {
    return penalty_player_is_keeper(model)
        ? penalty_is_save(shot->outcome) : penalty_is_goal(shot->outcome);
}

static void rect(lv_layer_t *layer, int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(color);
    d.bg_opa = LV_OPA_COVER;
    d.radius = 0;
    lv_area_t a = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(layer, &d, &a);
}

static void frame(lv_layer_t *l, int x, int y, int w, int h, uint32_t c) {
    rect(l, x, y, w, 2, c); rect(l, x, y + h - 2, w, 2, c);
    rect(l, x, y, 2, h, c); rect(l, x + w - 2, y, 2, h, c);
}

static void outline(lv_layer_t *l, int x, int y, int w, int h, uint32_t c) {
    rect(l, x, y, w, 1, c); rect(l, x, y + h - 1, w, 1, c);
    rect(l, x, y, 1, h, c); rect(l, x + w - 1, y, 1, h, c);
}

static void scaled_art(lv_layer_t *l, const lv_image_dsc_t *image, int x, int y,
                       int scale, int pivot_x, int pivot_y) {
    lv_draw_image_dsc_t d;
    lv_draw_image_dsc_init(&d);
    d.src = image;
    d.scale_x = d.scale_y = scale;
    d.pivot = (lv_point_t){pivot_x, pivot_y};
    d.antialias = false;
    lv_area_t a = {x, y, x + image->header.w - 1, y + image->header.h - 1};
    /* Flash-backed RGB565(A8); LVGL transforms only bounded strips when scaled. */
    lv_draw_image(l, &d, &a);
}

static void art(lv_layer_t *l, const lv_image_dsc_t *image, int x, int y) {
    scaled_art(l, image, x, y, LV_SCALE_NONE, 0, 0);
}

static void sprite(lv_layer_t *l, const lv_image_dsc_t *image,
                   lv_point_t offset, int x, int y, int scale, int px, int py) {
    /* Keep the original canvas anchor, including the blue player's ball pivot. */
    scaled_art(l, image, x + offset.x, y + offset.y, scale,
               px - offset.x, py - offset.y);
}

static void draw_cover_mark(lv_event_t *e) {
    lv_layer_t *layer = lv_event_get_layer(e);
    /* Erase only the baked mark; retain each row's original footer gradient. */
    for (int y = 232; y < 251; ++y) {
        uint16_t p = penalty_cover_pixels[y * 240];
        uint32_t color = ((uint32_t)(p >> 11) << 19) |
                         ((uint32_t)((p >> 5) & 63) << 10) | ((p & 31) << 3);
        rect(layer, 56, y, 128, 1, color);
    }
    /* 307/256 = 119.9%; move the crop down 6 px, keep it horizontally centered.
     * Nearest-neighbor preserves the approved cream/shadow pixel lettering.
     * The bounded transform ends above the prompt at y=263; no full-screen RAM. */
    scaled_art(layer, &s_cover_mark, 43, 238, 307, 0, 0);
}

static void draw_coin(lv_layer_t *l, int cx, int cy, uint8_t coin_frame,
                      penalty_coin_face_t locked_face) {
    static const uint8_t widths[] = {52, 34, 12, 34, 52, 34, 12, 34};
    uint8_t frame_index = coin_frame % (sizeof(widths) / sizeof(widths[0]));
    int width = widths[frame_index], x = cx - width / 2, y = cy - 26;
    rect(l, x + 4, y, width - 8, 52, 0x7B4B16);
    rect(l, x, y + 4, width, 44, 0x7B4B16);
    rect(l, x + 4, y + 4, width - 8, 44, 0xF2C94C);
    rect(l, x + 8, y + 8, width - 16, 36, 0xD99A2B);
    if (width < 30) {
        rect(l, cx - 2, y + 6, 4, 40, 0xFFF1A3);
        return;
    }
    penalty_coin_face_t face = locked_face;
    if (face == PENALTY_COIN_UNSET)
        face = frame_index < 4 ? PENALTY_COIN_FOOTBALL : PENALTY_COIN_GLOVE;
    int icon_x = cx - 10, icon_y = cy - 10;
    if (face == PENALTY_COIN_GLOVE) {
        rect(l, icon_x + 3, icon_y + 8, 14, 11, INK);
        rect(l, icon_x + 1, icon_y + 3, 4, 11, INK);
        rect(l, icon_x + 6, icon_y, 3, 10, INK);
        rect(l, icon_x + 11, icon_y + 1, 3, 9, INK);
        rect(l, icon_x + 16, icon_y + 4, 3, 10, INK);
    } else {
        rect(l, icon_x + 5, icon_y + 2, 10, 16, WHITE);
        rect(l, icon_x + 2, icon_y + 5, 16, 10, WHITE);
        rect(l, icon_x + 7, icon_y + 6, 6, 6, INK);
        rect(l, icon_x + 3, icon_y + 4, 4, 4, INK);
        rect(l, icon_x + 13, icon_y + 12, 4, 4, INK);
    }
}

static void draw_score_mark(lv_layer_t *l, int x, int y, bool done, bool scored) {
    if (!done) frame(l, x, y, 8, 8, DIM);
    else rect(l, x, y, 8, 8, scored ? LIME : RED);
}

static void draw_match_marks(lv_layer_t *l, int ox, int oy) {
    const penalty_match_t *match = &s_view.match;
    if (match->sudden_death) {
        draw_score_mark(l, ox + 104, oy + 5, match->pair_player_done,
                        match->pair_player_scored);
        draw_score_mark(l, ox + 128, oy + 5, match->pair_ai_done,
                        match->pair_ai_scored);
        return;
    }
    for (unsigned i = 0; i < PENALTY_SHOTS; ++i) {
        int x = ox + 90 + (int)i * 12;
        draw_score_mark(l, x, oy + 4, i < match->player_attempts,
                        (match->player_regular_scored_bits & (1u << i)) != 0);
        draw_score_mark(l, x, oy + 16, i < match->ai_attempts,
                        (match->ai_regular_scored_bits & (1u << i)) != 0);
    }
}

static void draw_field(lv_event_t *e) {
    lv_layer_t *l = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(s_field, &a);
    int ox = a.x1, oy = a.y1;
    if (s_view.state == PENALTY_SUMMARY) {
        for (unsigned i = 0; i < PENALTY_SHOTS; ++i) {
            int x = ox + 16 + (int)i * 42;
            uint32_t c = shot_success(&s_view, &s_view.shots[i]) ? LIME : RED;
            frame(l, x, oy + 63, 32, 32, c);
            if (shot_success(&s_view, &s_view.shots[i]))
                sprite(l, &penalty_art_balls[1], penalty_art_balls_offsets[1],
                       x + 8, oy + 71, LV_SCALE_NONE, 0, 0);
            else {
                rect(l, x + 6, oy + 77, 20, 4, c);
            }
        }
        return;
    }
    art(l, &penalty_art_background, 0, 0);
    bool coin = s_view.state == PENALTY_COIN_READY ||
                s_view.state == PENALTY_COIN_SPIN ||
                s_view.state == PENALTY_COIN_RESULT;
    if (coin) {
        penalty_coin_face_t face = s_view.state == PENALTY_COIN_RESULT
            ? s_view.match.coin_face : PENALTY_COIN_UNSET;
        draw_coin(l, ox + 120, oy + 70, s_coin_frame, face);
        return;
    }
    bool title = s_view.state == PENALTY_TITLE;
    if (s_scene.target_visible) {
        for (unsigned target = 0; target < PENALTY_TARGET_COUNT; ++target) {
            int center_x = 70 + penalty_target_column((penalty_direction_t)target) * 50;
            int center_y = penalty_target_row((penalty_direction_t)target) ? 80 : 58;
            outline(l, center_x - 21, center_y - 10, 42, 20, DIM);
        }
        frame(l, s_scene.target_x - 21, s_scene.target_y - 10, 42, 20, LIME);
    }
    sprite(l, &penalty_art_keeper[s_scene.keeper.frame],
           penalty_art_keeper_offsets[s_scene.keeper.frame],
           s_scene.keeper.x, s_scene.keeper.y, LV_SCALE_NONE, 0, 0);
    bool presentation = title || (!s_view.multiplayer && s_view.state == PENALTY_HANDOFF) ||
                        s_view.state == PENALTY_MATCH_SUMMARY;
    if (presentation) {
        if (s_view.session_kind == PENALTY_SESSION_SHOOTOUT && !title)
            if (!s_view.multiplayer) draw_match_marks(l, ox, oy);
        return;
    }
    if (s_scene.ball_visible) {
        const lv_image_dsc_t *ball = &penalty_art_balls[s_scene.ball_frame];
        int r = ball->header.w / 2;
        rect(l, s_scene.ball_x - r, s_scene.shadow_y, r * 2, 2, 0x123C2F);
        art(l, ball, s_scene.ball_x - r, s_scene.ball_y - r);
    }
    /* The receding ball can pass behind the foreground striker, never over him. */
    const lv_image_dsc_t *striker = s_scene.blue_striker
        ? &penalty_art_blue_striker[s_scene.striker.frame]
        : &penalty_art_striker[s_scene.striker.frame];
    if (s_scene.blue_striker) {
        /* Blue ready art is 78 px tall versus red's 65 px. Use one 83.2%
         * scale for all nine poses, pivoting at the stationary ball (131,147).
         * The transformed canvas stays inside the existing 88x88 dirty region. */
        sprite(l, striker, penalty_art_blue_striker_offsets[s_scene.striker.frame],
               s_scene.striker.x, s_scene.striker.y, 213, 83, 52);
    } else {
        sprite(l, striker, penalty_art_striker_offsets[s_scene.striker.frame],
               s_scene.striker.x, s_scene.striker.y, LV_SCALE_NONE, 0, 0);
    }
    if (s_view.session_kind == PENALTY_SESSION_SHOOTOUT) {
        if (!s_view.multiplayer) draw_match_marks(l, ox, oy);
    } else if (!title) {
        for (unsigned i = 0; i < PENALTY_SHOTS; ++i) {
            int x = 16 + (int)i * 12;
            if (i < s_view.completed)
                rect(l, x, 165, 8, 8, shot_success(&s_view, &s_view.shots[i]) ? LIME : RED);
            else frame(l, x, 165, 8, 8, i == s_view.completed ? WHITE : DIM);
        }
    }
}

static void draw_meter(lv_event_t *e) {
    lv_layer_t *l = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(s_meter, &a);
    int x = a.x1, y = a.y1;
    rect(l, x, y + 5, METER_WIDTH, 12, 0x426D55);
    /* Inclusive values occupy bins [p * width/101, (p+1) * width/101). */
    int low = s_view.current.green_low * METER_WIDTH / 101;
    int end = (s_view.current.green_high + 1) * METER_WIDTH / 101;
    rect(l, x + low, y + 5, end - low, 12, GREEN);
    int danger = 91 * METER_WIDTH / 101;
    rect(l, x + danger, y + 5, METER_WIDTH - danger, 12, RED);
    int line = s_view.current.perfect_low * METER_WIDTH / 101;
    int line_end = (s_view.current.perfect_low + 2) * METER_WIDTH / 101;
    rect(l, x + line, y + 5, line_end - line, 12, DARK_GREEN);
    rect(l, x + line, y + 4, line_end - line, 1, WHITE);
    rect(l, x + line, y + 17, line_end - line, 1, WHITE);
    int p = s_view.state == PENALTY_CHARGE ? s_view.current.power : 0;
    /* Bracket the selected bin without covering the tiny dark precision line. */
    rect(l, x + p * METER_WIDTH / 101, y, 2, 4, WHITE);
    rect(l, x + p * METER_WIDTH / 101, y + 18, 2, 4, WHITE);
}

static lv_obj_t *label(lv_obj_t *parent) {
    lv_obj_t *o = lv_label_create(parent);
    if (!o) return NULL;
    lv_obj_set_style_text_font(o, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(WHITE), 0);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(o, LV_LABEL_LONG_CLIP);
    return o;
}

static lv_obj_t *surface(lv_obj_t *parent, lv_event_cb_t cb) {
    lv_obj_t *o = lv_obj_create(parent);
    if (!o) return NULL;
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(o, cb, LV_EVENT_DRAW_MAIN, NULL);
    return o;
}

bool penalty_ui_create(void) {
    if (s_screen) return true;
    /* Leave drawing headroom in the small static LVGL pool. */
    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    if (memory.free_size < 9000 || memory.free_biggest_size < 2048) return false;
    s_screen = lv_obj_create(NULL);
    if (!s_screen) return false;
    lv_obj_remove_style_all(s_screen);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(INK), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    s_cover = lv_image_create(s_screen);
    if (!s_cover) { penalty_ui_destroy(); return false; }
    /* Native RGB565 is read directly from const Flash, not decoded into RAM. */
    lv_image_set_src(s_cover, &s_cover_image);
    lv_obj_set_pos(s_cover, 0, 0);
    lv_obj_add_event_cb(s_cover, draw_cover_mark, LV_EVENT_DRAW_POST, NULL);
    s_field = surface(s_screen, draw_field);
    s_meter = surface(s_screen, draw_meter);
    s_header = label(s_screen); s_battery = label(s_screen);
    s_heading = label(s_screen); s_status = label(s_screen);
    s_footer = label(s_screen); s_help = label(s_screen);
    if (!s_field || !s_meter || !s_header || !s_battery || !s_heading || !s_status || !s_footer || !s_help) {
        penalty_ui_destroy();
        return false;
    }
    lv_obj_set_style_text_font(s_heading, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_heading, lv_color_hex(LIME), 0);
    lv_obj_set_style_text_color(s_help, lv_color_hex(DIM), 0);
    lv_obj_set_style_bg_color(s_battery, lv_color_hex(INK), 0);
    lv_obj_set_style_bg_opa(s_battery, LV_OPA_COVER, 0);
    s_drawn = false;
    lv_screen_load(s_screen);
    return true;
}

static void set_text(lv_obj_t *obj, const char *text) {
    if (strcmp(lv_label_get_text(obj), text) != 0) lv_label_set_text(obj, text);
}

static void position(lv_obj_t *o, int x, int y, int w) {
    lv_obj_set_pos(o, x, y);
    lv_obj_set_width(o, w);
}

static void dirty(int x, int y, int width, int height) {
    lv_area_t area = {x, y, x + width - 1, y + height - 1};
    lv_obj_invalidate_area(s_field, &area);
}

static void dirty_sprite(penalty_sprite_t before, penalty_sprite_t after, int size) {
    if (before.x == after.x && before.y == after.y && before.frame == after.frame) return;
    dirty(before.x, before.y, size, size);
    dirty(after.x, after.y, size, size);
}

static void dirty_scene(penalty_scene_t before, penalty_scene_t after) {
    if (before.blue_striker != after.blue_striker) {
        dirty(before.striker.x, before.striker.y, 88, 88);
        dirty(after.striker.x, after.striker.y, 88, 88);
    }
    dirty_sprite(before.striker, after.striker, 88);
    dirty_sprite(before.keeper, after.keeper, 56);
    if (before.target_x != after.target_x || before.target_y != after.target_y ||
        before.target_visible != after.target_visible) {
        dirty(48, 46, 144, 47);
    }
    if (before.ball_x != after.ball_x || before.ball_y != after.ball_y ||
        before.ball_frame != after.ball_frame || before.ball_visible != after.ball_visible) {
        dirty(before.ball_x - 9, before.ball_y - 9, 18, 18);
        dirty(after.ball_x - 9, after.ball_y - 9, 18, 18);
    }
    if (before.shadow_y != after.shadow_y || before.ball_x != after.ball_x || before.ball_frame != after.ball_frame) {
        dirty(before.ball_x - 9, before.shadow_y, 18, 2);
        dirty(after.ball_x - 9, after.shadow_y, 18, 2);
    }
}

static void format_match_header(char *text, size_t size,
                                penalty_language_t language,
                                const penalty_model_t *model) {
    const penalty_match_t *match = &model->match;
    snprintf(text, size, penalty_text(language, match->sudden_death
             ? PENALTY_TEXT_MATCH_SD_HEADER_FMT : PENALTY_TEXT_MATCH_REG_HEADER_FMT),
             penalty_shootout_round(model), match->player_goals, match->ai_goals);
}

void penalty_ui_update(const penalty_model_t *m, uint64_t now, int battery,
                       bool audio, bool input_reset) {
    if (!s_screen) return;
    uint8_t coin_frame = penalty_coin_frame_at(m, now);
    bool coin_changed = s_drawn && s_view.state == PENALTY_COIN_SPIN &&
                        s_coin_frame != coin_frame;
    bool language_changed = !s_drawn || s_view.language != m->language;
    bool changed = !s_drawn || s_view.state != m->state || language_changed;
    bool redraw = changed || s_view.completed != m->completed ||
                  coin_changed || memcmp(&s_view.match, &m->match, sizeof(m->match)) != 0 ||
                  memcmp(s_view.shots, m->shots, sizeof(m->shots)) != 0;
    bool meter_changed = changed || s_view.current.power != m->current.power ||
                         s_view.current.green_low != m->current.green_low ||
                         s_view.current.green_high != m->current.green_high ||
                         s_view.current.perfect_low != m->current.perfect_low;
    s_view = *m;
    s_coin_frame = coin_frame;
    penalty_language_t language = (unsigned)m->language < PENALTY_LANGUAGE_COUNT
        ? m->language : PENALTY_DEFAULT_LANGUAGE;
    const lv_font_t *body_font = language == PENALTY_LANGUAGE_ZH_CN
        ? &penalty_font_zh_14 : &lv_font_montserrat_14;
    const lv_font_t *heading_font = language == PENALTY_LANGUAGE_ZH_CN
        ? &penalty_font_zh_20 : &lv_font_montserrat_20;
    lv_obj_t *localized[] = {s_header, s_heading, s_status, s_footer, s_help};
    for (unsigned i = 0; i < sizeof(localized) / sizeof(localized[0]); ++i)
        lv_obj_set_style_text_font(localized[i], body_font, 0);
    lv_obj_set_style_text_font(s_heading, heading_font, 0);
    /* Cover styling belongs only to the prompt; reset before every other view. */
    lv_obj_set_style_text_color(s_status, lv_color_hex(WHITE), 0);
    lv_obj_set_style_pad_top(s_status, 0, 0);
    lv_obj_set_style_bg_opa(s_status, LV_OPA_TRANSP, 0);
    /* Menu/help spacing is local; never carry it into gameplay,
     * cover, or multiplayer status panels. */
    int row_pitch = m->state == PENALTY_SETTINGS ? 40 :
                    m->state == PENALTY_TITLE || m->state == PENALTY_HELP ? 32 : 0;
    lv_obj_set_style_text_line_space(s_status,
        row_pitch ? row_pitch - body_font->line_height : 0, 0);
    penalty_scene_t scene = penalty_scene_at(m, now);
    if (!redraw) dirty_scene(s_scene, scene);
    s_scene = scene;
    bool cover = m->state == PENALTY_COVER;
    if (changed) {
        const int width = 240;
        lv_obj_set_size(s_screen, width, 320);
        position(s_header, 16, 1, 160);
        lv_obj_set_style_text_align(s_header, LV_TEXT_ALIGN_LEFT, 0);
        position(s_battery, 177, 1, 47);
        lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
        bool summary = m->state == PENALTY_SUMMARY || m->state == PENALTY_MATCH_SUMMARY;
        lv_obj_set_pos(s_field, 0, summary ? 30 : 18);
        lv_obj_set_size(s_field, width, summary ? 132 : m->state == PENALTY_TITLE ? 90 : 165);
        position(s_heading, 16, 190, 208);
        position(s_status, 16, 211, 208);
        position(s_footer, 16, 248, 208);
        position(s_help, 16, 292, 208);
        lv_obj_set_pos(s_meter, 24, 205);
        lv_obj_set_size(s_meter, METER_WIDTH, 22);
        lv_obj_add_flag(s_heading, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_meter, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_status, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(s_status, LV_OPA_TRANSP, 0);
        lv_obj_set_height(s_status, LV_SIZE_CONTENT);
        lv_obj_t *game_objects[] = {s_field, s_header, s_status, s_footer, s_help};
        for (unsigned i = 0; i < sizeof(game_objects) / sizeof(game_objects[0]); ++i) {
            if (cover) lv_obj_add_flag(game_objects[i], LV_OBJ_FLAG_HIDDEN);
            else lv_obj_remove_flag(game_objects[i], LV_OBJ_FLAG_HIDDEN);
        }
        if (cover) lv_obj_add_flag(s_battery, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(s_battery, LV_OBJ_FLAG_HIDDEN);
        if (cover) lv_obj_remove_flag(s_cover, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_cover, LV_OBJ_FLAG_HIDDEN);
    }
    char text[256];
    if (battery >= 0) snprintf(text, sizeof(text), "%d%%", battery);
    else snprintf(text, sizeof(text), "--%%");
    set_text(s_battery, text);
    set_text(s_help, penalty_text(language, PENALTY_TEXT_HOLD_COVER));
    if (m->state == PENALTY_COVER) {
        position(s_status, 40, 263, 159);
        lv_obj_set_height(s_status, 33);
        /* Use one font and one geometry for both languages so their optical
         * centres cannot drift. The same-color label background masks the
         * artwork's old prompt border without changing any source pixels. */
        lv_obj_set_style_text_font(s_status, &penalty_font_zh_14, 0);
        lv_obj_set_style_pad_top(s_status, 8, 0);
        lv_obj_set_style_bg_color(s_status, lv_color_hex(COVER_GREEN), 0);
        lv_obj_set_style_bg_opa(s_status, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(s_status, lv_color_hex(DIM), 0);
        set_text(s_status, penalty_text(language, input_reset
            ? PENALTY_TEXT_RETRY_PRESS_OK : PENALTY_TEXT_COVER_PROMPT));
        lv_obj_remove_flag(s_status, LV_OBJ_FLAG_HIDDEN);
    } else if (m->state == PENALTY_TITLE) {
        set_text(s_header, "");
        lv_obj_add_flag(s_header, LV_OBJ_FLAG_HIDDEN);
        /* The cover owns the game slogan; keep the title screen visual-only. */
        set_text(s_heading, "");
        lv_obj_add_flag(s_heading, LV_OBJ_FLAG_HIDDEN);
        position(s_status, 16, 116, 208);
        snprintf(text, sizeof(text), "%s%s%s\n%s%s%s\n%s%s%s\n%s%s%s\n%s%s%s",
                 m->selection == 0 ? PENALTY_MENU_SELECTED_PREFIX : "", penalty_text(language, PENALTY_TEXT_MENU_SHOOTOUT),
                 m->selection == 0 ? PENALTY_MENU_SELECTED_SUFFIX : "",
                 m->selection == 1 ? PENALTY_MENU_SELECTED_PREFIX : "", penalty_text(language, PENALTY_TEXT_MENU_SHOOT),
                 m->selection == 1 ? PENALTY_MENU_SELECTED_SUFFIX : "",
                 m->selection == 2 ? PENALTY_MENU_SELECTED_PREFIX : "", penalty_text(language, PENALTY_TEXT_MENU_KEEP),
                 m->selection == 2 ? PENALTY_MENU_SELECTED_SUFFIX : "",
                 m->selection == 3 ? PENALTY_MENU_SELECTED_PREFIX : "", penalty_text(language, PENALTY_TEXT_MENU_SETTINGS),
                 m->selection == 3 ? PENALTY_MENU_SELECTED_SUFFIX : "", m->selection == 4 ? PENALTY_MENU_SELECTED_PREFIX : "",
                 penalty_text(language, PENALTY_TEXT_MENU_HELP), m->selection == 4 ? PENALTY_MENU_SELECTED_SUFFIX : "");
        set_text(s_status, text);
        set_text(s_footer, "");
        lv_obj_add_flag(s_footer, LV_OBJ_FLAG_HIDDEN);
    } else if (m->state == PENALTY_SETTINGS) {
        lv_obj_add_flag(s_field, LV_OBJ_FLAG_HIDDEN);
        set_text(s_header, penalty_text(language, PENALTY_TEXT_HEADER_SETTINGS));
        lv_obj_remove_flag(s_heading, LV_OBJ_FLAG_HIDDEN);
        set_text(s_heading, penalty_text(language, PENALTY_TEXT_SETTINGS_HEADING));
        position(s_heading, 16, 48, 208);
        position(s_status, 16, 84, 208);
        const char *sound = penalty_text(language, !audio ? PENALTY_TEXT_SOUND_UNAVAILABLE :
            m->muted ? PENALTY_TEXT_SOUND_MUTED : PENALTY_TEXT_SOUND_ON);
        char difficulty[48], sound_line[48], language_line[48];
        snprintf(difficulty, sizeof(difficulty),
                 penalty_text(language, PENALTY_TEXT_SETTINGS_DIFFICULTY_FMT),
                 penalty_difficulty_text_localized(language, m->difficulty));
        snprintf(sound_line, sizeof(sound_line),
                 penalty_text(language, PENALTY_TEXT_SETTINGS_SOUND_FMT), sound);
        snprintf(language_line, sizeof(language_line),
                 penalty_text(language, PENALTY_TEXT_SETTINGS_LANGUAGE_FMT),
                 penalty_text(language, PENALTY_TEXT_LANGUAGE_NAME));
        snprintf(text, sizeof(text), "%s%s%s\n%s%s%s\n%s%s%s\n%s%s%s",
                 m->selection == 0 ? PENALTY_MENU_SELECTED_PREFIX : "", difficulty, m->selection == 0 ? PENALTY_MENU_SELECTED_SUFFIX : "",
                 m->selection == 1 ? PENALTY_MENU_SELECTED_PREFIX : "", sound_line, m->selection == 1 ? PENALTY_MENU_SELECTED_SUFFIX : "",
                 m->selection == 2 ? PENALTY_MENU_SELECTED_PREFIX : "", language_line, m->selection == 2 ? PENALTY_MENU_SELECTED_SUFFIX : "",
                 m->selection == 3 ? PENALTY_MENU_SELECTED_PREFIX : "", penalty_text(language, PENALTY_TEXT_SETTINGS_BACK),
                 m->selection == 3 ? PENALTY_MENU_SELECTED_SUFFIX : "");
        set_text(s_status, text);
        position(s_footer, 16, 246, 208);
        set_text(s_footer, penalty_text(language, PENALTY_TEXT_SETTINGS_FOOTER));
    } else if (m->state == PENALTY_HELP) {
        lv_obj_add_flag(s_field, LV_OBJ_FLAG_HIDDEN);
        set_text(s_header, penalty_text(language, PENALTY_TEXT_HEADER_HELP));
        lv_obj_remove_flag(s_heading, LV_OBJ_FLAG_HIDDEN);
        set_text(s_heading, penalty_text(language, PENALTY_TEXT_HELP_HEADING));
        position(s_heading, 16, 48, 208);
        position(s_status, 16, 84, 208);
        set_text(s_status, penalty_text(language, PENALTY_TEXT_HELP_BODY));
        position(s_footer, 16, 260, 208);
        set_text(s_footer, penalty_text(language, PENALTY_TEXT_HELP_FOOTER));
    } else if (m->state == PENALTY_COIN_READY || m->state == PENALTY_COIN_SPIN ||
               m->state == PENALTY_COIN_RESULT) {
        set_text(s_header, penalty_text(language, PENALTY_TEXT_COIN_HEADER));
        lv_obj_remove_flag(s_heading, LV_OBJ_FLAG_HIDDEN);
        set_text(s_heading, penalty_text(language, PENALTY_TEXT_COIN_HEADING));
        position(s_heading, 16, 190, 208);
        position(s_status, 16, 220, 208);
        penalty_text_id_t status = m->state == PENALTY_COIN_READY
            ? PENALTY_TEXT_COIN_READY : m->state == PENALTY_COIN_SPIN
            ? PENALTY_TEXT_COIN_SPIN : m->match.coin_face == PENALTY_COIN_FOOTBALL
            ? PENALTY_TEXT_COIN_FOOTBALL : PENALTY_TEXT_COIN_GLOVE;
        set_text(s_status, penalty_text(language, status));
        set_text(s_footer, m->state == PENALTY_COIN_RESULT
            ? penalty_text(language, PENALTY_TEXT_HANDOFF_FOOTER) : "");
        position(s_footer, 16, 258, 208);
    } else if (m->state == PENALTY_HANDOFF) {
        format_match_header(text, sizeof(text), language, m);
        set_text(s_header, text);
        lv_obj_remove_flag(s_heading, LV_OBJ_FLAG_HIDDEN);
        set_text(s_heading, penalty_text(language, penalty_player_is_keeper(m)
                 ? PENALTY_TEXT_HANDOFF_KEEP : PENALTY_TEXT_HANDOFF_SHOOT));
        position(s_heading, 16, 190, 208);
        set_text(s_status, penalty_difficulty_text_localized(language, m->difficulty));
        position(s_status, 16, 222, 208);
        set_text(s_footer, penalty_text(language, PENALTY_TEXT_HANDOFF_FOOTER));
        position(s_footer, 16, 258, 208);
    } else if (m->state == PENALTY_SUMMARY) {
        snprintf(text, sizeof(text), penalty_text(language, PENALTY_TEXT_SUMMARY_HEADER_FMT),
                 penalty_difficulty_text_localized(language, m->difficulty));
        set_text(s_header, text);
        lv_obj_remove_flag(s_heading, LV_OBJ_FLAG_HIDDEN);
        snprintf(text, sizeof(text), penalty_text(language,
                 penalty_player_is_keeper(m) ? PENALTY_TEXT_SUMMARY_SAVES_FMT
                                                : PENALTY_TEXT_SUMMARY_HEADING_FMT),
                 penalty_player_is_keeper(m) ? m->saves : m->goals);
        set_text(s_heading, text);
        position(s_heading, 16, 177, 208);
        position(s_status, 16, 218, 208);
        snprintf(text, sizeof(text), penalty_text(language, PENALTY_TEXT_SUMMARY_STATUS_FMT), m->perfect,
                 penalty_text(language,
                     (penalty_player_is_keeper(m) ? m->saves : m->goals) == 5
                         ? PENALTY_TEXT_SUMMARY_FLAWLESS :
                     (penalty_player_is_keeper(m) ? m->saves : m->goals) >= 3
                         ? PENALTY_TEXT_SUMMARY_WELL_PLAYED : PENALTY_TEXT_SUMMARY_KEEP_PRACTISING));
        set_text(s_status, text);
        snprintf(text, sizeof(text), m->selection == 0 ? ">> %s <<    %s" : "%s    >> %s <<",
                 penalty_text(language, PENALTY_TEXT_SUMMARY_RETRY),
                 penalty_text(language, PENALTY_TEXT_SUMMARY_TITLE));
        set_text(s_footer, text);
        position(s_footer, 16, 262, 208);
    } else if (m->state == PENALTY_MATCH_SUMMARY) {
        set_text(s_header, penalty_text(language, PENALTY_TEXT_COIN_HEADER));
        lv_obj_remove_flag(s_heading, LV_OBJ_FLAG_HIDDEN);
        set_text(s_heading, penalty_text(language,
                 m->match.result == PENALTY_MATCH_PLAYER_WIN
                     ? PENALTY_TEXT_MATCH_WIN : PENALTY_TEXT_MATCH_LOSE));
        position(s_heading, 16, 172, 208);
        char score[64], reason[64], stats[64];
        snprintf(score, sizeof(score), penalty_text(language, PENALTY_TEXT_MATCH_SCORE_FMT),
                 m->match.player_goals, m->match.ai_goals);
        if (m->match.sudden_death)
            snprintf(reason, sizeof(reason), penalty_text(language,
                     PENALTY_TEXT_MATCH_SD_RESULT_FMT), m->match.sudden_death_round);
        else snprintf(reason, sizeof(reason), "%s",
                      penalty_text(language, PENALTY_TEXT_MATCH_EARLY));
        snprintf(stats, sizeof(stats), penalty_text(language, PENALTY_TEXT_MATCH_STATS_FMT),
                 m->match.player_goals, m->match.player_saves);
        snprintf(text, sizeof(text), "%s\n%s\n%s", score, reason, stats);
        set_text(s_status, text);
        position(s_status, 16, 207, 208);
        snprintf(text, sizeof(text), m->selection == 0 ? ">> %s <<    %s" : "%s    >> %s <<",
                 penalty_text(language, PENALTY_TEXT_SUMMARY_RETRY),
                 penalty_text(language, PENALTY_TEXT_SUMMARY_TITLE));
        set_text(s_footer, text);
        position(s_footer, 16, 269, 208);
    } else {
        if (m->session_kind == PENALTY_SESSION_SHOOTOUT)
            format_match_header(text, sizeof(text), language, m);
        else snprintf(text, sizeof(text), penalty_text(language,
                 penalty_player_is_keeper(m) ? PENALTY_TEXT_GAME_HEADER_KEEPER_FMT
                                             : PENALTY_TEXT_GAME_HEADER_FMT),
                 penalty_difficulty_text_localized(language, m->difficulty),
                 m->completed + (m->state != PENALTY_RESULT),
                 penalty_player_is_keeper(m) ? m->saves : m->goals);
        set_text(s_header, text);
        if (m->state == PENALTY_AIM) {
            lv_obj_remove_flag(s_meter, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_status, LV_OBJ_FLAG_HIDDEN);
            snprintf(text, sizeof(text), penalty_text(language,
                     penalty_player_is_keeper(m) ? PENALTY_TEXT_AIM_KEEPER_FMT
                                                    : PENALTY_TEXT_AIM_FMT),
                     penalty_direction_text(language, m->current.direction));
            set_text(s_footer, input_reset ? penalty_text(language, PENALTY_TEXT_INPUT_RESET) : text);
            position(s_footer, 16, 235, 208);
        } else if (m->state == PENALTY_CHARGE) {
            lv_obj_remove_flag(s_meter, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_status, LV_OBJ_FLAG_HIDDEN);
            snprintf(text, sizeof(text), penalty_text(language,
                     penalty_player_is_keeper(m) ? PENALTY_TEXT_CHARGE_KEEPER_FMT
                                                    : PENALTY_TEXT_CHARGE_FMT), m->current.power);
            set_text(s_footer, text);
            position(s_footer, 16, 235, 208);
        } else if (m->state == PENALTY_FLIGHT) {
            set_text(s_status, penalty_text(language,
                     penalty_player_is_keeper(m) ? PENALTY_TEXT_FLIGHT_KEEPER
                                                    : PENALTY_TEXT_FLIGHT));
            set_text(s_footer, "");
        } else if (m->state == PENALTY_RESULT) {
            set_text(s_status, penalty_outcome_text_localized(language, m->current.outcome));
            set_text(s_footer, penalty_result_copy(m->current.outcome, language));
        } else {
            set_text(s_status, penalty_text(language, PENALTY_TEXT_RETURNING)); set_text(s_footer, "");
        }
        if (m->state == PENALTY_AIM || m->state == PENALTY_CHARGE) {
            set_text(s_help, penalty_pre_shot_copy(m));
        }
    }
    if (redraw) lv_obj_invalidate(s_field);
    if (meter_changed) lv_obj_invalidate(s_meter);
    s_drawn = true;
}

void penalty_ui_destroy(void) {
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = s_field = s_meter = NULL;
    s_cover = NULL;
    s_header = s_battery = s_heading = s_status = s_footer = s_help = NULL;
    s_drawn = false;
    s_coin_frame = 0;
}

/* Reuse the Penalty pitch artwork and fonts, with independent multiplayer copy. */
void penalty_ui_panel(const penalty_model_t *view, uint64_t now, int battery,
                      bool audio, const char *header, const char *heading,
                      const char *detail, const char *footer, bool field) {
    penalty_ui_update(view, now, battery, audio, false);
    if (!s_screen) return;
    set_text(s_header, header);
    lv_obj_remove_flag(s_header, LV_OBJ_FLAG_HIDDEN);
    set_text(s_heading, heading);
    set_text(s_status, detail);
    lv_obj_set_style_text_line_space(s_status, 0, 0);
    set_text(s_footer, footer);
    lv_obj_remove_flag(s_footer, LV_OBJ_FLAG_HIDDEN);
    set_text(s_help, penalty_text(view->language,
        field && (view->state == PENALTY_AIM || view->state == PENALTY_CHARGE)
            ? PENALTY_TEXT_MP_AIM_HINT : PENALTY_TEXT_HOLD_COVER));
    position(s_help, 16, 278, 208);
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_WRAP);
    lv_label_set_long_mode(s_footer, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(s_status, LV_SIZE_CONTENT);
    lv_obj_set_height(s_footer, LV_SIZE_CONTENT);
    if (!field) {
        lv_obj_add_flag(s_field, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_meter, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_heading, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_status, LV_OBJ_FLAG_HIDDEN);
        position(s_heading, 16, 45, 208);
        position(s_status, 16, 100, 208);
        position(s_footer, 16, 250, 208);
    } else {
        lv_obj_remove_flag(s_field, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_heading, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_status, LV_OBJ_FLAG_HIDDEN);
        position(s_status, 16, view->state == PENALTY_AIM || view->state == PENALTY_CHARGE
                 ? 186 : 210, 208);
        position(s_footer, 16, 240, 208);
    }
}

void penalty_ui_menu_panel(const penalty_model_t *view, uint64_t now, int battery,
                          bool audio, const char *header, const char *heading,
                          const char *detail, const char *footer) {
    penalty_ui_panel(view, now, battery, audio, header, heading, detail, footer, false);
    if (!s_screen) return;
    const lv_font_t *font = lv_obj_get_style_text_font(s_status, 0);
    lv_obj_set_style_text_line_space(s_status, 40 - font->line_height, 0);
    position(s_status, 16, 84, 208);
}
