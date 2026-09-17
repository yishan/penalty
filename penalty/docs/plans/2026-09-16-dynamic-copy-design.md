<p align="right">
  <a href="2026-09-16-dynamic-copy-design.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Penalty Dynamic Copy Design

Status: approved and implemented locally on 2026-09-16; device readability remains pending.

## Intent

Replace the test-like numeric `LINE n-n = GOAL` footer with short, playful copy without hiding the visible dark-green perfect line or changing shot rules. Copy must remain stable while the player aims and charges, fit the existing English font, and never consume gameplay randomness.

## Rules

- EASY shows `DARK LINE = 90%` on every kick.
- NORMAL and HARD show that teaching line only on the first kick.
- Later NORMAL/HARD kicks choose one stable encouragement line from an eight-line pool. Adjacent interactive kicks cannot repeat. Their fifth kick uses `LAST KICK. NO PRESSURE.`
- Result footer copy is outcome-specific: goal, perfect goal, perfect save, ordinary save, weak shot, high shot, timeout, green goal, and green save each have a distinct line.
- The bottom-most `HOLD OK: COVER` instruction remains available outside aim/charge. Copy does not move, animate, or change while the meter is running.

## Implementation boundary

`main/penalty_copy.c` is a pure-C presentation rule with no LVGL or ESP-IDF dependency. It derives a deterministic prompt from already-generated shot fields and recorded shots, so it does not advance or alter the gameplay random state. `penalty_ui.c` only places the selected string.

Host tests cover the EASY/first-kick teaching boundary, stable selection, adjacent non-repetition, last-kick copy, exact result mapping, and a 28-character limit. The LVGL harness renders normal interactive copy, last-kick copy, and every outcome while checking label widths and 50 lifecycle cycles.
