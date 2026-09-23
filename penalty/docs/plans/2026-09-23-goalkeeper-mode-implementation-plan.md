<p align="right">
  <a href="2026-09-23-goalkeeper-mode-implementation-plan.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Goalkeeper Mode Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a selectable, deterministic five-kick Keeper Challenge with truthful blue-striker cues, six-cell player dives, timing-based save odds, complete pixel animation, bilingual UI, and verified firmware.

**Architecture:** Extend the pure-C Penalty model with a role-neutral mode and mode-specific judging while reusing its six-cell navigation, meter, event timing, and exactly-once settlement. Keep rendering in the existing LVGL scene/UI layer and store all new native-size sprites as const RGB565A8 Flash data.

**Tech Stack:** C11 host model, ESP-IDF 5.5.3, LVGL 9.5, Pillow asset conversion, native RGB565A8 sprite data, shell/Python validation.

---

### Task 1: Document the approved design

**Files:**
- Create: `penalty/docs/plans/2026-09-23-goalkeeper-mode-design.md`
- Create: `penalty/docs/plans/2026-09-23-goalkeeper-mode-design.zh_CN.md`
- Modify: `penalty/docs/game-design.md`
- Modify: `penalty/docs/game-design.zh_CN.md`

Write the agreed viewpoint, menu, six-cell controls, cue durations, save table, asset inventory, state flow, and validation boundary. Run `./tools/validate.sh --static`; expect paired-document and link checks to pass.

### Task 2: Add mode and goalkeeper rules with TDD

**Files:**
- Modify: `tests/test_penalty_model.c`
- Modify: `main/penalty_model.h`
- Modify: `main/penalty_model.c`

Add failing tests for the four-item title menu, Keeper mode start, locked opponent target, cue expiry, save probability table, early/late outcomes, save counting, replay, and deterministic sessions. Run the focused host compile from `tools/validate.sh` and confirm RED failures. Add the smallest model API and implementation, then rerun until `Penalty model: PASS`.

### Task 3: Add bilingual copy and font coverage with TDD

**Files:**
- Modify: `tests/test_penalty_i18n.c`
- Modify: `tests/test_penalty_copy.c`
- Modify: `main/penalty_i18n.h`
- Modify: `main/penalty_i18n.c`
- Modify: `main/penalty_copy.c`
- Regenerate: `assets/fonts/penalty/penalty_font_zh_14.c`
- Regenerate: `assets/fonts/penalty/penalty_font_zh_20.c`

Add failing expectations for Shooting Challenge, Keeper Challenge, save count, catch, parry, conceded goal, wrong way, correct read, early, late, and keeper instructions. Implement bounded English/Chinese strings, regenerate fonts, and run copy/i18n/font tests until PASS.

### Task 4: Create and convert action sprites

**Files:**
- Add/modify: `assets/images/penalty/gameplay-v2/*striker*.png`
- Add/modify: `assets/images/penalty/gameplay-v2/*keeper*.png`
- Modify: `assets/images/penalty/gameplay-v2/manifest.json`
- Modify: `tools/convert-penalty-gameplay.py`
- Regenerate: `assets/images/penalty/gameplay-v2/gameplay_assets.c`
- Regenerate: `assets/images/penalty/gameplay-v2/gameplay_assets.h`
- Modify: `tests/test_penalty_art_contract.py`

Generate source sheets with genuine alpha, extract native 88×88 striker and 56×56 keeper frames, and record prompts and anchors. First change the contract test to require 5 red, 9 blue, and 7 keeper frames and observe RED. Convert assets, inspect every native-size frame, and run `python3 tools/convert-penalty-gameplay.py --check` plus the art contract until PASS.

### Task 5: Render both modes and action timing with TDD

**Files:**
- Modify: `tests/test_penalty_scene.c`
- Modify: `main/penalty_scene.h`
- Modify: `main/penalty_scene.c`
- Modify: `main/penalty_ui.c`
- Modify: `tests/penalty_preview/preview.c`

Add failing scene tests for red ready/shift/backswing/contact/follow-through, six blue cue frames and expiry, six keeper endpoints, incoming Keeper-mode ball scale, and target visibility. Implement role-specific scene composition and UI summary/menu layouts. Run scene tests, then render all preview cases and inspect PNG output.

### Task 6: Integrate lifecycle and audio

**Files:**
- Modify: `main/penalty_app.c`
- Modify: `main/penalty_audio.c`
- Modify: `tests/test_penalty_shell_contract.py`

Add failing shell checks for mode-safe event routing, teardown, and no new blocking button work. Reuse the existing shoot/save/goal cues where appropriate; do not add a worker or persistence path. Run focused shell and static checks until PASS.

### Task 7: Update authoritative docs and run complete validation

**Files:**
- Modify: `penalty/README.md`
- Modify: `penalty/README.zh_CN.md`
- Modify: `penalty/docs/game-design.md`
- Modify: `penalty/docs/game-design.zh_CN.md`
- Modify: `penalty/docs/localization.md`
- Modify: `penalty/docs/localization.zh_CN.md`
- Modify: `penalty/docs/validation.md`
- Modify: `penalty/docs/validation.zh_CN.md`
- Modify: `assets/README.md`
- Modify: `assets/README.zh_CN.md`

Run `./tools/validate.sh --static`, the actual LVGL preview, `./tools/validate.sh --firmware`, and finally `./tools/validate.sh` with ESP-IDF 5.5.3. Verify the content-addressed firmware archive and report Build, Host tests, Device tests, and Unverified separately. Do not commit, flash, push, or publish unless separately authorized.
