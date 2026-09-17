#include "penalty_ui.h"
#include "penalty_scene.h"
#include "../../assets/images/penalty/gameplay-v2/gameplay_assets.h"
#include "bsp_display_rounding.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t s_pixels[320 * 320];
static uint16_t s_partial[320 * 320];
static uint16_t s_buffer[240 * 20];
static size_t s_flushed_pixels;
static size_t s_max_motion_pixels;
static lv_display_t *s_display;
extern const uint16_t penalty_cover_pixels[240 * 320];

static uint16_t rgb565(unsigned color) {
    return ((color >> 8) & 0xf800) | ((color >> 5) & 0x07e0) | ((color >> 3) & 0x001f);
}

static void check_meter(const penalty_model_t *m) {
    if (m->state != PENALTY_AIM && m->state != PENALTY_CHARGE) return;
    /* Every pixel uses the same 101-value bin mapping as the cursor. */
    for (unsigned x = 0; x < 256; ++x) {
        unsigned p = 0;
        while (p < 100 && (p + 1) * 256 / 101 <= x) ++p;
        unsigned color = p > 90 ? 0xED705B : 0x426D55;
        if (p >= m->current.green_low && p <= m->current.green_high) color = 0x86CF70;
        if (p >= m->current.perfect_low && p <= m->current.perfect_low + 1) color = 0x125132;
        assert(s_pixels[193 * 320 + 32 + x] == rgb565(color));
    }
}

static void check_target_grid(const penalty_model_t *m) {
    if (m->state != PENALTY_AIM && m->state != PENALTY_CHARGE) return;
    for (unsigned target = 0; target < PENALTY_TARGET_COUNT; ++target) {
        int center_x = 110 + penalty_target_column((penalty_direction_t)target) * 50;
        int center_y = penalty_target_row((penalty_direction_t)target) ? 80 : 58;
        unsigned expected = target == (unsigned)m->current.direction ? 0xD4F45B : 0xA2BC9E;
        assert(s_pixels[(center_y - 10) * 320 + center_x - 21] == rgb565(expected));
    }
}

static void check_foreground(const penalty_model_t *m, uint64_t now) {
    if (m->state < PENALTY_AIM || m->state > PENALTY_RESULT) return;
    penalty_scene_t s = penalty_scene_at(m, now);
    const uint8_t *data = penalty_art_striker[s.striker.frame].data;
    for (unsigned y = 0; y < 88; ++y) {
        for (unsigned x = 0; x < 88; ++x) {
            unsigned i = y * 88 + x;
            if (data[88 * 88 * 2 + i] != 255) continue;
            unsigned expected = data[2*i] | data[2*i + 1] << 8;
            assert(s_pixels[(s.striker.y + y) * 320 + s.striker.x + x] == expected);
        }
    }
}

static void check_labels(lv_obj_t *obj) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(obj, &lv_label_class)) {
        lv_point_t size;
        const char *text = lv_label_get_text(obj);
        lv_text_get_size(&size, text, lv_obj_get_style_text_font(obj, 0), 0, 0,
                         LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x > lv_obj_get_content_width(obj)) {
            fprintf(stderr, "Clipped label: %s (%d > %d)\n", text,
                    (int)size.x, (int)lv_obj_get_content_width(obj));
            abort();
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i) check_labels(lv_obj_get_child(obj, i));
}

static bool has_label_text(lv_obj_t *obj, const char *expected) {
    if (lv_obj_check_type(obj, &lv_label_class) &&
        strcmp(lv_label_get_text(obj), expected) == 0) return true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
        if (has_label_text(lv_obj_get_child(obj, i), expected)) return true;
    return false;
}

static bool has_visible_label_text(lv_obj_t *obj, const char *expected) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return false;
    if (lv_obj_check_type(obj, &lv_label_class) &&
        strcmp(lv_label_get_text(obj), expected) == 0) return true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
        if (has_visible_label_text(lv_obj_get_child(obj, i), expected)) return true;
    return false;
}

static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *data) {
    s_flushed_pixels += lv_area_get_width(a) * lv_area_get_height(a);
    int w = lv_display_get_horizontal_resolution(d);
    int h = lv_display_get_vertical_resolution(d);
    int aw = lv_area_get_width(a);
    uint16_t *pixels = (uint16_t *)data;
    for (int y = a->y1; y <= a->y2; ++y) {
        for (int x = a->x1; x <= a->x2; ++x) {
            assert(x >= 0 && y >= 0 && x < w && y < h);
            s_pixels[y * w + x] = bsp_display_pixel_outside_rounded_rect(x, y, w, h, 30)
                ? 0 : pixels[(y - a->y1) * aw + x - a->x1];
        }
    }
    lv_display_flush_ready(d);
}

static void render(penalty_model_t *m, uint64_t now, const char *dir, const char *name) {
    static penalty_state_t previous = PENALTY_EXITING;
    uint16_t header_before[18 * 320];
    memcpy(header_before, s_pixels, sizeof(header_before));
    s_flushed_pixels = 0;
    lv_display_rotation_t rotation = m->state == PENALTY_ORIENT ? LV_DISPLAY_ROTATION_0 : LV_DISPLAY_ROTATION_90;
    if (lv_display_get_rotation(s_display) != rotation) lv_display_set_rotation(s_display, rotation);
    penalty_ui_update(m, now, 76, true, false);
    lv_obj_update_layout(lv_screen_active());
    check_labels(lv_screen_active());
    lv_refr_now(s_display);
    if (m->state == PENALTY_FLIGHT && previous == PENALTY_FLIGHT) {
        assert(s_flushed_pixels < 320 * 165); /* No full pitch redraw for one moving frame. */
        if (s_flushed_pixels > s_max_motion_pixels) s_max_motion_pixels = s_flushed_pixels;
        assert(memcmp(header_before, s_pixels, sizeof(header_before)) == 0);
    }
    previous = m->state;
    check_meter(m);
    check_target_grid(m);
    check_foreground(m, now);
    /* Dirty-region updates must exactly match a clean full redraw: no trails. */
    size_t bytes = lv_display_get_horizontal_resolution(s_display) *
                   lv_display_get_vertical_resolution(s_display) * sizeof(uint16_t);
    memcpy(s_partial, s_pixels, bytes);
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(s_display);
    if (memcmp(s_partial, s_pixels, bytes) != 0) {
        for (size_t i = 0; i < bytes / 2; ++i) {
            if (s_partial[i] == s_pixels[i]) continue;
            fprintf(stderr, "Dirty mismatch state=%d now=%llu at=(%zu,%zu): %04x != %04x\n",
                    m->state, (unsigned long long)now, i % 320, i / 320, s_partial[i], s_pixels[i]);
            break;
        }
        abort();
    }
    if (!name) return;
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "wb");
    assert(f);
    int w = lv_display_get_horizontal_resolution(s_display);
    int h = lv_display_get_vertical_resolution(s_display);
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; ++i) {
        uint16_t p = s_pixels[i];
        uint8_t rgb[] = {(uint8_t)(((p >> 11) & 31) * 255 / 31),
                         (uint8_t)(((p >> 5) & 63) * 255 / 63),
                         (uint8_t)((p & 31) * 255 / 31)};
        assert(fwrite(rgb, 1, 3, f) == 3);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    assert(penalty_art_background.data_size == 320 * 240 * 2);
    assert(penalty_art_striker[2].header.cf == LV_COLOR_FORMAT_RGB565A8);
    /* The calibrated toe is actual opaque artwork, not just a guessed anchor. */
    assert(penalty_art_striker[2].data[88 * 88 * 2 + 52 * 88 + 71] > 128);
    lv_init();
    s_display = lv_display_create(240, 320);
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(s_display, s_buffer, NULL, sizeof(s_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_display, flush);
    lv_obj_t *blank = lv_screen_active();
    size_t after_warmup = 0;
    for (unsigned cycle = 0; cycle < 50; ++cycle) {
        penalty_model_t m;
        penalty_model_init(&m, 42, 0);
        if (!penalty_ui_create()) {
            lv_mem_monitor_t memory;
            lv_mem_monitor(&memory);
            fprintf(stderr, "UI allocation guard, cycle=%u free=%zu biggest=%zu\n",
                    cycle, memory.free_size, memory.free_biggest_size);
            abort();
        }
        render(&m, 0, argv[1], cycle == 0 ? "01-orientation" : NULL);
        /* A real full-bleed cover, not the old text-only turn-right prompt. */
        assert(s_pixels[160 * 240 + 120] != s_pixels[40 * 240 + 120]);
        for (int y = 0; y < 320; ++y) {
            for (int x = 0; x < 240; ++x) {
                uint16_t expected = bsp_display_pixel_outside_rounded_rect(x, y, 240, 320, 30)
                    ? 0 : penalty_cover_pixels[y * 240 + x];
                assert(s_pixels[y * 240 + x] == expected);
            }
        }
        /* Unknown and full charge fit; changing charge does not replace the art. */
        penalty_ui_update(&m, 0, -1, false, false);
        lv_obj_update_layout(lv_screen_active());
        check_labels(lv_screen_active());
        penalty_ui_update(&m, 0, 100, true, false);
        lv_obj_update_layout(lv_screen_active());
        check_labels(lv_screen_active());
        penalty_model_input(&m, PENALTY_INPUT_OK, 1);
        m.difficulty = cycle % 3;
        render(&m, 1, argv[1], cycle == 0 ? "02-title" : NULL);
        penalty_model_input(&m, PENALTY_INPUT_OK, 2);
        render(&m, 2, argv[1], cycle == 0 ? "03-aim" : NULL);
        if (cycle == 0) {
            for (unsigned target = 0; target < PENALTY_TARGET_COUNT; ++target) {
                char capture[48];
                m.current.direction = (penalty_direction_t)target;
                snprintf(capture, sizeof(capture), "31-aim-target-%u", target);
                render(&m, 2, argv[1], capture);
            }
            m.current.direction = PENALTY_BOTTOM_CENTER;
            render(&m, 2, argv[1], NULL);
        }
        /* New stadium artwork: pale goal frame at its calibrated top-left. */
        assert(s_pixels[40 * 320 + 86] == rgb565(0xfbfbed));
        uint64_t t = 100;
        for (unsigned shot = 0; shot < 5; ++shot) {
            penalty_model_input(&m, PENALTY_INPUT_RIGHT, t++);
            m.current.outcome_roll = 0;
            penalty_model_input(&m, PENALTY_INPUT_OK, t);
            uint64_t fired = t + m.current.green_low * 8u +
                (m.current.perfect_low - m.current.green_low) * 40u + 20u;
            penalty_model_tick(&m, fired - 40);
            render(&m, fired - 40, argv[1], cycle == 0 && shot == 0 ? "04-charge" : NULL);
            penalty_model_input(&m, PENALTY_INPUT_OK, fired);
            for (unsigned elapsed = 0; elapsed < 600; elapsed += 50) {
                char capture[48];
                snprintf(capture, sizeof(capture), "flight-%03u", elapsed);
                render(&m, fired + elapsed, argv[1], cycle == 0 && shot == 0 ? capture : NULL);
            }
            render(&m, fired + 599, argv[1], cycle == 0 && shot == 0 ? "05-flight" : NULL);
            penalty_model_tick(&m, fired + 600);
            render(&m, fired + 600, argv[1], cycle == 0 && shot == 0 ? "06-result" : NULL);
            penalty_model_tick(&m, fired + 1500);
            t = fired + 1600;
        }
        assert(m.state == PENALTY_SUMMARY && m.perfect == 5 && m.goals == 5);
        render(&m, t, argv[1], cycle == 0 ? "07-summary" : NULL);
        if (cycle == 0) {
            const char *names[] = {"08-goal", "09-perfect", "10-save", "11-weak", "12-high", "13-timeout",
                                   "18-green-goal", "19-green-save", "30-perfect-save"};
            for (unsigned outcome = 0; outcome < PENALTY_OUTCOME_COUNT; ++outcome) {
                m.state = PENALTY_RESULT;
                m.current.outcome = outcome;
                m.current.keeper = outcome == PENALTY_GOAL
                    ? (m.current.direction + 2) % PENALTY_TARGET_COUNT : m.current.direction;
                m.shots[4] = m.current;
                m.goals = 4 + penalty_is_goal(outcome);
                m.perfect = 4 + (outcome == PENALTY_PERFECT || outcome == PENALTY_PERFECT_SAVE);
                render(&m, t, argv[1], names[outcome]);
            }
            for (unsigned direction = 0; direction < PENALTY_TARGET_COUNT; ++direction) {
                for (unsigned outcome = 0; outcome < PENALTY_OUTCOME_COUNT; ++outcome) {
                    m.current.direction = direction;
                    m.current.keeper = outcome == PENALTY_GOAL
                        ? (direction + 2) % PENALTY_TARGET_COUNT : direction;
                    m.current.outcome = outcome;
                    m.state = PENALTY_FLIGHT; m.since_ms = t;
                    for (unsigned step = 0; step <= 600; step += 50)
                        render(&m, t + step, argv[1], NULL);
                    m.state = PENALTY_RESULT;
                    char capture[48];
                    snprintf(capture, sizeof(capture), "outcome-%u-dir-%u", outcome, direction);
                    render(&m, t + 601, argv[1], capture);
                    if (outcome == PENALTY_SAVE || outcome == PENALTY_GREEN_SAVE ||
                        outcome == PENALTY_PERFECT_SAVE) {
                        penalty_scene_t s = penalty_scene_at(&m, t + 601);
                        const lv_image_dsc_t *image = &penalty_art_keeper[s.keeper.frame];
                        int x = s.ball_x - s.keeper.x, y = s.ball_y - s.keeper.y;
                        assert(image->data[56 * 56 * 2 + y * 56 + x] > 128);
                    }
                }
            }
            m.state = PENALTY_TITLE;
            m.selection = 2; m.muted = true;
            render(&m, t, argv[1], "14-muted");
            m.state = PENALTY_SUMMARY;
            m.goals = m.perfect = 0;
            for (unsigned i = 0; i < PENALTY_SHOTS; ++i) m.shots[i].outcome = PENALTY_WEAK;
            render(&m, t, argv[1], "15-summary-zero");
            m.state = PENALTY_HELP;
            render(&m, t, argv[1], "16-help");
            for (unsigned d = 0; d < 3; ++d) {
                const unsigned widths[] = {20, 11, 6};
                m.completed = m.goals = m.perfect = 0;
                m.difficulty = d; m.selection = 1; m.state = PENALTY_TITLE;
                char name[48];
                snprintf(name, sizeof(name), "20-difficulty-%u", d);
                render(&m, t, argv[1], name);
                for (unsigned edge = 0; edge < 2; ++edge) {
                    m.state = PENALTY_AIM;
                    m.current.green_low = edge ? 91 - widths[d] : 30;
                    m.current.green_high = m.current.green_low + widths[d] - 1;
                    m.current.perfect_low = edge ? m.current.green_high - 1 : m.current.green_low;
                    snprintf(name, sizeof(name), "21-meter-%u-edge-%u", d, edge);
                    render(&m, t, argv[1], name);
                    m.state = PENALTY_CHARGE;
                    for (unsigned p = 0; p <= 100; ++p) {
                        m.current.power = p;
                        render(&m, t, argv[1], NULL);
                    }
                }
            }
            m.state = PENALTY_AIM;
            m.difficulty = PENALTY_NORMAL;
            m.completed = 1;
            m.current = (penalty_shot_t){
                .green_low = 47, .green_high = 57, .perfect_low = 52,
                .outcome_roll = 6, .keeper = PENALTY_TOP_RIGHT,
            };
            render(&m, t, argv[1], "22-interactive-copy");
            m.completed = PENALTY_SHOTS - 1;
            render(&m, t, argv[1], "23-last-kick-copy");

            m.language = PENALTY_LANGUAGE_ZH_CN;
            m.state = PENALTY_TITLE; m.selection = 0;
            render(&m, t, argv[1], "24-title-zh");
            assert(!has_visible_label_text(lv_screen_active(), "一球定胜负"));
            m.state = PENALTY_SETTINGS; m.selection = 2;
            render(&m, t, argv[1], "25-settings-zh");
            assert(has_label_text(lv_screen_active(), "设置"));
            m.state = PENALTY_HELP;
            render(&m, t, argv[1], "26-help-zh");
            assert(has_label_text(lv_screen_active(), "玩法说明"));
            m.state = PENALTY_AIM; m.completed = 1;
            render(&m, t, argv[1], "27-aim-zh");
            m.state = PENALTY_SUMMARY; m.goals = 3; m.perfect = 1;
            render(&m, t, argv[1], "28-summary-zh");
            m.state = PENALTY_ORIENT;
            render(&m, t, argv[1], "29-cover-zh");
            assert(has_label_text(lv_screen_active(), "设备向右横转 90°\n再按确认键"));
        }
        lv_display_set_rotation(s_display, LV_DISPLAY_ROTATION_0);
        lv_screen_load(blank);
        penalty_ui_destroy();
        lv_refr_now(s_display);
        lv_mem_monitor_t memory;
        lv_mem_monitor(&memory);
        if (cycle == 0) after_warmup = memory.free_size;
        else assert(memory.free_size == after_warmup);
    }
    printf("Penalty LVGL: PASS (50 sessions/entry-exit cycles, 24 KB pool, free after exit=%zu, max moving-frame pixels=%zu)\n",
           after_warmup, s_max_motion_pixels);
    lv_display_delete(s_display);
    lv_deinit();
    return 0;
}
