<p align="right">
  <a href="2026-09-23-shootout-mode-implementation-plan.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Shootout Mode Implementation Plan

> **For Codex:** Execute this plan test-first. Preserve both practice modes and do not commit, push, flash, or publish without explicit authorization.

**Goal:** Deliver Penalty v1.5.0 with an official-rule shootout, locked animated coin toss, explicit role handoffs, fixed-memory sudden death, and renamed practice modes.

**Architecture:** Keep the pure-C model authoritative. Split session kind from current player role; store match aggregates, fixed regular-round marks, and a bounded recent-result ring. Reuse existing kick logic, LVGL objects, audio worker, and lifecycle. UI and audio render transitions but never decide results.

## Task 1: Lock rules in host tests

**Files:** `tests/test_penalty_model.c`, `main/penalty_model.h`

1. Define session, role, coin, match-result, bounded-history, and match-counter types.
2. Add failing tests for both coin outcomes, duplicate input, fixed order, early-finish boundaries, equality-not-finished, tied regular time, paired sudden death, retry, abandon, and 1,000 tied sudden-death pairs.
3. Compile and verify the failure is caused by missing behavior.

## Task 2: Implement the pure-C match state machine

**Files:** `main/penalty_model.c`, `main/penalty_model.h`, `tests/test_penalty_model.c`

1. Implement pre-animation coin locking, role order, handoff gating, scoring, early finish, sudden-death pairs, bounded history, retry, abandon, and deterministic resync.
2. Keep both practice paths at five attempts and outside match states.
3. Run the focused model test to green, then refactor without changing behavior.

## Task 3: Adapt scene, copy, and audio contracts

**Files:** `main/penalty_scene.[ch]`, `main/penalty_copy.c`, `main/penalty_audio.[ch]`, and their host tests.

1. Add failing tests for turn-role scene selection, isolated practice history, and bounded coin spin/landing audio.
2. Replace overloaded mode checks with explicit session/role helpers.
3. Run all focused tests to green.

## Task 4: Add bilingual UI and glyphs

**Files:** `main/penalty_i18n.[ch]`, `main/penalty_ui.c`, localization/font tests, generated Chinese fonts.

1. Add failing tests for the five-row menu, coin, handoff, regular/sudden-death header, and match-summary copy.
2. Implement the menu, procedural pixel coin, role-result and handoff pages, bounded score marks, and match summary.
3. Regenerate font subsets with `python3 tools/generate-penalty-fonts.py`; validate glyph coverage and widths.

## Task 5: Complete lifecycle and desktop rendering

**Files:** `main/penalty_app.c`, `tests/penalty_preview/preview.c`, relevant contract tests.

1. Reuse the current screen, timer, event epoch, and audio worker.
2. Ensure long OK exits every match state without recording a winner.
3. Render representative English/Chinese coin, starter, regular, double-digit sudden-death, and summary frames; inspect safe areas and alignment.

## Task 6: Align version and documentation

**Files:** `penalty/VERSION`, bilingual Penalty README and game-design documents.

1. Set version `1.5.0`.
2. Align bilingual rules, controls, mode names, and validation status; do not edit repository changelogs.
3. Run `git diff --check` and verify reciprocal language links.

## Task 7: Validate and hand off

1. Run `./tools/validate.sh --static` and `./tools/validate.sh --firmware` with ESP-IDF 5.5.3.
2. Verify `build/FoloToy-AI-Passport-Penalty-v1.5.0-full.bin` and merged-image checks.
3. Report build, host, device, and remaining checks separately; ask before flashing; do not commit or push unless requested.
