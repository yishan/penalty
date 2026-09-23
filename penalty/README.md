<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Penalty

A three-button, offline portrait penalty-kick game for AI Passport. Keep the device upright: UP and DOWN scan the six targets vertically by column, and OK confirms.

Status: v1.4 adds a standalone Keeper Challenge beside the existing shooting challenge. Both modes stay portrait at 240 × 320 and use the same six-target cycle and two-press timing meter. In goalkeeper mode, the blue AI shooter's truthful body/foot pose previews one of six shots for 900 / 600 / 350 ms on Easy / Normal / Hard; the player selects the black keeper's dive and times the second OK press. Host and firmware evidence is recorded in the [validation and device checklist](docs/validation.md); this revision still needs device acceptance.

## Start here

- [Game design outline](docs/game-design.md): scope, controls, rules, screens, architecture, milestones, and acceptance criteria.
- [Goalkeeper mode design](docs/plans/2026-09-23-goalkeeper-mode-design.md): approved viewpoint, cue timing, save probabilities, animation set, and acceptance boundary.
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

Power on to the portrait pixel cover with fixed English and Chinese game marks and press OK without turning the device. UP/DOWN select Shoot, Keep, Settings, or Help. Settings contains difficulty, sound, language, and Back. A device with no valid saved preference starts in Simplified Chinese. Choose Language to switch English / Simplified Chinese; the page refreshes immediately and a valid choice persists across game exit and device restart. Choose Difficulty to cycle Easy / Normal / Hard.

Shoot keeps the existing rules: green widths are 20 / 11 / 6 values at a new random position for each kick. The two-value dark line averages 90% goals, the rest of green 65%, and normal power outside green 25% after the keeper's uniformly random six-target prediction.

Keep uses the same five-attempt flow from the same behind-the-shooter camera. Watch the blue shooter's opening pose, select the distant black keeper's dive, press OK to start the timing meter, then press OK again to dive. An exact-cell read saves 90% / 65% / 25% on dark line / green / outside green; the other row in the same column saves 35% / 20% / 5%; a different column cannot save. Values 0–29 are too early and 91–100 too late. The AI target and outcome roll are locked before input and never rerolled.

Each attempt starts at top-center. DOWN cycles `top-center → bottom-center → top-left → bottom-left → top-right → bottom-right → top-center`; UP follows the exact reverse. This keeps movement within each column aligned with the physical UP/DOWN direction while the column changes only after its two cells. Press OK to lock the target and start the meter, then press OK again to kick or dive. The marker slows to one-fifth speed in green. Five attempts lead to a goals or saves summary; long OK cancels the session and returns to the portrait cover. Sound toggles mute with a final off cue and an on cue; Help explains both modes. Retry/title retain the chosen mode, difficulty, and mute; returning to the cover resets session state. Language alone is persistent; scores, difficulty, and mute are not. See the [exact rules](docs/game-design.md#5-shooting-rules--v10-calibrated-probabilities).

Run from the repository root:

```bash
./tools/validate.sh --static
./tools/validate.sh --firmware
./tools/validate.sh
```

The project version is stored in `penalty/VERSION`. A successful firmware gate
keeps the canonical merged image and also creates the delivery artifact
`build/FoloToy-AI-Passport-Penalty-v1.4.2-full.bin`. Future releases keep the
same naming pattern and update only the semantic version.

The static gate includes the game model and navigation regression tests. To render the actual UI on a desktop with CMake, a C compiler, and the locked LVGL 9.5.0 sources:

```bash
./tools/preview-penalty.sh /path/to/lvgl-9.5.0
```

The default source path is `managed_components/lvgl__lvgl` after an ESP-IDF build. Captures are written to `build/penalty-preview/*.ppm` (v0.5 uses `build/penalty-art-preview/`); tests check text width, the six-target overlay, green/precision-line pixels, upper/lower keeper contact, foreground occlusion, dirty/full redraw equivalence and 50 sessions/entry-exit cycles in the 24 KB LVGL pool. This is a rendering harness, not a hardware emulator or interactive desktop port.
