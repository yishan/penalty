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

static lv_obj_t *s_screen, *s_field, *s_meter;
static lv_obj_t *s_cover;
extern const uint16_t penalty_cover_pixels[240 * 320];
static const lv_image_dsc_t s_cover_image = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
               .w = 240, .h = 320, .stride = 240 * 2},
    .data_size = 240 * 320 * 2,
    .data = (const uint8_t *)penalty_cover_pixels,
};
static lv_obj_t *s_header, *s_battery, *s_heading, *s_status, *s_footer, *s_help;
static penalty_model_t s_view;
static penalty_scene_t s_scene;
static bool s_drawn;

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

static void art(lv_layer_t *l, const lv_image_dsc_t *image, int x, int y) {
    lv_draw_image_dsc_t d;
    lv_draw_image_dsc_init(&d);
    d.src = image;
    lv_area_t a = {x, y, x + image->header.w - 1, y + image->header.h - 1};
    /* Native-size const RGB565(A8) goes directly through the existing small buffer. */
    lv_draw_image(l, &d, &a);
}

static void draw_field(lv_event_t *e) {
    lv_layer_t *l = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(s_field, &a);
    int ox = a.x1, oy = a.y1;
    if (s_view.state == PENALTY_SUMMARY) {
        for (unsigned i = 0; i < PENALTY_SHOTS; ++i) {
            int x = ox + 43 + (int)i * 49;
            uint32_t c = penalty_is_goal(s_view.shots[i].outcome) ? LIME : RED;
            frame(l, x, oy + 63, 38, 36, c);
            if (penalty_is_goal(s_view.shots[i].outcome)) art(l, &penalty_art_balls[1], x + 11, oy + 73);
            else {
                rect(l, x + 9, oy + 79, 20, 4, c);
            }
        }
        return;
    }
    art(l, &penalty_art_background, 0, 0);
    bool title = s_view.state == PENALTY_TITLE;
    if (s_scene.target_visible) {
        for (unsigned target = 0; target < PENALTY_TARGET_COUNT; ++target) {
            int center_x = 110 + penalty_target_column((penalty_direction_t)target) * 50;
            int center_y = penalty_target_row((penalty_direction_t)target) ? 80 : 58;
            outline(l, center_x - 21, center_y - 10, 42, 20, DIM);
        }
        frame(l, s_scene.target_x - 21, s_scene.target_y - 10, 42, 20, LIME);
    }
    art(l, &penalty_art_keeper[s_scene.keeper.frame], s_scene.keeper.x, s_scene.keeper.y);
    if (title) return;
    if (s_scene.ball_visible) {
        const lv_image_dsc_t *ball = &penalty_art_balls[s_scene.ball_frame];
        int r = ball->header.w / 2;
        rect(l, s_scene.ball_x - r, s_scene.shadow_y, r * 2, 2, 0x123C2F);
        art(l, ball, s_scene.ball_x - r, s_scene.ball_y - r);
    }
    /* The receding ball can pass behind the foreground striker, never over him. */
    art(l, &penalty_art_striker[s_scene.striker.frame], s_scene.striker.x, s_scene.striker.y);
    if (!title) {
        for (unsigned i = 0; i < PENALTY_SHOTS; ++i) {
            int x = 32 + (int)i * 12;
            if (i < s_view.completed)
                rect(l, x, 165, 8, 8, penalty_is_goal(s_view.shots[i].outcome) ? LIME : RED);
            else frame(l, x, 165, 8, 8, i == s_view.completed ? WHITE : DIM);
        }
    }
}

static void draw_meter(lv_event_t *e) {
    lv_layer_t *l = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(s_meter, &a);
    int x = a.x1, y = a.y1;
    rect(l, x, y + 5, 256, 12, 0x426D55);
    /* Inclusive values occupy bins [p * 256/101, (p+1) * 256/101). */
    int low = s_view.current.green_low * 256 / 101;
    int end = (s_view.current.green_high + 1) * 256 / 101;
    rect(l, x + low, y + 5, end - low, 12, GREEN);
    int danger = 91 * 256 / 101;
    rect(l, x + danger, y + 5, 256 - danger, 12, RED);
    int line = s_view.current.perfect_low * 256 / 101;
    int line_end = (s_view.current.perfect_low + 2) * 256 / 101;
    rect(l, x + line, y + 5, line_end - line, 12, DARK_GREEN);
    rect(l, x + line, y + 4, line_end - line, 1, WHITE);
    rect(l, x + line, y + 17, line_end - line, 1, WHITE);
    int p = s_view.state == PENALTY_CHARGE ? s_view.current.power : 0;
    /* Bracket the selected bin without covering the tiny dark precision line. */
    rect(l, x + p * 256 / 101, y, 2, 4, WHITE);
    rect(l, x + p * 256 / 101, y + 18, 2, 4, WHITE);
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
    dirty_sprite(before.striker, after.striker, 88);
    dirty_sprite(before.keeper, after.keeper, 56);
    if (before.target_x != after.target_x || before.target_y != after.target_y ||
        before.target_visible != after.target_visible) {
        dirty(88, 46, 144, 47);
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

void penalty_ui_update(const penalty_model_t *m, uint64_t now, int battery,
                       bool audio, bool input_reset) {
    if (!s_screen) return;
    bool language_changed = !s_drawn || s_view.language != m->language;
    bool changed = !s_drawn || s_view.state != m->state || language_changed;
    bool redraw = changed || s_view.completed != m->completed ||
                  memcmp(s_view.shots, m->shots, sizeof(m->shots)) != 0;
    bool meter_changed = changed || s_view.current.power != m->current.power ||
                         s_view.current.green_low != m->current.green_low ||
                         s_view.current.green_high != m->current.green_high ||
                         s_view.current.perfect_low != m->current.perfect_low;
    s_view = *m;
    penalty_language_t language = (unsigned)m->language < PENALTY_LANGUAGE_COUNT
        ? m->language : PENALTY_LANGUAGE_EN;
    const lv_font_t *body_font = language == PENALTY_LANGUAGE_ZH_CN
        ? &penalty_font_zh_14 : &lv_font_montserrat_14;
    const lv_font_t *heading_font = language == PENALTY_LANGUAGE_ZH_CN
        ? &penalty_font_zh_20 : &lv_font_montserrat_20;
    lv_obj_t *localized[] = {s_header, s_heading, s_status, s_footer, s_help};
    for (unsigned i = 0; i < sizeof(localized) / sizeof(localized[0]); ++i)
        lv_obj_set_style_text_font(localized[i], body_font, 0);
    lv_obj_set_style_text_font(s_heading, heading_font, 0);
    penalty_scene_t scene = penalty_scene_at(m, now);
    if (!redraw) dirty_scene(s_scene, scene);
    s_scene = scene;
    bool portrait = m->state == PENALTY_ORIENT;
    if (changed) {
        int width = portrait ? 240 : 320;
        lv_obj_set_size(s_screen, width, portrait ? 320 : 240);
        position(s_header, 32, 1, 155);
        lv_obj_set_style_text_align(s_header, LV_TEXT_ALIGN_LEFT, 0);
        position(s_battery, width - 79, portrait ? 2 : 1, 47);
        lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(s_field, 0, m->state == PENALTY_SUMMARY ? 30 : 18);
        lv_obj_set_size(s_field, width, m->state == PENALTY_SUMMARY ? 132 : m->state == PENALTY_TITLE ? 144 : 165);
        position(s_heading, 32, portrait ? 50 : 42, width - 64);
        position(s_status, 32, portrait ? 211 : 184, width - 64);
        position(s_footer, 32, portrait ? 255 : 204, width - 64);
        position(s_help, 32, portrait ? 282 : 222, width - 64);
        lv_obj_set_pos(s_meter, 32, 182);
        lv_obj_set_size(s_meter, 256, 22);
        lv_obj_add_flag(s_heading, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_meter, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_status, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(s_status, LV_OPA_TRANSP, 0);
        lv_obj_set_height(s_status, LV_SIZE_CONTENT);
        lv_obj_t *game_objects[] = {s_field, s_header, s_status, s_footer, s_help};
        for (unsigned i = 0; i < sizeof(game_objects) / sizeof(game_objects[0]); ++i) {
            if (portrait) lv_obj_add_flag(game_objects[i], LV_OBJ_FLAG_HIDDEN);
            else lv_obj_remove_flag(game_objects[i], LV_OBJ_FLAG_HIDDEN);
        }
        if (portrait) lv_obj_add_flag(s_battery, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(s_battery, LV_OBJ_FLAG_HIDDEN);
        if (portrait) lv_obj_remove_flag(s_cover, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_cover, LV_OBJ_FLAG_HIDDEN);
    }
    char text[256];
    if (battery >= 0) snprintf(text, sizeof(text), "%d%%", battery);
    else snprintf(text, sizeof(text), "--%%");
    set_text(s_battery, text);
    set_text(s_help, penalty_text(language, PENALTY_TEXT_HOLD_COVER));
    if (m->state == PENALTY_ORIENT) {
        if (input_reset) {
            position(s_status, 32, language == PENALTY_LANGUAGE_ZH_CN ? 276 : 234, 176);
            lv_obj_set_height(s_status, language == PENALTY_LANGUAGE_ZH_CN ? 38 : LV_SIZE_CONTENT);
            lv_obj_set_style_bg_color(s_status, lv_color_hex(
                language == PENALTY_LANGUAGE_ZH_CN ? COVER_GREEN : INK), 0);
            lv_obj_set_style_bg_opa(s_status, LV_OPA_COVER, 0);
            set_text(s_status, penalty_text(language, PENALTY_TEXT_RETRY_PRESS_OK));
            lv_obj_remove_flag(s_status, LV_OBJ_FLAG_HIDDEN);
        } else if (language == PENALTY_LANGUAGE_ZH_CN) {
            position(s_status, 32, 276, 176);
            lv_obj_set_height(s_status, 38);
            lv_obj_set_style_bg_color(s_status, lv_color_hex(COVER_GREEN), 0);
            lv_obj_set_style_bg_opa(s_status, LV_OPA_COVER, 0);
            set_text(s_status, penalty_text(language, PENALTY_TEXT_COVER_PROMPT));
            lv_obj_remove_flag(s_status, LV_OBJ_FLAG_HIDDEN);
        } else lv_obj_add_flag(s_status, LV_OBJ_FLAG_HIDDEN);
    } else if (m->state == PENALTY_TITLE) {
        set_text(s_header, penalty_text(language, PENALTY_TEXT_HEADER_TITLE));
        /* The cover owns the game slogan; keep the landscape title screen visual-only. */
        set_text(s_heading, "");
        lv_obj_add_flag(s_heading, LV_OBJ_FLAG_HIDDEN);
        position(s_status, 32, 166, 256);
        snprintf(text, sizeof(text), "%s%s%s    %s%s%s\n%s%s%s",
                 m->selection == 0 ? "[" : "", penalty_text(language, PENALTY_TEXT_MENU_PLAY),
                 m->selection == 0 ? "]" : "",
                 m->selection == 1 ? "[" : "", penalty_text(language, PENALTY_TEXT_MENU_SETTINGS),
                 m->selection == 1 ? "]" : "", m->selection == 2 ? "[" : "",
                 penalty_text(language, PENALTY_TEXT_MENU_HELP), m->selection == 2 ? "]" : "");
        set_text(s_status, text);
        set_text(s_footer, penalty_text(language, PENALTY_TEXT_MENU_FOOTER));
    } else if (m->state == PENALTY_SETTINGS) {
        lv_obj_add_flag(s_field, LV_OBJ_FLAG_HIDDEN);
        set_text(s_header, penalty_text(language, PENALTY_TEXT_HEADER_SETTINGS));
        lv_obj_remove_flag(s_heading, LV_OBJ_FLAG_HIDDEN);
        set_text(s_heading, penalty_text(language, PENALTY_TEXT_SETTINGS_HEADING));
        position(s_status, 32, 78, 256);
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
                 m->selection == 0 ? "[" : "", difficulty, m->selection == 0 ? "]" : "",
                 m->selection == 1 ? "[" : "", sound_line, m->selection == 1 ? "]" : "",
                 m->selection == 2 ? "[" : "", language_line, m->selection == 2 ? "]" : "",
                 m->selection == 3 ? "[" : "", penalty_text(language, PENALTY_TEXT_SETTINGS_BACK),
                 m->selection == 3 ? "]" : "");
        set_text(s_status, text);
        set_text(s_footer, penalty_text(language, PENALTY_TEXT_SETTINGS_FOOTER));
    } else if (m->state == PENALTY_HELP) {
        lv_obj_add_flag(s_field, LV_OBJ_FLAG_HIDDEN);
        set_text(s_header, penalty_text(language, PENALTY_TEXT_HEADER_HELP));
        lv_obj_remove_flag(s_heading, LV_OBJ_FLAG_HIDDEN);
        set_text(s_heading, penalty_text(language, PENALTY_TEXT_HELP_HEADING));
        position(s_status, 32, 83, 256);
        set_text(s_status, penalty_text(language, PENALTY_TEXT_HELP_BODY));
        set_text(s_footer, penalty_text(language, PENALTY_TEXT_HELP_FOOTER));
    } else if (m->state == PENALTY_SUMMARY) {
        snprintf(text, sizeof(text), penalty_text(language, PENALTY_TEXT_SUMMARY_HEADER_FMT),
                 penalty_difficulty_text_localized(language, m->difficulty));
        set_text(s_header, text);
        lv_obj_remove_flag(s_heading, LV_OBJ_FLAG_HIDDEN);
        snprintf(text, sizeof(text), penalty_text(language, PENALTY_TEXT_SUMMARY_HEADING_FMT), m->goals);
        set_text(s_heading, text);
        snprintf(text, sizeof(text), penalty_text(language, PENALTY_TEXT_SUMMARY_STATUS_FMT), m->perfect,
                 penalty_text(language, m->goals == 5 ? PENALTY_TEXT_SUMMARY_FLAWLESS :
                     m->goals >= 3 ? PENALTY_TEXT_SUMMARY_WELL_PLAYED : PENALTY_TEXT_SUMMARY_KEEP_PRACTISING));
        set_text(s_status, text);
        snprintf(text, sizeof(text), m->selection == 0 ? "[%s]        %s" : "%s        [%s]",
                 penalty_text(language, PENALTY_TEXT_SUMMARY_RETRY),
                 penalty_text(language, PENALTY_TEXT_SUMMARY_TITLE));
        set_text(s_footer, text);
    } else {
        snprintf(text, sizeof(text), penalty_text(language, PENALTY_TEXT_GAME_HEADER_FMT),
                 penalty_difficulty_text_localized(language, m->difficulty),
                 m->completed + (m->state != PENALTY_RESULT), m->goals);
        set_text(s_header, text);
        if (m->state == PENALTY_AIM) {
            lv_obj_remove_flag(s_meter, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_status, LV_OBJ_FLAG_HIDDEN);
            snprintf(text, sizeof(text), penalty_text(language, PENALTY_TEXT_AIM_FMT),
                     penalty_direction_text(language, m->current.direction));
            set_text(s_footer, input_reset ? penalty_text(language, PENALTY_TEXT_INPUT_RESET) : text);
        } else if (m->state == PENALTY_CHARGE) {
            lv_obj_remove_flag(s_meter, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_status, LV_OBJ_FLAG_HIDDEN);
            snprintf(text, sizeof(text), penalty_text(language, PENALTY_TEXT_CHARGE_FMT), m->current.power);
            set_text(s_footer, text);
        } else if (m->state == PENALTY_FLIGHT) {
            set_text(s_status, penalty_text(language, PENALTY_TEXT_FLIGHT));
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
}
