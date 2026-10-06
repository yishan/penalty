#include "penalty_ui.h"
#include "penalty_i18n.h"
#include "penalty_multiplayer_core.h"
#include "penalty_scene.h"
#include "penalty_fonts.h"
#include "bsp_display_rounding.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint16_t pixels[240 * 320], partial[240 * 320], buffer[240 * 20];
static lv_display_t *display;
static uint16_t reference_pitch[240 * 165]; /* Desktop-only comparison, not firmware RAM. */
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *data) {
    const uint16_t *p = (const uint16_t *)data;
    for (int y = a->y1; y <= a->y2; ++y)
        for (int x = a->x1; x <= a->x2; ++x)
            pixels[y * 240 + x] = bsp_display_pixel_outside_rounded_rect(x, y, 240, 320, 30)
                ? 0 : p[(y - a->y1) * lv_area_get_width(a) + x - a->x1];
    lv_display_flush_ready(d);
}
static void glyphs(const lv_font_t *font, const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        uint32_t code = *p++;
        unsigned continuation = code >= 0xf0 ? 3 : code >= 0xe0 ? 2 : code >= 0xc0 ? 1 : 0;
        if (continuation) code &= (1u << (6 - continuation)) - 1;
        while (continuation--) code = (code << 6) | (*p++ & 0x3f);
        if (code < 32) continue;
        lv_font_glyph_dsc_t d;
        assert(lv_font_get_glyph_dsc(font, &d, code, 0) && !d.is_placeholder);
    }
}
static void labels(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *text = lv_label_get_text(o);
        glyphs(lv_obj_get_style_text_font(o, 0), text);
        lv_area_t a;
        lv_obj_get_coords(o, &a);
        if (a.x1 < 0 || a.x2 >= 240 || a.y1 < 0 || a.y2 >= 320) {
            fprintf(stderr, "Multiplayer label outside screen: %s (%d,%d-%d,%d)\n", text, a.x1,a.y1,a.x2,a.y2);
            assert(false);
        }
        if (lv_label_get_long_mode(o) == LV_LABEL_LONG_CLIP) {
            lv_point_t size;
            lv_text_get_size(&size, text, lv_obj_get_style_text_font(o,0), 0, 0,
                             LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            if (size.x > lv_obj_get_content_width(o)) {
                fprintf(stderr, "Multiplayer clipped text: %s\n", text); assert(false);
            }
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) labels(lv_obj_get_child(o, i));
}
static void screenshot(const char *dir, const char *name, unsigned language) {
    char file[1024];
    snprintf(file, sizeof(file), "%s/m3-%s-%u.ppm", dir, name, language);
    FILE *out = fopen(file, "wb"); assert(out);
    fprintf(out, "P6\n240 320\n255\n");
    for (unsigned i = 0; i < 240 * 320; ++i) {
        uint16_t p = pixels[i];
        uint8_t rgb[3] = {((p >> 11) & 31) * 255 / 31,
            ((p >> 5) & 63) * 255 / 63, (p & 31) * 255 / 31};
        assert(fwrite(rgb, 1, 3, out) == 3);
    }
    fclose(out);
}
static lv_obj_t *visible_label(lv_obj_t *o, const char *text) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return NULL;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), text)) return o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) {
        lv_obj_t *found = visible_label(lv_obj_get_child(o, i), text);
        if (found) return found;
    }
    return NULL;
}

static void panel_layout(penalty_model_t *v, uint64_t at, const char *header, const char *heading,
                         const char *detail, const char *footer, bool field, bool menu) {
    if (menu) penalty_ui_menu_panel(v, at, 100, true, header, heading, detail, footer);
    else penalty_ui_panel(v, at, 100, true, header, heading, detail, footer, field);
    lv_obj_update_layout(lv_screen_active());
    labels(lv_screen_active());
    if (*detail) {
        lv_obj_t *body = visible_label(lv_screen_active(), detail);
        assert(body);
        const lv_font_t *font = lv_obj_get_style_text_font(body, 0);
        assert(lv_obj_get_style_text_line_space(body, 0) == (menu ? 40 - font->line_height : 0));
        if (menu) {
            unsigned rows = 1;
            for (const char *p = detail; *p; ++p) rows += *p == '\n';
            assert(rows <= 4);
            assert(lv_obj_get_y(body) == 84);
            assert(lv_obj_get_height(body) == (int)(rows - 1) * 40 + font->line_height);
            assert(lv_obj_get_y(body) + lv_obj_get_height(body) < 250);
        }
    }
    lv_refr_now(display);
    memcpy(partial, pixels, sizeof(pixels));
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(display);
    assert(memcmp(partial, pixels, sizeof(pixels)) == 0);
}

static void panel(penalty_model_t *v, uint64_t at, const char *header, const char *heading,
                  const char *detail, const char *footer, bool field) {
    panel_layout(v, at, header, heading, detail, footer, field, false);
}

static void paired_round_headers(unsigned language) {
    const uint8_t id[16] = {1};
    penalty_mp_core_t c;
    penalty_mp_init(&c, PENALTY_DUEL_HOST, PENALTY_NORMAL, id, 42, 1000);
    const penalty_mp_phase_t phases[] = {PENALTY_MP_AIM, PENALTY_MP_CHARGE,
        PENALTY_MP_WAIT, PENALTY_MP_ANIMATION, PENALTY_MP_RESULT, PENALTY_MP_DONE};
    for (unsigned kick = 1; kick <= 14; ++kick) {
        c.duel.round_id = kick;
        c.duel.round_result.outcome = PENALTY_DUEL_GOAL;
        for (unsigned phase = 0; phase < sizeof(phases)/sizeof(phases[0]); ++phase) {
            c.phase = phases[phase];
            for (unsigned peer = 0; peer < 2; ++peer) {
                c.peer = peer;
                penalty_model_t v;
                penalty_mp_project(&c, &v, language, false, 1000);
                char header[96], expected[96];
                snprintf(header, sizeof(header), penalty_text(language, PENALTY_TEXT_MP_SCORE_FMT),
                         penalty_round_for_kick(c.duel.round_id), 12u, 11u);
                snprintf(expected, sizeof(expected), penalty_text(language, PENALTY_TEXT_MP_SCORE_FMT),
                         (kick + 1) / 2, 12u, 11u);
                assert(!strcmp(header, expected));
                panel(&v, 1000, header, "", "", "", true);
                assert(visible_label(lv_screen_active(), expected));
            }
        }
    }
}

static void check_ready_kit(bool blue) {
    unsigned red_pixels = 0, blue_pixels = 0;
    for (unsigned y = 100; y < 183; ++y) {
        for (unsigned x = 48; x < 136; ++x) {
            uint16_t p = pixels[y * 240 + x];
            unsigned r = (p >> 11) & 31, g = (p >> 5) & 63, b = p & 31;
            red_pixels += r > 24 && g < 24 && b < 16;
            blue_pixels += b > 16 && r < 12;
        }
    }
    assert(blue ? blue_pixels > 35 && red_pixels == 0
                : red_pixels > 35 && blue_pixels == 0);
}

static void jersey_views(const char *dir, unsigned language) {
    uint8_t id[16] = {1};
    penalty_mp_core_t c;
    penalty_mp_init(&c, PENALTY_DUEL_HOST, PENALTY_NORMAL, id, 42, 1000);
    c.duel.actions[0].target = PENALTY_TOP_LEFT;
    c.duel.actions[1].target = PENALTY_BOTTOM_RIGHT;
    c.duel.round_result.outcome = PENALTY_DUEL_GOAL;
    const penalty_mp_phase_t phases[] = {PENALTY_MP_AIM, PENALTY_MP_CHARGE,
        PENALTY_MP_WAIT, PENALTY_MP_ANIMATION, PENALTY_MP_RESULT};
    for (unsigned shooter = 0; shooter < 2; ++shooter) {
        c.duel.current_shooter = (penalty_duel_peer_t)shooter;
        for (unsigned phase = 0; phase < sizeof(phases)/sizeof(phases[0]); ++phase) {
            c.phase = phases[phase];
            for (unsigned elapsed = 0; elapsed <= 600; elapsed += 50) {
                for (unsigned viewer = 0; viewer < 2; ++viewer) {
                    c.peer = (penalty_duel_peer_t)viewer;
                    penalty_model_t v;
                    penalty_mp_project(&c, &v, language, false, 1000 + elapsed);
                    penalty_scene_t scene = penalty_scene_at(&v, 1000 + elapsed);
                    assert(scene.blue_striker == (shooter == PENALTY_DUEL_GUEST));
                    panel(&v, 1000 + elapsed, "", "", "", "", true);
                    bool moving = phase >= 3;
                    if (!moving) check_ready_kit(shooter == PENALTY_DUEL_GUEST);
                    /* Private aim/meter text may differ. The actual playing field
                     * during the shared animation must be pixel-identical. */
                    if (moving) {
                        if (!viewer) memcpy(reference_pitch, pixels + 18 * 240, sizeof(reference_pitch));
                        else assert(!memcmp(reference_pitch, pixels + 18 * 240, sizeof(reference_pitch)));
                    }
                    if (elapsed == 0 && (phase == 0 || phase == 2 || phase == 3)) {
                        char name[64];
                        snprintf(name, sizeof(name), "jersey-shooter-%u-viewer-%u-phase-%u", shooter, viewer, phase);
                        screenshot(dir, name, language);
                    }
                }
            }
        }
    }
    /* Changing only the identity at a ready pose must invalidate the old kit. */
    penalty_model_t v;
    c.phase = PENALTY_MP_AIM;
    c.peer = PENALTY_DUEL_HOST;
    penalty_mp_project(&c, &v, language, false, 1000);
    for (unsigned i = 0; i < 4; ++i) {
        v.multiplayer_blue_shooter = (i & 1) != 0;
        panel(&v, 1000, "", "", "", "", true);
        check_ready_kit(v.multiplayer_blue_shooter);
    }
}
int main(int argc, char **argv) {
    assert(argc == 2);
    lv_init(); display = lv_display_create(240, 320);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    lv_obj_t *blank = lv_screen_active();
    size_t free_after_first = 0;
    for (unsigned cycle = 0; cycle < 50; ++cycle) {
        assert(penalty_ui_create());
        for (unsigned language = 0; language < PENALTY_LANGUAGE_COUNT; ++language) {
            if (!cycle) {
                jersey_views(argv[1], language);
                paired_round_headers(language);
            }
            for (unsigned id = 0; id < PENALTY_TEXT_COUNT; ++id) {
                const char *text = penalty_text(language, id);
                assert(text);
                if (language == PENALTY_LANGUAGE_ZH_CN) {
                    glyphs(&penalty_font_zh_14, text); glyphs(&penalty_font_zh_20, text);
                }
            }
            penalty_model_t v = {.language = language, .state = PENALTY_SETTINGS};
            char detail[256], heading[96];
            const penalty_text_id_t rows[] = {PENALTY_TEXT_MP_SINGLE, PENALTY_TEXT_MP_CREATE,
                PENALTY_TEXT_MP_JOIN, PENALTY_TEXT_SETTINGS_BACK};
            for (unsigned selected = 0; selected < 4; ++selected) {
                size_t used = 0;
                for (unsigned row = 0; row < 4; ++row)
                    used += snprintf(detail + used, sizeof(detail) - used, "%s%s%s%s",
                        selected == row ? PENALTY_MENU_SELECTED_PREFIX : "", penalty_text(language, rows[row]),
                        selected == row ? PENALTY_MENU_SELECTED_SUFFIX : "", row < 3 ? "\n" : "");
                panel_layout(&v, 0, penalty_text(language, PENALTY_TEXT_COIN_HEADER),
                    penalty_text(language, PENALTY_TEXT_MENU_SHOOTOUT), detail,
                    penalty_text(language, PENALTY_TEXT_MENU_FOOTER), false, true);
                if (!cycle) {
                    char name[32]; snprintf(name, sizeof(name), "submenu-row-%u", selected);
                    screenshot(argv[1], name, language);
                }
            }
            snprintf(heading, sizeof(heading), penalty_text(language, PENALTY_TEXT_MP_COMPARE_FMT), 999999ul);
            panel(&v, 0, penalty_text(language, PENALTY_TEXT_MP_HEADER), heading,
                penalty_text(language, PENALTY_TEXT_MP_COMPARE_HINT),
                penalty_text(language, PENALTY_TEXT_MP_ACCEPT_HINT), false);
            if (!cycle) screenshot(argv[1], "compare", language);
            panel(&v, 0, penalty_text(language, PENALTY_TEXT_MP_HEADER),
                penalty_text(language, PENALTY_TEXT_MP_DIFFICULTY),
                penalty_text(language, PENALTY_TEXT_MP_HOST_WAIT), penalty_text(language, PENALTY_TEXT_HOLD_COVER), false);
            if (!cycle) screenshot(argv[1], "difficulty", language);
            const penalty_text_id_t stages[] = {PENALTY_TEXT_MP_GUEST_WAIT,
                PENALTY_TEXT_MP_CONFIRM_DIFF, PENALTY_TEXT_MP_WAIT_COIN};
            for (unsigned difficulty = 0; difficulty < 3; ++difficulty) {
                for (unsigned stage = 0; stage < sizeof(stages)/sizeof(stages[0]); ++stage) {
                    snprintf(detail, sizeof(detail), "%s\n\n%s",
                        penalty_difficulty_text_localized(language, difficulty),
                        penalty_text(language, stages[stage]));
                    panel(&v, 0, penalty_text(language, PENALTY_TEXT_MP_HEADER),
                        penalty_text(language, PENALTY_TEXT_MP_DIFFICULTY), detail,
                        penalty_text(language, PENALTY_TEXT_HOLD_COVER), false);
                }
            }
            panel(&v, 0, penalty_text(language, PENALTY_TEXT_MP_HEADER),
                penalty_text(language, PENALTY_TEXT_COIN_HEADING),
                penalty_text(language, PENALTY_TEXT_COIN_READY),
                penalty_text(language, PENALTY_TEXT_HOLD_COVER), false);
            if (!cycle) screenshot(argv[1], "coin", language);
            panel_layout(&v, 0, penalty_text(language, PENALTY_TEXT_MP_HEADER),
                penalty_text(language, PENALTY_TEXT_MP_SCAN), ">> 9999 <<\n0000\n1234\n5678",
                penalty_text(language, PENALTY_TEXT_MENU_FOOTER), false, true);
            if (!cycle) screenshot(argv[1], "lobbies", language);
            char error[64];
            snprintf(error, sizeof(error), penalty_text(language, PENALTY_TEXT_MP_ERROR_FMT), 99999);
            panel(&v, 0, penalty_text(language, PENALTY_TEXT_MP_HEADER),
                penalty_text(language, PENALTY_TEXT_MP_DISCONNECTED), error,
                penalty_text(language, PENALTY_TEXT_MP_RETRY), false);
            if (!cycle) screenshot(argv[1], "disconnected", language);
            snprintf(heading,sizeof(heading),penalty_text(language,PENALTY_TEXT_MP_RECOVERY_FMT),30u);
            panel(&v,0,penalty_text(language,PENALTY_TEXT_MP_HEADER),heading,
                penalty_text(language,PENALTY_TEXT_MP_RECOVERY_HINT),penalty_text(language,PENALTY_TEXT_MP_CANCEL),false);
            if (!cycle) screenshot(argv[1],"reconnect",language);
            snprintf(heading,sizeof(heading),penalty_text(language,PENALTY_TEXT_MP_SUDDEN_FMT),12u);
            snprintf(detail,sizeof(detail),penalty_text(language,PENALTY_TEXT_MP_SCORE_FMT),10u,5u,4u);
            size_t used=strlen(detail); snprintf(detail+used,sizeof(detail)-used,"\n\n%s",heading);
            panel(&v,0,penalty_text(language,PENALTY_TEXT_MP_HEADER),penalty_text(language,PENALTY_TEXT_MP_WON),
                detail,penalty_text(language,PENALTY_TEXT_MP_DONE),false);
            if (!cycle) screenshot(argv[1],"summary",language);
            panel(&v,0,penalty_text(language,PENALTY_TEXT_MP_HEADER),penalty_text(language,PENALTY_TEXT_MP_LOST),
                detail,penalty_text(language,PENALTY_TEXT_MP_DONE),false);
            if (!cycle) screenshot(argv[1],"lost",language);
            panel(&v,0,penalty_text(language,PENALTY_TEXT_MP_HEADER),penalty_text(language,PENALTY_TEXT_MP_DISCONNECTED),
                penalty_text(language,PENALTY_TEXT_MP_NO_CONTEST),penalty_text(language,PENALTY_TEXT_MP_RETRY),false);
            if (!cycle) screenshot(argv[1],"no-contest",language);
            panel(&v,0,"DEV BLE TEST","Disconnect test",
                "OK: disconnect 10s\nDOWN: disconnect 35s\nUP: back",
                "Keep both devices powered",false);
            if (!cycle) screenshot(argv[1],"dev-ble-fault",language);
            uint8_t id[16] = {1}; penalty_mp_core_t c;
            penalty_mp_init(&c, PENALTY_DUEL_GUEST, PENALTY_NORMAL, id, 42, 0);
            c.phase = PENALTY_MP_AIM; c.duel.current_shooter = PENALTY_DUEL_HOST;
            c.duel.round_config.keeper_meter = (penalty_duel_meter_t){50,60,55};
            c.local_action.target = PENALTY_BOTTOM_RIGHT;
            penalty_mp_project(&c, &v, language, false, 0);
            for (unsigned target = 0; target < PENALTY_TARGET_COUNT; ++target) {
                v.current.opponent_target = target;
                assert(penalty_scene_at(&v, 0).striker.frame == 0); /* Blind keeper: no cue. */
            }
            snprintf(heading, sizeof(heading), penalty_text(language, PENALTY_TEXT_MP_SCORE_FMT), 1u, 0u, 0u);
            snprintf(detail, sizeof(detail), penalty_text(language, PENALTY_TEXT_AIM_KEEPER_FMT),
                penalty_direction_text(language, v.current.direction));
            panel(&v, 0, heading, "", penalty_text(language, PENALTY_TEXT_MP_KEEP), detail, true);
            if (!cycle) screenshot(argv[1], "blind-keeper", language);
            c.phase = PENALTY_MP_WAIT;
            penalty_mp_project(&c, &v, language, false, 0);
            panel(&v, 0, heading, "", penalty_text(language, PENALTY_TEXT_MP_WAIT_ACTION), "", true);
            if (!cycle) screenshot(argv[1], "locked-wait", language);
            c.phase = PENALTY_MP_CHARGE; penalty_mp_project(&c, &v, language, false, 600);
            snprintf(detail, sizeof(detail), penalty_text(language, PENALTY_TEXT_CHARGE_KEEPER_FMT), v.current.power);
            panel(&v, 600, heading, "", penalty_text(language, PENALTY_TEXT_MP_KEEP), detail, true);
            if (!cycle) screenshot(argv[1], "charge", language);
            c.phase = PENALTY_MP_DONE; c.duel.round_result.outcome = PENALTY_DUEL_SAVE;
            penalty_mp_project(&c, &v, language, false, 900);
            panel(&v, 900, heading, "", penalty_outcome_text_localized(language, v.current.outcome),
                  penalty_text(language, PENALTY_TEXT_MP_DONE), true);
            if (!cycle) screenshot(argv[1], "done", language);
        }
        lv_screen_load(blank); penalty_ui_destroy(); lv_refr_now(display);
        lv_mem_monitor_t memory; lv_mem_monitor(&memory);
        if (!cycle) free_after_first = memory.free_size;
        else assert(memory.free_size == free_after_first);
    }
    printf("Penalty PvP jerseys: PASS (creator red/joiner blue, both viewers, aim/charge/wait/animation/result, pixel-identical shared animation, color-only invalidation)\n");
    printf("Penalty paired rounds/menu: PASS (both languages, 40px menu/lobby rows, status spacing reset, two kicks per displayed round, all match phases)\n");
    printf("Penalty M5 LVGL: PASS (both languages, actual glyphs/layout, difficulty/coin/summary/reconnect, blind keeper, dirty/full redraw, 50 cycles, 24 KB pool; free after exit=%zu)\n", free_after_first);
    lv_display_delete(display); lv_deinit();
}
