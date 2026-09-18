<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Penalty

A three-button, offline penalty-kick game for the [FoloToy AI Passport](https://github.com/folotoy/ai-passport) wearable. Five kicks per session, six goal cells, and a timing meter that decides how good your shot is.

Turn the device clockwise, press OK, and the portrait cover gives way to the game.

## How to play

Power on to the portrait cover, turn the device clockwise so its top points right, and press OK.

The title screen offers Play, Settings and Help; UP/DOWN moves and OK selects.

Every kick takes two presses:

1. **Aim.** UP advances clockwise around the six goal cells, DOWN reverses. The ring runs top-left → top-center → top-right → bottom-right → bottom-center → bottom-left and wraps at both ends. Press OK to lock the target and start the power meter.
2. **Shoot.** The meter sweeps 0 → 100 → 0. Press OK again to stop it and strike the ball.

The meter runs at 8 ms per value but slows to 40 ms inside the green band, so the good window stays readable instead of blurring past.

Long OK leaves the game from any screen and returns to the cover.

## Rules

Difficulty changes only how wide the green band is. The meter timing and the probabilities stay the same.

| Difficulty | Green width | Full meter cycle | Perfect window per crossing |
| --- | --- | --- | --- |
| EASY (default) | 20 values | 2,880 ms | 80 ms |
| NORMAL | 11 values | 2,304 ms | 80 ms |
| HARD | 6 values | 1,984 ms | 80 ms |

Where the second press lands decides everything:

| Power | Result |
| --- | --- |
| 0–29 | SAVE — too weak, the keeper collects it |
| 91–100 | MISS — over the bar |
| The two-value dark line inside the green band | Precision shot, 90% average goal rate |
| The rest of the green band | 65% average goal rate |
| 30–90 outside the green band | 25% average goal rate |

The goal is six cells in two rows, and the keeper independently predicts one of them. The relation between the keeper's cell and yours is calibrated so that a uniform prediction produces those averages for every target you might pick. They are long-run probabilities, not per-session quotas: a run of five can go anywhere.

The green band, the keeper's hidden choice and the outcome roll are all drawn once when a kick starts and are never rerolled — a dropped input resyncs to the same kick rather than changing your fate. The animation shows the result that was already decided the moment you pressed.

Five kicks lead to a summary with your goals, each outcome and your precision hits, then Retry or Title.

## Settings

| | |
| --- | --- |
| Difficulty | Cycles EASY → NORMAL → HARD |
| Sound | Mutes and unmutes, with a cue at each end |
| Language | Switches English / 简体中文 immediately |
| Back | Returns to the title screen |

Language is the only setting that survives a restart. Scores, difficulty and sound are session-scoped by design, and retry keeps your difficulty.

Help explains the controls in-game.

## Sound

Short ES8311 cues for the kick, the glove and the goal, capped well under a second, at 16 kHz mono. The audio thread is a bounded worker: if it falls behind, obsolete cues are dropped rather than queued up, and if audio fails to start at all the game stays fully playable in silence.

## Building

This is an application layer rather than a standalone project — `main` needs the `bsp` component, and the build needs the baseline's CMake project, partition table and dependency lock. Lay these files over a checkout of the baseline first:

```bash
git clone https://github.com/folotoy/ai-passport.git
cd ai-passport
/path/to/this/repository/apply.sh .
```

Then build. ESP-IDF 5.5.3, ESP32-C3 with 8 MB Flash:

```bash
source /path/to/esp-idf-5.5.3/export.sh
./tools/validate.sh --static     # repository checks and host tests
./tools/validate.sh --firmware   # ESP-IDF build and merged-image verification
```

The firmware stage produces `build/FoloToy-AI-Passport-full.bin`, which is flashed at offset `0x0`. The browser flasher at <https://ai-passport.folotoy.cn/tools/web-flasher/> will do that without uploading anything.

There is also a desktop harness that renders the real UI through LVGL 9.5.0, useful for checking layout without hardware:

```bash
./tools/preview-penalty.sh /path/to/lvgl-9.5.0
```

`MANIFEST.md` lists exactly which baseline files this layer replaces.

The game builds, its host tests pass, and the v1.1 image has been flashed and played on a real device.

## Layout

```text
main/penalty_*.{c,h}         rules, scene, UI, localisation, audio, app adapter
main/main.c                  portrait cover -> game -> cover
assets/{images,fonts,music}/penalty/
                             artwork, font subset, sound effects
tests/                       host tests and the desktop LVGL render harness
tools/                       asset generation, preview, validation gate
penalty/docs/                design, localisation and validation records
```

`main/penalty_model.c` carries the rules as plain C with no ESP-IDF or LVGL dependency, which is why the probabilities can be tested on a host.

## Licence

MIT, with the FoloToy baseline's copyright notice kept in [LICENSE](LICENSE). The Chinese subset is generated from Source Han Sans SC under the SIL Open Font License, which is bundled beside it; the sound effects are CC0; the artwork was generated with an AI image tool. Details are in [`assets/README.md`](assets/README.md).
