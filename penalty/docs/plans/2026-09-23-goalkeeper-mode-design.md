<p align="right">
  <a href="2026-09-23-goalkeeper-mode-design.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Keeper Challenge Design

Date: 2026-09-23 · Status: approved for implementation

## Goal and scope

Add an optional, standalone five-kick Keeper Challenge beside the existing Shooting Challenge. The camera remains behind the penalty taker: a blue AI striker faces the goal while the player controls the distant black-clad goalkeeper. This preserves the established portrait composition and lets the player read the striker's hips, plant foot, and body lean from behind.

The title menu becomes Shooting Challenge, Keeper Challenge, Settings, and Help. Difficulty, sound, and language are shared. Each mode has its own five-attempt summary; no alternating shootout, sudden death, persistent score, fake cue, or new settings are included.

## Goalkeeper loop

At the start of each kick, the model fixes the AI shot target, timing zone, perfect line, cue pose, and outcome roll. The player starts at top-center and scans the same six cells with UP/DOWN. The AI displays its truthful target cue once: 900 ms on Easy, 600 ms on Normal, and 350 ms on Hard, then returns to ready. The cue never repeats and the AI never changes its target after observing player input.

The first OK locks the dive target and starts the existing 0–100 timing meter. The second OK locks takeoff timing, starts the blue striker's contact animation, and animates the black goalkeeper toward the chosen cell. Values 0–29 are too early and 91–100 too late. For values 30–90, save chances are:

| Dive relation | Perfect line | Green zone | Outside green |
| --- | ---: | ---: | ---: |
| Exact cell | 90% | 65% | 25% |
| Same column, wrong row | 35% | 20% | 5% |
| Different column | 0% | 0% | 0% |

A catch or parry counts as a save. The summary reports saves out of five and distinguishes catches, parries, and goals conceded. Failure feedback identifies wrong direction, correct read but missed save, early takeoff, or late takeoff.

## Art and presentation

The existing pitch, ball, goal, crowd, and portrait layout remain. Red is the player-side striker color in Shooting Challenge; blue is the AI striker color in Keeper Challenge; the goalkeeper stays black in both modes.

- Red player striker: retain three frames and add weight-shift and ball-contact frames, producing a five-frame visible kick.
- Blue AI striker: ready, six truthful directional cues, contact, and follow-through, for nine frames.
- Black goalkeeper: retain ready, left, right, and low-center poses; add high-center, low-left, and low-right so all six target cells have readable motion.

Shooting animation shows ready during target selection, weight shift on meter entry, backswing while the meter runs, contact on the second OK, and follow-through during early flight. Keeper Challenge uses the same contact timing, but its six blue cue poses are gameplay information. Sprite assets remain native-size planar RGB565A8 data in Flash; no full-screen buffer or runtime image scaling is added.

## Model, lifecycle, and acceptance

The pure-C model gains a role-neutral `mode`, `player_target`, `opponent_target`, and `timing_value` interpretation while retaining deterministic RNG and exactly-once settlement. Input callbacks remain non-blocking; UI updates remain in the LVGL task; stale event and teardown behavior remain unchanged. Mode is not persisted.

Host tests must exhaust the save probability table, verify cue duration and one-shot visibility for every difficulty, confirm the AI target is locked before input, traverse both six-cell cycles, and complete five-save and five-concede sessions. Scene tests cover all red and blue frames, ball contact, six keeper endpoints, and frame bounds. The LVGL preview must render both languages, both modes, all six targets, all outcomes, and repeated entry/exit within the current memory pool. Firmware build is not device acceptance; physical cue readability, button feel, frame cadence, audio timing, and endurance remain device checks.
