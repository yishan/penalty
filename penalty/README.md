<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Penalty

A three-button, offline landscape penalty-kick game for AI Passport. The image rotates 90° counterclockwise; hold the device 90° clockwise so UP means right and DOWN means left. OK remains confirm.

Status: v1.1 visual refresh implemented and built, 2026-09-17. The portrait cover now adds the fixed Chinese game mark beneath `PENALTY`; active pitch art uses horizontal grass bands; and the keeper wears black. The landscape title screen no longer displays the former English/Chinese slogan. The v1.0 six-target rules, calibrated 90% / 65% / 25% goal rates, bilingual UI, and persistent language choice are unchanged. See the [installation, validation, and device checklist](docs/validation.md) for exact evidence; device acceptance remains pending.

## Start here

- [Game design outline](docs/game-design.md): scope, controls, rules, screens, architecture, milestones, and acceptance criteria.
- [Pixel-art layers and integration](docs/plans/2026-09-15-pixel-art-integration-design.md): integrated artwork, calibrated contact/flight, conversion and device checks.
- [Hybrid audio feedback design](docs/plans/2026-09-16-audio-feedback-design.md): confirmed sound style, silent-output finding, event map, delivery milestones, and device acceptance.
- [Dynamic copy design](docs/plans/2026-09-16-dynamic-copy-design.md): teaching boundary, encouragement pool, result commentary, deterministic selection, and UI checks.
- [Localization design](docs/localization.md): language menu, persistence boundary, Chinese font subset, overflow checks, and fallback behavior.
- [Validation and device checklist](docs/validation.md): evidence, limitations, and how to try the game.
- [Community submission](docs/community-submission.md): bilingual listing materials, illustrative cover, and actual review status.
- [Repository agent instructions](../AGENTS.md): mandatory entry point before development.
- [AI development guide](../docs/development/ai-guide.md): application/BSP boundaries and lifecycle rules.
- [Build and test](../docs/development/engineering/build-and-test.md): shared validation commands.

## Project layout

```text
penalty/
  README.md / README.zh_CN.md
  docs/
    game-design.md / game-design.zh_CN.md
    validation.md / validation.zh_CN.md
```

The user requested `penalty/` as the project directory. Keep its planning and future game-specific design assets here. Create asset/prototype subdirectories when they contain actual work.

The existing repository builds the game: [model](../main/penalty_model.c), [audio event map](../main/penalty_audio.c), [UI](../main/penalty_ui.c), and [application adapter](../main/penalty_app.c) live in `main/`; reusable rotation and observable volume control stay in `components/bsp/`. [Rule/audio tests](../tests/test_penalty_audio.c) and [actual LVGL rendering tests](../tests/penalty_preview/preview.c) live in `tests/`. There is no duplicated BSP or independent ESP-IDF project under `penalty/`.

## Play and validate

Power on to the portrait pixel cover with fixed English and Chinese game marks. Turn the device right and press OK. UP/DOWN select Play, Settings, or Help. Settings contains difficulty, sound, language, and Back. Choose Language to switch English / Simplified Chinese; the page refreshes immediately and the choice persists across game exit and device restart. Choose Difficulty to cycle Easy / Normal / Hard, then return and select Play. Green widths remain 20 / 11 / 6 values at a new random position for each kick. The two-value dark line averages 90% goals, the rest of green 65%, and normal power outside green 25% after the keeper's uniformly random six-target prediction.

UP advances clockwise and DOWN reverses around top-left → top-center → top-right → bottom-right → bottom-center → bottom-left. Observe the zones before charging; press OK to lock the target and start the meter, then press OK again to kick. The marker slows to one-fifth speed in green. Five attempts lead to the summary; long OK cancels the session and returns to the portrait cover. Sound toggles mute with a final off cue and an on cue; Help explains the controls. Retry/title retain difficulty and mute; returning to the cover resets them. Language alone is persistent; scores, difficulty, and mute are not. See the [exact rules](docs/game-design.md#5-shooting-rules--v10-calibrated-probabilities).

Run from the repository root:

```bash
./tools/validate.sh --static
./tools/validate.sh --firmware
./tools/validate.sh
```

The static gate includes the game model and navigation regression tests. To render the actual UI on a desktop with CMake, a C compiler, and the locked LVGL 9.5.0 sources:

```bash
./tools/preview-penalty.sh /path/to/lvgl-9.5.0
```

The default source path is `managed_components/lvgl__lvgl` after an ESP-IDF build. Captures are written to `build/penalty-preview/*.ppm` (v0.5 uses `build/penalty-art-preview/`); tests check text width, the six-target overlay, green/precision-line pixels, upper/lower keeper contact, foreground occlusion, dirty/full redraw equivalence and 50 sessions/entry-exit cycles in the 24 KB LVGL pool. This is a rendering harness, not a hardware emulator or interactive desktop port.
