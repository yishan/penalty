<p align="right">
  <a href="2026-09-22-portrait-gameplay-design.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Portrait Gameplay Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Convert Penalty from a portrait-cover/landscape-game flow into a fully portrait 240 x 320 game while preserving its six-target rules, probabilities, difficulty, audio, localization, persistence, and five-kick session.

**Architecture:** Keep the pure-C model, scene calculator, LVGL renderer, and application adapter separated. Replace orientation-specific state/input names with portrait cover and vertical navigation semantics, export a centered portrait-compatible pitch from the approved art, and reflow all LVGL coordinates without adding runtime scaling or a full-screen buffer.

**Tech Stack:** C11 pure model and scene tests, LVGL 9.5.0, ESP-IDF 5.5.3, deterministic Pillow asset conversion, ESP32-C3 with an 8 MB Flash and no PSRAM.

---

## Accepted behavior

- The display remains portrait from cover through summary and exit. No turn-device prompt or landscape transition remains.
- The cover keeps its approved artwork and replaces the rotation prompt with a short press-OK start prompt.
- Every kick starts at top-center.
- DOWN cycles `top-center -> bottom-center -> top-left -> bottom-left -> top-right -> bottom-right -> top-center`.
- UP follows the exact reverse cycle. Within each column, DOWN moves down and UP moves up.
- OK still locks the target, starts charge, locks power, and shoots. Long OK still returns to the cover.
- Six keeper cells, goal probabilities, difficulty widths, green-zone timing, five kicks, audio, bilingual copy, language persistence, battery behavior, and score settlement do not change.

## Portrait layout

The logical screen is always 240 x 320. The title/game pitch occupies the upper portion, using a new 240 x 240 centered export of the current approved 320 x 240 stadium. Game sprites remain native-size Flash assets and receive portrait-calibrated coordinates. The HUD uses the remaining vertical space: header at the top, field below it, a 192-pixel power meter, result/interaction copy, and the long-OK help line at the bottom. Settings and Help use the additional vertical room rather than compressing landscape text.

No runtime image scaling, PNG decoder, extra frame buffer, network feature, persistent-format change, or partition change is introduced. Portrait remains the BSP baseline rotation, so the application no longer requests a quarter-turn. Failure handling for audio, battery, stale button events, and memory allocation remains intact.

### Task 1: Lock vertical input and target order with failing tests

**Files:**
- Modify: `tests/test_penalty_model.c`
- Modify: `main/penalty_model.h`
- Modify: `main/penalty_model.c`

1. Change model tests to use `PENALTY_INPUT_UP` and `PENALTY_INPUT_DOWN` and assert the accepted complete cycles from top-center.
2. Run the focused model test and confirm it fails against the existing row-wise LEFT/RIGHT implementation.
3. Rename the cover state and navigation inputs, then implement the column-first vertical cycle with explicit lookup tables.
4. Re-run the focused model test and all pure-C model tests.

### Task 2: Remove runtime rotation from the application shell

**Files:**
- Modify: `main/penalty_app.c`
- Modify: `main/penalty_audio.c`
- Modify: `tests/test_penalty_shell_contract.py`
- Modify: `tests/test_penalty_audio.c`

1. Add contract assertions that the Penalty path does not enter landscape and maps physical UP/DOWN directly to model UP/DOWN.
2. Run the shell/audio tests and confirm the old rotation path fails the new contract.
3. Remove the enter-landscape branch and rotation-failure recovery. Retain portrait restoration on stop as an idempotent safety action.
4. Update cover-to-title audio transition naming and re-run the focused tests.

### Task 3: Export portrait pitch art and recalibrate the scene

**Files:**
- Modify: `tools/convert-penalty-gameplay.py`
- Modify: `tools/style-penalty-art.py`
- Modify: `assets/images/penalty/gameplay-v2/manifest.json`
- Create: `assets/images/penalty/gameplay-v2/background-240x240.png`
- Regenerate: `assets/images/penalty/gameplay-v2/gameplay_assets.c`
- Regenerate: `assets/images/penalty/gameplay-v2/gameplay_assets.h`
- Modify: `main/penalty_scene.c`
- Modify: `tests/test_penalty_art_contract.py`
- Modify: `tests/test_penalty_scene.c`

1. Write failing asset and scene assertions for the 240-pixel background, portrait target centers, player/ball bounds, and upper/lower keeper contacts.
2. Produce the new background deterministically by center-cropping the approved styled stadium; preserve horizontal grass bands, goal geometry, dark boards, and the black keeper sprites.
3. Update the converter manifest and generated const assets without runtime decoding or scaling.
4. Recalibrate target, keeper, striker, ball, shadow, and dirty-region coordinates and make all scene tests pass.

### Task 4: Reflow every LVGL state to 240 x 320

**Files:**
- Modify: `main/penalty_ui.c`
- Modify: `main/penalty_i18n.c`
- Modify: `assets/images/penalty/cover-240x320.png`
- Regenerate: `assets/images/penalty/cover_rgb565.c`
- Modify: `tests/penalty_preview/preview.c`
- Modify: `tests/test_penalty_i18n.c`
- Modify: `tests/test_penalty_art_contract.py`

1. Add failing preview/contract checks that all states use portrait rotation, the cover has no rotation instruction, and title/settings/help/gameplay/summary text fits 240 pixels.
2. Keep the screen at 240 x 320, reposition the field/HUD, reduce the meter to 192 pixels, and replace hard-coded landscape widths with portrait constants.
3. Change localized help and cover prompts to UP/DOWN portrait instructions while reusing the existing Chinese subset where possible; regenerate the font only if a required glyph is absent.
4. Update the cover prompt deterministically and regenerate its RGB565 source.
5. Run the actual LVGL preview for both languages, all six targets, outcomes, and 50 entry/exit cycles.

### Task 5: Align product documentation and validation

**Files:**
- Modify: `penalty/README.md`
- Modify: `penalty/README.zh_CN.md`
- Modify: `penalty/docs/game-design.md`
- Modify: `penalty/docs/game-design.zh_CN.md`
- Modify: `penalty/docs/validation.md`
- Modify: `penalty/docs/validation.zh_CN.md`
- Modify: `assets/README.md`
- Modify: `assets/README.zh_CN.md`

1. Replace landscape instructions and historical current-state wording with the accepted portrait behavior while preserving older version records as history.
2. Record exact host-render evidence, artifact identity, and the remaining device checks.
3. Run repository link/language checks and `git diff --check`.

### Task 6: Complete validation and firmware packaging

**Files:**
- Generated, not committed: `build/FoloToy-AI-Passport-full.bin`
- Generated, not committed: the versioned portrait acceptance image

1. Run focused model, scene, localization, art, shell, and LVGL preview checks during implementation.
2. Run `./tools/validate.sh --static`.
3. Activate ESP-IDF 5.5.3 and run `./tools/validate.sh`.
4. Verify the merged image size, offset, partition fit, and SHA-256; keep binaries out of Git.
5. Report Build, Host tests, Device tests, and Unverified separately. Offer flashing only after implementation is complete; flashing still requires explicit approval.

Git commits in this plan are checkpoints only and require separate user authorization under the repository rules.
