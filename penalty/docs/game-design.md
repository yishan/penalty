<p align="right">
  <a href="game-design.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Penalty — Game Design Outline

Version: 1.4 · Date: 2026-09-23 · Stage: portrait shooting and standalone goalkeeper challenges integrated locally. [Validation status](validation.md) separates host, firmware, and device evidence. The [localization design](localization.md) records the language menu, persistence, font subset and overflow policy.

## 1. Concept and player goal

An offline pixel-art penalty game for short breaks with AI Passport. The player chooses either a shooting challenge or a standalone goalkeeper challenge, selects one of six targets with three buttons, times the action, and receives a clear result. The intended session lasts about 30–90 seconds; this is a playtest target, not a time limit.

The core tension is choosing a direction while stopping the power marker in its sweet spot. A perfect kick should visibly reward timing instead of making every result a random guess.

The brief builds on the penalty/Pong feasibility discussion in Codex task `01a0a2a6-7e5c-7dd2-af45-4c5d90ef5403`. Version 0.2 introduced the earlier landscape flow; version 0.3 added the approved portrait pixel-art boot cover and standalone game shell; version 0.9 organized difficulty, sound and language under Settings with a persistent English / Simplified Chinese choice. Version 1.3 replaced the landscape flow with an upright 240 × 320 experience and vertical target scanning; version 1.4 adds the standalone goalkeeper challenge. No Pong prerequisite is needed.

## 2. Scope and releases

| Stage | Included | Completion condition |
| --- | --- | --- |
| MVP: shooting challenge | Offline single player; six upper/lower targets; two-press power meter; six-target AI keeper; five shots; result summary; replay; mute; short effects | A complete five-shot session runs on device with clear input and outcomes |
| v1.4: goalkeeper challenge | Five AI shots; truthful blue-shooter cues; player selects the black keeper's dive and confirms it within a visible timing window | Six cues, timing bands, outcomes, animation, audio and bilingual UI pass host/render checks |
| Later: alternating shootout | Player shoots and keeps against AI; five paired rounds; early victory and sudden death | Turn order and winner rules pass host tests |
| v0.4: difficulty | Easy / Normal / Hard; random green zones; a two-value guaranteed-goal line; slow green meter | Automated rule/render checks plus pending device timing acceptance |
| v1.0: six-target rules | Two rows of three targets; ring navigation; six-target keeper; 90% / 65% / 25% calibrated average goal rates | Exhaustive model probabilities and actual LVGL target/trajectory checks pass |
| Optional | Local personal best, extra art | Added only after input, memory, and frame timing are measured |

MVP has no opponent score, win/loss, or sudden death: it is a five-shot challenge scored 0–5. A 3/5 result ends the session; it is not a draw. In alternating shootouts, each side gets up to five kicks; stop when the trailing side cannot catch up. If tied after five kicks each, play one kick per side until scores differ after a complete pair. A first kick alone cannot decide a sudden-death pair.

Not included: free-roaming football, 3D physics, networking, leaderboards, multiplayer, voice control, background music, and hardware changes.

## 3. Hardware and design boundaries

| Current baseline | Design consequence |
| --- | --- |
| ESP32-C3, 8 MB Flash, no PSRAM | Small objects and compact assets; no full-screen framebuffer allocation by the game |
| Physical panel: 240 × 320 RGB565; 40 MHz SPI | Native 240 × 320 game coordinates; local updates; provisional 20 FPS animation target |
| Current LVGL single buffer: 20 display rows | Preserve the buffer initially; measure refresh cost before expanding it |
| UP / DOWN / OK on one ADC ladder | Sequential single-button actions; no simultaneous presses or held-direction requirement |
| BSP already exposes PRESS, CLICK, DOUBLE, LONG | Use PRESS for timing-sensitive actions; do not count the subsequent CLICK/DOUBLE again |
| ES8311 audio support | Short queued effects; audio failure must not block a kick |

Hardware/API references: [pin definitions](../../components/bsp/include/bsp_pins.h), [display implementation](../../components/bsp/src/bsp_display_lvgl.c), [button API](../../components/bsp/include/bsp_button.h), and [product specifications](../../docs/hardware-design/specifications.md). The earlier discussion understated existing PRESS support; the current header and implementation already provide it. FPS, latency, free heap, and battery runtime remain unmeasured.

## 4. Controls

### Portrait entry

On power-on, show the approved 240 × 320 pixel cover: PENALTY title, the matching cream/dark-shadow Chinese game mark, player facing the goal, horizontal grass bands, a black-kit keeper, and `PRESS OK TO START`. The cover deliberately has no battery overlay. The device stays upright when confirmation opens the 240 × 320 title screen; live battery text appears in the title and later game screens. The title page shows the field and menu without the former English/Chinese slogan.

Consume the entry OK event so it cannot also start the game. Keep the title, settings, play screen, results, retry flow, and cover in portrait. Retry does not repeat the prompt. On exit, stop the game, reset the input epoch, and return directly to the portrait cover. The stop path may idempotently request portrait as a safety action, but gameplay never enters landscape. There is no hardware-demo launcher in the normal flow.

### Button mapping

UP and DOWN scan the selected target vertically; OK confirms. Each attempt starts at top-center. DOWN cycles `top-center → bottom-center → top-left → bottom-left → top-right → bottom-right → top-center`; UP follows the exact reverse. The two cells in a column therefore follow the physical vertical direction before selection advances to the next column. The title menu contains Shoot, Keep, Settings and Help. Settings contains Difficulty, Sound, Language and Back. DOWN selects the next menu option and UP the previous one.

| Screen/state | UP / DOWN | Short OK | Long OK |
| --- | --- | --- | --- |
| Portrait cover | No action | Open portrait menu | Reset to portrait cover |
| Title | Select Shoot, Keep, Settings, or Help | Activate selected item | Return to portrait cover |
| Settings | Select Difficulty, Sound, Language, or Back | Cycle/toggle selected setting, or return | Return to portrait cover |
| Help | No action | Return to portrait menu | Return to portrait cover |
| Aim | Scan the six kick/dive targets by column in either direction | Lock target and start timing meter | Exit current session |
| Charge | No action | Lock timing and kick/dive | Exit current session |
| Flight/result | No action | No action; automatic transition | Exit current session |
| Summary | Select Retry or Title | Activate selection | Return to portrait cover |

Charge uses two separate presses, not press-and-hold. One physical press produces one game action. The entry press must not also start a shot; stale queued inputs must not advance a new screen. For menus, delayed CLICK is acceptable; use PRESS with event timestamps for aim/charge timing, ignore its later CLICK/DOUBLE, and handle LONG as the exit command. An OK press may start a transient action before it becomes a long press; exit cancels the session and discards the outcome. No pause feature in MVP.

Respect the existing long-OK exit convention. Future input integration must explicitly route PRESS to the game and test navigation ownership rather than assuming the header alone guarantees correct screen dispatch.

## 5. Shooting rules — v1.0 calibrated probabilities

1. Start each session with zero goals and five empty result slots.
2. On entering aim for each new kick, generate its green zone, inner dark line, uniformly selected keeper target among the same six cells, and one 0–99 outcome roll. Show the zones and target grid before charging; keep the keeper target and roll hidden. Never reroll during aim, charge, or late-input recovery. Lock the chosen target on starting charge.
3. The meter runs 0 to 100 and back, with 8 ms per ordinary value and 40 ms per green value (one-fifth speed), symmetrically in both directions. Derive power from elapsed monotonic time, not frame count. No lock within 4,000 ms is a timeout miss, consuming one shot. Aim itself has no timeout.
4. On the second OK, lock power and determine the outcome once using this table; endpoints are inclusive.

| Power | Outcome |
| --- | --- |
| 0–29 | SAVE: weak kick, keeper collects regardless of direction |
| Dark-green line (2 values within green) | Precision attempt; average final GOAL rate 90% across six equally likely keeper targets |
| Rest of green | Average final GOAL rate 65% across the six keeper targets |
| 30–90 outside green | Average final GOAL rate 25% across the six keeper targets |
| 91–100 | MISS: overpowered kick clears the goal |

| Difficulty | Green width (inclusive values) | Full meter cycle | Perfect window per crossing |
| --- | --- | --- | --- |
| EASY (default) | 20, originally proposed as 61–80 | 2,880 ms | 80 ms |
| NORMAL | 11, originally proposed as 70–80 | 2,304 ms | 80 ms |
| HARD | 6, originally proposed as 75–80 | 1,984 ms | 80 ms |

The original ranges still define widths, not fixed locations. Each green zone is entirely within 30–90; consecutive same-width shots use different starts. The two-value dark line is randomly positioned inside it, including either edge. Hitting this line increments the perfect/precision count whether the shot scores or is exceptionally saved. The keeper relation table below is calibrated so a uniform six-target prediction produces the requested averages for every player target; these are long-run probabilities, not per-session quotas.

| Power tier | Keeper exact cell | Same column, other row | Any other column | Six-target average |
| --- | ---: | ---: | ---: | ---: |
| Dark line | 75% goal | 85% goal | 95% goal | 90% goal |
| Rest of green | 30% goal | 40% goal | 80% goal | 65% goal |
| Outside green, power 30–90 | 0% goal | 10% goal | 35% goal | 25% goal |

Difficulty changes only the visible green-zone width. Meter timing remains 8 ms per ordinary value and 40 ms per green value; no hidden speed or probability penalty is added on harder levels.

These are arcade rules, not a real-football simulation. Show all six goal cells, the selected bright frame, the green zone, an outlined dark-green line, and the red 91–100 zone. The cursor brackets the meter without obscuring the line. Goals send the ball to the selected upper/lower cell while the keeper follows its hidden prediction; saves end at a glove contact in the actual ball cell. A dark-line save uses the save sound and a dedicated result. Animation explains the precomputed result, never decides it through visual collision.

Difficulty is selected before PLAY and stays fixed during a session. Retry and return-to-title retain it; returning to the portrait cover or restarting resets to EASY. Mute follows the same lifetime. Device playtesting must validate the small line and 80 ms timing window with the existing 50 ms UI update cadence; host arithmetic is not a responsiveness measurement.

5. Animate the ball/keeper for approximately 600 ms, then show the result and reason for 900 ms. For timeout, show the result directly. Record a shot exactly once. After shots 1–4 return to aim; after shot 5 show summary.
6. Summary shows goals/5, the five outcomes, dark-line hit count, and Retry/Title. No automatic replay.

Feedback is localized, including goal, perfect, ordinary-green goal, green save, weak save, save, high miss, and timeout. Both languages use concise labels sized against the real LVGL layout. Simplified Chinese is rendered by a tracked Source Han Sans SC subset containing only the required game glyphs.

The lower copy line is instructional only in EASY or on the first kick. Later NORMAL/HARD kicks use one stable, non-repeating encouragement line per kick; the result footer uses outcome-specific light commentary. Copy never changes during charge and does not consume gameplay randomness. Exact strings and boundaries are recorded in the [dynamic copy design](plans/2026-09-16-dynamic-copy-design.md).

Keeper target uses a seedable generator in pure logic and is fixed before the player commits target or power. Tests supply fixed seeds/decisions; device integration supplies a session seed. More difficult AI must not inspect a committed player choice and silently alter its decision.

### 5.1 Standalone goalkeeper challenge — v1.4

The goalkeeper challenge uses the same portrait camera behind the shooter. The AI shooter wears blue and stays in the foreground; the player controls the distant black-kit keeper. At the start of each attempt, the blue shooter's foot/hip/body pose truthfully cues one locked shot target for 900 ms on Easy, 600 ms on Normal, or 350 ms on Hard. The cue is shown once, then the shooter returns to ready; there are no false cues or post-input target changes.

The player scans the same six cells, presses OK to lock a dive and start the timing meter, then presses OK again to dive. Green widths, two-value dark line, 0–100 meter path, five-attempt session, 4,000 ms timeout, and 600 ms presentation timing match the shooting challenge. The outcome is settled once from the preselected AI target, player dive, timing value, and preselected 0–99 roll:

| Timing tier | Exact cell | Same column, other row | Different column |
| --- | ---: | ---: | ---: |
| Dark line | 90% save | 35% save | 0% save |
| Rest of green | 65% save | 20% save | 0% save |
| Outside green, 30–90 | 25% save | 5% save | 0% save |

Values 0–29 are `TOO EARLY`; values 91–100 and the meter timeout are `TOO LATE`. Successful exact-cell reads use `CAUGHT!`; successful same-column coverage uses `PARRIED!`. A different column is `WRONG WAY`; a correct column that loses the probability roll is `READ IT - MISSED`. The summary reports saves out of five and retains goalkeeper mode on Retry.

Runtime art uses five red-shooter frames for shooting mode, nine blue-shooter frames for goalkeeper mode (ready, contact, follow-through, six direction cues), and seven black-keeper frames (ready plus all six dive cells). These are real transparent sprites in Flash, not full-screen frame sequences. The animation visualizes the precomputed decision; collision does not change the result.

## 6. Screens and state flow

```text
POWER ON → PORTRAIT COVER → PORTRAIT MENU (SHOOT / KEEP / SETTINGS / HELP)
SETTINGS (DIFFICULTY / SOUND / LANGUAGE / BACK) → TITLE
HELP → TITLE (OK)
SHOOT or KEEP → AIM → CHARGE → FLIGHT → RESULT → AIM (attempts 1–4)
                 └─ timeout ──→ RESULT → SUMMARY (shot 5)
SUMMARY → AIM (Retry same mode, reset session) / TITLE
Any active state → EXITING → cover
```

Both flight and timeout use the same result-to-next-shot/summary decision. Result accounting is idempotent. Exit stops production of events before destroying the view.

Implemented 240 × 320 portrait layout:

| Region | Vertical bounds | Content |
| --- | --- | --- |
| Header | y=0–17 | Difficulty, attempt 1/5, goals or saves, battery |
| Field | y=18–182 | Illustrated stadium, six target hints, red/blue striker, black keeper, independent ball and shadow |
| Power/result | y=205–226 | 192-pixel meter, best band, marker or result text |
| Footer | y=235–319 | Target/action text, interaction copy, long-OK hint |

Keep critical labels and controls clear of the rounded corners, normally within x=16–223. The pitch uses a native 240 × 240 center crop of the approved stadium; the active field clips it at y=18–182. Three goal columns remain centered at x=70/120/170. No runtime image scaling or full-screen game buffer is used.

Look: dark green pitch, white goal lines, bright ball, original neutral player sprites, readable outlines. Distinguish zones and outcomes by labels/shapes as well as color. Keep the target visible through charge. Use scaling/position to suggest depth without a 3D renderer; avoid camera motion, full-screen effects, real club branding, or player likenesses.

Audio: brief kick, goal, save, and miss cues; cap each at 300 ms initially. Use one worker with a bounded queue; drop obsolete effects on rapid transitions. Sound is on by default with a session-only mute toggle; no microphone use.

## 7. Development structure

`penalty/` is the project planning/design home requested by the user. The firmware remains part of the existing ESP-IDF application, following repository boundaries:

| Planned module | Responsibility |
| --- | --- |
| `main/penalty_model.{c,h}` | Pure C rules, seeded decisions, clock inputs, state transitions, shot records |
| `main/penalty_ui.{c,h}` | LVGL objects, screen layout, animations reflecting model results |
| `main/penalty_app.{c,h}` | Input event routing, lifecycle, clock/seed adapters, audio queue |
| `tests/test_penalty_model.c` | Host tests for rules, timing, transitions, duplicate input and reset |
| `penalty/assets/` (when needed) | Editable game art/sound sources and attribution |

The model does not include ESP-IDF or LVGL headers. Its session data includes mode, state, attempt index, goals/saves, perfect count, player/opponent targets, timing value, timestamps, seed, and five attempt records. Each record includes both targets, timing, outcome roll, and outcome/reason. No player identity or network data.

Use one owner for model changes. Button callbacks enqueue bounded events and return. UI access outside the LVGL task holds `bsp_lvgl_lock()`; audio and language-preference NVS writes run outside that lock in workers. Teardown stops timers, rejects stale session events, flushes the pending language preference, stops/joins workers, and only then deletes UI objects. If a worker cannot stop, retain the view until safe teardown is possible.

The standalone shell in `main/main.c` boots directly into Penalty. Baseline demo source files remain as references but are not linked into this game. Returning to the cover stops the timer and service worker, recreates the screen, advances the input epoch, and restarts services. The approved cover uses a 153,600-byte const RGB565 array in Flash and the gameplay background uses a 115,200-byte const RGB565 array, both with no full-image RAM decode. Source and conversion recipes are recorded in [Assets](../../assets/README.md). No partitions or pins change.

The game keeps the BSP's portrait baseline and does not request a quarter-turn. The partial buffer capacity stays unchanged, rounded-corner clipping continues to use the active 240 × 320 dimensions, and no full-screen rotation buffer is allocated. The stop path retains an idempotent portrait request for recovery safety. Portrait clipping, target readability, and control feel still need physical-device validation.

## 8. Storage, failure handling, and performance

- MVP stores progress, difficulty and mute state in RAM. Exit or power loss discards the session; do not claim resume or saved records. The language preference is the only persistent game setting and is restored from the existing NVS partition.
- Optional best-score storage uses a versioned, compact NVS record updated at session end. NVS failure falls back to session-only scores and a visible unsaved indicator. Partition changes are not required by this design.
- Ignore inappropriate input during flight/result; discard events from older sessions. Queue overflow must not duplicate shots or produce an arbitrary delayed kick; resynchronize to aim before commitment, or complete the already committed result.
- If audio initialization fails, continue silently. If UI allocation fails, stop partial resources and show a minimal error screen with a long-OK retry instead of leaving a broken screen.
- Animation target: 20 FPS with local updates. Timing must stay correct across a delayed frame. Input-to-visible-feedback target: at most 100 ms during active play. Both require on-device measurement.
- Keep a constant number of LVGL objects during play and reuse them between shots. New localized text must pass the generated-font glyph contract and real LVGL width checks before inclusion. Record minimum free heap, largest free block, stack high-water marks, and display update times during device testing; resource budgets follow those results.

## 9. Milestones and acceptance

| Milestone | Deliverable | Required evidence |
| --- | --- | --- |
| M0 — planning | Project folder and bilingual design outline | Paired docs, valid local links, clear proposed/implemented boundary |
| M1 — rules | Pure C model and test runner integration | All powers/direction pairs/rolls; green/perfect boundaries in all three difficulties; random zones; slow ascent/descent; 4,000 ms timeout; delayed frames; five shots exactly; retry/reset; duplicate events |
| M2 — minimal device loop | Portrait cover/title/help/play/summary, six targets, power bar and ball | Device stays upright; UP/DOWN traverse the column-wise target cycle with consistent vertical direction; upper/lower cells and text stay clear; entry press is consumed; PRESS/CLICK/DOUBLE do not double-count; retry stays portrait; long OK returns to the cover from every state |
| M3 — presentation | Approved pixel cover, red/blue shooter frames, six keeper dives, outcome animations, short audio and mute | Both roles match precomputed results; cue readability and animation timing pass full-screen and physical-size review |
| M4 — acceptance | Measured device session and validation report | Complete repository gate; 20 sessions without crash; 50 entry/exit cycles with no progressive heap loss after warm-up; timing and frame measurements |

Before firmware delivery run `./tools/validate.sh --static`, `./tools/validate.sh --firmware`, and `./tools/validate.sh` from the repository root with ESP-IDF 5.5.3. Register new game tests in the shared runner; baseline tests alone do not validate the game. Report Build, Host tests, Device tests, and Unverified separately.

## 10. Decisions to revisit after the first playable build

The implemented baseline now includes separate Shoot and Keep challenges; upright portrait play; a reversible column-wise six-target cycle starting at top-center; timing-based action; five-attempt summaries; neutral pixel art; bilingual UI; persistent language; and RAM-only scores. Shooting retains calibrated 90% / 65% / 25% average goal rates. Goalkeeper mode adds truthful difficulty-timed cues and the exact/same-column save table above. Playtests should determine whether the six blue-shooter cues are readable at native size, whether the six keeper poses and timing feel fair, whether upper/lower cells and Chinese remain legible, and whether 20 FPS feels responsive. Then decide whether alternating shootouts or saved records is the next priority.
