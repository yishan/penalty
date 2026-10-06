<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Penalty

A three-button portrait penalty-kick game for AI Passport, with offline single-player modes and an experimental two-device BLE duel. Keep the device upright: UP and DOWN scan the six targets vertically by column, and OK confirms.

Status: v2.0.0 is a **multiplayer public beta**. The open-source application layer and downloads belong to [yishan/penalty](https://github.com/yishan/penalty), with versioned assets on the [v2.0.0 GitHub Release](https://github.com/yishan/penalty/releases/tag/v2.0.0). This is separate from the official community, where this version has not been submitted. Shootout supports solo play against AI and nearby two-device play. Shoot Practice and Keeper Practice retain their five-attempt rules. A coin toss selects the first role; players alternate kicks, matches end early when a comeback is impossible, and tied five-round matches continue in paired sudden death. See the [v2.0.0 release notes](docs/releases/v2.0.0.md) and [validation record](docs/validation.md) for the tested scope and remaining checks.

The Shootout submenu offers Single Player, Create Lobby, Join Lobby, and Back. The creator chooses the shared difficulty, both players compare the six-digit security values, and the creator triggers the random coin toss; BLE roles do not determine first shooter. Both players choose blindly without a shooter cue. A powered, authenticated match can recover within 30 seconds; exceeding the deadline ends it without a winner. The creator's shooter wears red, the joiner's wears blue, and the keeper wears black on both screens. Each round contains one kick per player. See the [BLE v2 design and milestone record](docs/plans/2026-09-24-ble-duel-v2-design.md) for exact rules and acceptance. M6 endurance and exception coverage remains incomplete.

## Start here

- [Game design outline](docs/game-design.md): scope, controls, rules, screens, architecture, milestones, and acceptance criteria.
- [BLE v2 duel design](docs/plans/2026-09-24-ble-duel-v2-design.md): lobby/security, blind choices, recovery, milestones and recorded device acceptance.
- [Goalkeeper mode design](docs/plans/2026-09-23-goalkeeper-mode-design.md): approved viewpoint, cue timing, save probabilities, animation set, and acceptance boundary.
- [Shootout mode design](docs/plans/2026-09-23-shootout-mode-design.md): coin toss, alternating roles, early finish, sudden death, fixed-memory model, and acceptance boundary.
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

Power on to the portrait pixel cover with fixed English and Chinese game marks and press OK without turning the device. UP/DOWN select Shootout, Shoot Practice, Keeper Practice, Settings, or Help. Settings contains difficulty, sound, language, and Back. A device with no valid saved preference starts in Simplified Chinese. Choose Language to switch English / Simplified Chinese; the page refreshes immediately and a valid choice persists across game exit and device restart. Choose Difficulty to cycle Easy / Normal / Hard.

Choose Single Player in the Shootout submenu for AI play. The following coin/handoff flow describes play against AI; Create/Join use the separate blind PvP flow above.

Shootout opens a gold pixel coin. Press OK to toss: football means the player shoots first, glove means the player keeps first. The result is locked before the animation. Press OK again to begin, then confirm every attack/defence handoff so the keeper cue never starts off-screen. Each side gets at most five regular kicks, unless the trailing side can no longer catch up. A 5–5-round tie enters paired sudden death; both kicks in the pair must finish before a winner is decided. The header and fixed-size marks show the live score without storing an unbounded history. Retry starts a fresh coin toss.

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
`build/FoloToy-AI-Passport-Penalty-v2.0.0-full.bin`. The embedded application version also reads `penalty/VERSION`. Future releases keep the
same naming pattern and update only the semantic version.

The static gate includes the game model and navigation regression tests. To render the actual UI on a desktop with CMake, a C compiler, and the locked LVGL 9.5.0 sources:

```bash
./tools/preview-penalty.sh /path/to/lvgl-9.5.0
```

The preview script now runs both single-player and multiplayer renderers. Firmware/full gates include these renderers before creating the firmware archive. Multiplayer captures additionally check lobby/security/difficulty, coin, locked waiting, reconnect and win/loss/no-contest panels in both languages; host rendering is not physical acceptance.

The default source path is `managed_components/lvgl__lvgl` after an ESP-IDF build. Captures are written to `build/penalty-preview/*.ppm` (v0.5 uses `build/penalty-art-preview/`); tests check text width, the six-target overlay, green/precision-line pixels, upper/lower keeper contact, foreground occlusion, dirty/full redraw equivalence and 50 sessions/entry-exit cycles in the 24 KB LVGL pool. This is a rendering harness, not a hardware emulator or interactive desktop port.
