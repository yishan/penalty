<p align="right">
  <a href="2026-09-23-shootout-mode-design.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Penalty Shootout Mode Design

Date: 2026-09-23 · Status: approved for implementation · Target: v1.5.0

## Goal and scope

Add a primary Shootout mode in which the player alternates between shooting and goalkeeping against AI. The match follows a best-of-five penalty shootout, ends early when the trailing side can no longer draw level, and enters uncapped paired sudden death after a tie. The existing standalone modes remain available and are renamed Shoot Practice and Keeper Practice without changing their five-attempt rules.

The title menu becomes Shootout, Shoot Practice, Keeper Practice, Settings, and Help. Difficulty, sound, and language remain shared. Shootout scores are session-only and are not persisted. Networking, multiplayer, player identities, rankings, fake striker cues, and a complete unbounded kick history are outside this version.

## Coin toss and turn order

Selecting Shootout opens an original pixel coin screen. OK starts the toss. The model chooses and locks one of two equally likely faces before animation begins:

- Football: the player shoots first and AI keeps first.
- Glove: AI shoots first and the player keeps first.

The coin rotates for about one second using a fixed set of front, edge, and back frames. Animation timing, dropped frames, repeated input, or resynchronization cannot change the locked result. UP and DOWN do nothing on this screen. Short presses are ignored while the coin is moving; long OK still abandons the session safely.

After landing, the page states whether the player shoots or keeps first. A second OK starts the match. Every later role change uses the same explicit handoff rule: show the current score, round, and next player role, then wait for OK. The goalkeeper cue timer starts only after that confirmation, so an automatic transition cannot hide the AI striker's directional cue.

## Match rules

The official score counts goals only. Player and AI saves are secondary statistics and are never added to the score. The regular stage gives each side at most five kicks. The starter remains first throughout every paired round.

After every settled kick, calculate each side's remaining regular kicks independently:

```text
player_remaining = 5 - player_attempts
ai_remaining = 5 - ai_attempts
```

The match ends immediately when either side's lead is strictly greater than the trailing side's remaining kicks. A lead equal to the remaining kicks is not enough because a draw is still possible. This check is valid after either member of a regular pair, including matches where the second side no longer needs its final kick.

If both sides complete five kicks with equal scores, enter sudden death. A sudden-death round always contains one kick by each side in the original order. It can end only after both kicks are settled and exactly one side scored. The first kick of a sudden-death pair can never decide the match by itself. There is no gameplay round limit: paired rounds continue until a winner exists.

Long OK abandons the match without recording a winner. Retry starts a new match and therefore returns to the coin toss.

## Difficulty and kick behavior

One selected difficulty applies to the entire match and remains unchanged in sudden death.

- On player shooting turns, retain the current six-cell target cycle, timing meter, green-zone widths, goalkeeper prediction, and calibrated scoring rules.
- On player goalkeeping turns, retain the current truthful blue-striker cue, six-cell dive selection, timing meter, and save table. Easy, Normal, and Hard keep their 900 / 600 / 350 ms cue durations.
- Difficulty does not secretly improve AI random rolls or let AI revise a locked direction after player input.

Every kick locks its direction, timing zone, perfect line, and outcome roll exactly once. The existing red player striker, blue AI striker, black goalkeeper, ball, field, animation, and short sound cues are reused. Shoot Practice and Keeper Practice continue through their current independent five-attempt paths and do not enter coin-toss, handoff, score, early-finish, or sudden-death states.

## Model and fixed-memory architecture

Separate the current overloaded mode concept into:

- `session_kind`: Shootout, Shoot Practice, or Keeper Practice.
- `turn_role`: the player is currently shooting or goalkeeping.

Add explicit coin-ready, coin-spin, coin-result, handoff, and match-summary states around the existing aim, charge, flight, and result states. The pure-C model owns the starter, current role, regular attempts, goals, saves, sudden-death flag and round number, current pair results, and final match result.

Uncapped sudden death must not cause unbounded allocation. Store aggregate counters, the current kick, current sudden-death pair, and a fixed-size ring buffer of recent outcomes. Reuse all LVGL objects, timers, animation objects, and audio workers between kicks. Use wide counters for attempts, scores, and rounds; no gameplay-reachable path may write past the existing five-kick practice record.

Settlement remains exactly once. On a player shooting turn, a successful shot adds one player goal; a failed shot may add an AI save statistic. On a player goalkeeping turn, a failed save adds one AI goal; a catch or parry adds one player save. Misses are not credited as goalkeeper saves. The model evaluates early finish or pair completion before requesting the next handoff.

## UI, art, audio, and copy

The five title rows use short labels that fit the portrait safe area. During regular play, the header shows the stage, paired round, and score, for example `REG 2/5  YOU 2:1 AI`. Two rows of five compact marks show goals and failures. In sudden death, the header changes to an `SD` round counter and the result strip shows only the current pair instead of growing indefinitely.

The coin is a 48–56 pixel original gold token with a football face, glove face, and readable edge frames. It uses one reusable image object rather than full-screen frames. Optional spin and landing cues are short bounded audio events; audio failure leaves the toss fully playable.

The handoff page shows score, regular or sudden-death round, the next role, and a press-OK prompt. The final page shows win or loss, final score, whether regular play ended early or which sudden-death round decided the match, player goals, and player saves. It offers Retry and Title. It does not attempt to render an unlimited historical list.

All new localized copy must be present in both languages, included in the generated font subset, and checked with actual LVGL text widths. Layout checks must include five title rows, two-digit scores, double-digit sudden-death rounds, and the rounded-corner safe area.

## Failure and lifecycle behavior

- Coin result and kick decisions are generated once and survive delayed frames or input resynchronization.
- A stale press from the previous page cannot throw the coin, confirm the result, or begin the next role.
- Leaving during coin animation, handoff, aim, charge, or result cancels the active match and advances the input epoch before rebuilding the cover.
- Queue pressure drops obsolete audio and UI events rather than replaying them in a later kick.
- UI allocation or worker startup failure follows the existing safe fallback; audio failure never blocks play.
- Teardown stops timers and event producers, rejects old-session events, stops workers, and only then deletes UI objects.

## Validation and milestones

### M0 — design

Maintain this paired design and align the main game outline. No code, asset, or firmware behavior changes are part of M0.

### M1 — pure-C rules

Implement the new model states and fixed-memory match data. Host tests cover both starters, all regular turn sequences, early-finish boundaries, equal-lead non-termination, five-kick ties, pair-complete sudden-death decisions, first-kick non-decisions, retry, abandon, duplicate events, resynchronization, and deterministic coin results. A stress test runs at least 1,000 tied sudden-death pairs without allocation growth or record overflow. Existing practice tests remain unchanged and passing.

### M2 — UI loop

Add the five-row title, coin pages, handoff, match header, regular result rows, sudden-death pair display, and match summary. Desktop LVGL renders cover both languages, both coin faces, both starters, early regular wins, two-digit scores, and sudden-death wins.

### M3 — presentation

Add deterministic coin frames and optional bounded toss cues. Verify frame bounds, face readability, animation/result agreement, font coverage, localized widths, and repeated page entry/exit in the existing memory pool.

### M4 — firmware and device acceptance

Run the complete static, host, firmware, and merged-image gates. On device, verify both coin faces, physical input handoff, cue visibility after confirmation, early finish, at least one sudden-death match, two-digit score layout, audio timing, long-OK exit, repeated matches, heap stability, stack headroom, and endurance. Build success and desktop rendering are not device acceptance.
