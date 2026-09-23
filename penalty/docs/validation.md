<p align="right">
  <a href="validation.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Penalty — Validation and Device Checklist

Updated: 2026-09-23. This record distinguishes implementation from physical acceptance.

## v1.4.2 Chinese default, shorter keeper title, and Launcher contract — release candidate

The English title menu now uses `KEEPER CHALLENGE` instead of `GOALKEEPER CHALLENGE`. Simplified Chinese is the default when the language preference is missing, invalid, or unavailable; a valid saved English preference still restores English. The application now also exposes Launcher contract version 1 lifecycle entry points. No gameplay rule, timing, art, audio, or control behavior changes in this revision.

- ESP-IDF 5.5.3 firmware build and merged-image verification: PASS after the Launcher contract integration. The last validated application is 1,487,680 bytes, the merged image is 1,553,216 bytes, and the factory partition remains 82% free.
- Static/host gate: PASS. Focused model, preference, and localization tests cover the Chinese default, invalid-value fallback, retained valid English preference, and exact English menu copy. All repository, scene, font, art, audio, display, and firmware-layout checks also pass.
- Actual LVGL 9.5.0 rendering: PASS for the default Chinese cover/title, the English title containing `KEEPER CHALLENGE`, both language covers, and 50 sessions/entry-exit cycles in the 24 KB pool. No tested label overflows. Free memory after warm-up remains 12,248 bytes and the largest tested moving-frame redraw is 27,640 pixels. Captures are under `build/penalty-preview/`; they are desktop LVGL renders, not device photographs.
- Device verification for v1.4.2: NOT RUN. The connected device still has v1.4.1; first-start Chinese behavior and the shorter English title remain pending on-device observation.

```text
File: build/FoloToy-AI-Passport-Penalty-v1.4.2-full.bin
Canonical copy: build/FoloToy-AI-Passport-full.bin
Size: recorded in the GitHub Release asset metadata
Flash offset: 0x0
SHA-256: see the attached SHA256SUMS.txt in the GitHub Release
Build: PASS
Host tests: PASS
Device tests: NOT RUN
Unverified: physical default-language startup and English menu readability
```

Use the project-and-version file attached to the v1.4.2 GitHub Release for device acceptance, and verify it against the attached `SHA256SUMS.txt`. Flashing it at `0x0` installs the complete standalone firmware and can reset NVS because the merged file spans the partition gaps. This makes it suitable for verifying the new no-preference Chinese default. The v1.4.1 image below remains the historical device-started build.

## v1.4.1 cover prompt alignment fix — historical device-tested artifact

The cover prompt is now localized at runtime in both languages instead of mixing baked artwork text with an overlaid label. The shorter copy is `PRESS OK` in English and the equivalent “press OK to start” instruction in Chinese. Both variants use the same font, 156-pixel text box, vertical position, centered alignment, and transparent background over an empty prompt panel, so switching language cannot expose or offset text from the artwork. Gameplay is unchanged.

- ESP-IDF 5.5.3 firmware build and merged-image verification: PASS. The application is 1,476,480 bytes and the factory partition remains 82% free.
- Static/host gate: PASS. Repository, model, scene, localization, preferences, font, art, navigation, audio, display, firmware-layout, and merged-image checks all pass. The art contract now verifies that the prompt panel contains no baked text.
- Actual LVGL 9.5.0 rendering: PASS for centered English and Chinese cover prompts, plus 50 sessions/entry-exit cycles in the 24 KB pool. Free memory after warm-up remains 12,248 bytes and the largest tested moving-frame redraw is 26,824 pixels. Captures are under `build/penalty-preview/`; they are desktop LVGL renders, not device photographs.
- Device installation/startup on 2026-09-23: PASS for the exact v1.4.1 image below on the connected ESP32-C3 revision 1.1 with 8 MB embedded Flash. Esptool completed its data-hash verification. A bounded 12-second reset/startup observation booted the factory app and initialized the 240 x 320 display/LVGL, ES8311 audio, CW2017 battery gauge, and ADC buttons without a reset loop, assertion, watchdog, or persistent error. Penalty entry reported 249,200 bytes free heap and a 114,688-byte largest block.
- Physical cover acceptance: NOT RUN. The corrected English and Chinese prompt alignment and readability still require user observation on the display.
- Gameplay acceptance boundary: the user reported that all gameplay outside the cover prompt is fine on the previously flashed v1.4.0 image. This change intentionally does not modify gameplay rules, controls, timing, art, or audio.

```text
File: build/FoloToy-AI-Passport-Penalty-v1.4.1-full.bin
Canonical copy: build/FoloToy-AI-Passport-full.bin
Size: 1542016 bytes
Flash offset: 0x0
SHA-256: 5dd8954564e8a8309cf146c6a9de05851562ecd89e75304d08346050aca08486
Build: PASS
Host tests: PASS
Device tests: PASS (flash and bounded startup scope)
Unverified: physical English/Chinese cover alignment and readability
```

Use the project-and-version file above for v1.4.1 device acceptance. It is byte-identical to the canonical merged image produced by the same successful gate. Flashing it at `0x0` installs the complete standalone firmware and can reset NVS because the merged file spans the partition gaps. Use segmented `idf.py flash` if the saved language must be preserved. The v1.4.0 image below remains the historical device-started build.

## v1.4.0 standalone goalkeeper challenge — historical device-tested artifact

The title now offers separate Shoot and Keep challenges. Keep retains the portrait behind-the-shooter camera: the AI shooter wears blue and truthfully previews one of six locked shot directions through body/foot poses for 900 / 600 / 350 ms on Easy / Normal / Hard, while the player controls the distant black keeper. The same two-press timing meter decides the dive. Exact-cell save rates are 90% / 65% / 25% on dark line / green / outside green; the other row in the same column uses 35% / 20% / 5%; another column is always conceded. Five attempts lead to a saves summary.

- ESP-IDF 5.5.3 firmware build and merged-image verification: PASS. The application is 1,476,656 bytes and the factory partition remains 82% free.
- Static/host gate: PASS. Pure-C tests cover both modes, all target/timing/roll relations, truthful locked cues, three cue durations, five saves, replay mode retention, audio outcomes, bilingual copy, fonts, and deterministic art generation.
- Actual LVGL 9.5.0 rendering: PASS for the four-item menu, six blue-shooter cues, timing state, all six goalkeeper outcomes, both summaries, and 50 sessions/entry-exit cycles in the 24 KB pool. Free memory after warm-up remains 12,248 bytes and the largest tested moving-frame redraw is 26,824 pixels. Captures are under `build/penalty-preview/`; they are desktop LVGL renders, not device photographs.
- Device installation/startup on 2026-09-23: PASS for the exact image below on an ESP32-C3 revision 1.1 with 8 MB embedded Flash. The write completed with esptool's data-hash verification. A bounded 12-second reset/startup observation booted the factory app and initialized the 240 x 320 display/LVGL, ES8311 audio, CW2017 battery gauge, and ADC buttons without a reset loop, assertion, watchdog, or persistent error. Penalty entry reported 249,200 bytes free heap and a 114,688-byte largest block.
- Physical gameplay acceptance: PASS for gameplay behavior by user report on 2026-09-23. The user found no gameplay issue outside the cover. The English and Chinese cover prompts were reported misaligned; that isolated presentation defect is addressed by v1.4.1 above. Instrumented stack headroom and endurance remain unmeasured.

```text
File: build/FoloToy-AI-Passport-Penalty-v1.4.0-full.bin
Canonical copy: build/FoloToy-AI-Passport-full.bin
Size: 1542192 bytes
Flash offset: 0x0
SHA-256: 44278fec6d7a7d6892b2a3aae4afbca3d1dc1b0cb5819cf518c6bda1ff3c920d
Build: PASS
Host tests: PASS
Device tests: PASS (flash and bounded startup scope)
Gameplay acceptance: PASS by user report, excluding the cover prompt defect
Unverified: instrumented stack headroom and endurance
```

This historical image is byte-identical to the canonical merged image produced by its successful gate. The v1.3 image below remains an older historical artifact.

## v1.3 portrait gameplay — historical local artifact

The display now stays at 240 × 320 from cover through summary. The cover replaces its rotation diagram with `PRESS OK TO START`; the title, Settings, Help, pitch, 192-pixel power meter, results, and summary are reflowed for portrait. Each kick starts at top-center. DOWN follows `top-center -> bottom-center -> top-left -> bottom-left -> top-right -> bottom-right -> top-center`; UP follows the exact reverse. Goal probabilities, difficulty widths, keeper logic, audio, language persistence, and the five-kick session are unchanged.

- ESP-IDF 5.5.3 firmware build and merged-image verification: PASS. The application is 1,178,800 bytes and the factory partition remains 86% free.
- Static/host gate: PASS. Model tests cover both complete target cycles and locked charge behavior; scene tests cover portrait coordinates and every target/outcome path; localization, fonts, art generation, audio, preferences, input lifecycle, display rounding, and firmware layout also pass.
- Actual LVGL 9.5.0 rendering: PASS for the cover, both languages, all six targets, all outcomes, and 50 sessions/entry-exit cycles in the 24 KB pool. Free memory after warm-up remains 12,248 bytes and the largest tested moving-frame redraw is 27,352 pixels. Captures were generated under `/tmp/penalty-preview-portrait/`; they are desktop LVGL renders, not device photographs.
- Device verification: NOT RUN. Portrait clipping at the rounded corners, physical UP/DOWN feel, small target/precision-line readability, audio synchronization, battery display, and endurance remain device checks.

```text
File: build/FoloToy-AI-Passport-full.bin
Size: 1244336 bytes
Flash offset: 0x0
SHA-256: 3473ba02ac7595573890f24e8cd7da73c60eafeadeb2bf08bda88d3ccbcb0916
Build: PASS
Host tests: PASS
Device tests: NOT RUN
Unverified: portrait corner clipping, physical navigation feel, small target/line
  readability, audio/display cadence, measured board heap/stack, and endurance
```

Use this merged image for v1.3 device acceptance. Flashing at `0x0` installs the complete standalone image and initializes its bundled NVS state. Older artifacts below remain historical records.

## v1.2 row-wise target navigation — historical local artifact

Every kick starts at top-center. UP, which is screen-right in the intended landscape hold, follows `top-center -> top-right -> bottom-left -> bottom-center -> bottom-right -> top-left -> top-center`. DOWN follows the exact reverse cycle. This keeps movement inside either row aligned with the physical button direction, while crossing rows begins from the new row's left or right edge. Selection still cycles at both ends; shot probabilities and every other rule are unchanged.

- ESP-IDF 5.5.3 firmware build and merged-image verification: PASS. App size is 1,220,256 bytes and the unchanged factory partition remains 85% free.
- Static/host gate: PASS. The new regression test covers both complete cycles, reverse movement, top-center initialization and resynchronization, plus the locked-target behavior during charge. Existing repository, model, scene, localization, preferences, fonts, art, audio, display, and firmware-layout checks remain green.
- Actual LVGL 9.5.0 rendering: PASS for all six target selections and 50 sessions/entry-exit cycles in the 24 KB pool. Free memory after warm-up remains 12,248 bytes and the largest tested moving-frame redraw is 27,535 pixels. Captures are in `build/penalty-navigation-v1.2-preview/`; they are desktop LVGL renders, not device photographs.
- Device verification: NOT RUN. The physical UP/right and DOWN/left control feel, row transition clarity, input/display cadence, and endurance remain device checks.

```text
File: build/Penalty-v1.2-row-navigation-standalone-full.bin
Canonical copy: build/FoloToy-AI-Passport-full.bin
Size: 1285792 bytes
Flash offset: 0x0
SHA-256: 3c7a24f97407ca50377d3c40efa3cace43df6f91955e7f8b6c6dfa73488b3d89
Build: PASS
Host tests: PASS
Device tests: NOT RUN
Unverified: physical navigation feel and row-transition clarity, input/display
  cadence, measured board heap/stack, and endurance
```

Use this merged image for v1.2 device acceptance. Flashing at `0x0` installs the complete standalone image and initializes its bundled NVS state. The v1.1 device-verified artifact remains unchanged.

## v1.1 visual refresh — historical device-verified artifact

The portrait cover adds a fixed Chinese game mark beneath `PENALTY`, all active pitch art uses horizontal grass bands, and the keeper kit is black. The landscape title screen no longer renders its former English/Chinese slogan. Rules, controls, probability calibration, localization behavior, audio, and persistence are unchanged from v1.0.

- Complete firmware build and merged-image verification: PASS with ESP-IDF 5.5.3. App size is 1,222,656 bytes and the unchanged factory partition remains 85% free.
- Static/host gate: PASS. Repository checks, model, scene, localization, preferences, dynamic copy, fonts, art contracts, navigation, display, and firmware-layout tests pass.
- Deterministic art contract: PASS for a 240 x 320 portrait cover, 320 x 240 gameplay background, 1086 x 1448 community-cover candidate, exact cover-mark composition, horizontal-band orientation, and removal of blue keeper-uniform pixels from active runtime sprites.
- Actual LVGL 9.5.0 rendering: PASS for the portrait cover, both title languages, gameplay, and 50 sessions/entry-exit cycles in the 24 KB pool. Free memory after warm-up remains 12,248 bytes and the largest tested moving-frame redraw is 27,535 pixels. The review montage is `build/penalty-art-v1.1-preview/review.png`; it is a desktop LVGL render, not a device photograph.
- Device verification: PASS. The merged v1.1 image below has been flashed and played on a real device, so physical color/contrast, cover-mark legibility, stripe readability, and the timing/audio behaviour are no longer open items.

```text
File: build/Penalty-art-v1.1-full.bin
Canonical copy: build/FoloToy-AI-Passport-full.bin
Size: 1288192 bytes
Flash offset: 0x0
SHA-256: 98aeea498634309fe4d10248347c513dd7b212e3d9ce02d651a7400c5ba66607
Build: PASS
Host tests: PASS
Device tests: PASS
Unverified: no measured heap, frame-cadence or endurance figures are recorded
  for this revision
```

This is the versioned file the visual-refresh device acceptance used. The v1.0 file remains an unchanged historical artifact. The v1.1 community-cover candidate has not been uploaded, and the existing community review state is unchanged.

## v1.0 six-target rules — historical local artifact

The goal now exposes six selectable cells in two rows. LEFT/RIGHT traverses a closed clockwise/counterclockwise ring, while the keeper independently predicts one of the same six cells. With uniform keeper predictions, the calibrated final average goal rates are 90% on the two-value dark line, 65% in the rest of green, and 25% at normal power outside green. Weak, high, and timeout shots remain failures. A dark-line save still counts as a precision hit, but not as a goal.

- Complete firmware build and merged-image verification: PASS with ESP-IDF 5.5.3. App size is 1,222,672 bytes and the unchanged factory partition remains 85% free.
- Static/host gate: PASS. Exhaustive model coverage checks all 363,600 player-target, keeper-target, power, and 0–99 roll combinations. Each of the six player targets independently produces exact 90% / 65% / 25% averages across uniform keeper predictions.
- Actual LVGL 9.5.0 rendering: PASS for all six target selections, every target/outcome trajectory, glove contact for all three save families, both languages, and 50 sessions/entry-exit cycles in the 24 KB pool. Free memory after warm-up remains 12,248 bytes and the largest tested moving-frame redraw is 27,535 pixels.
- Six target-selection captures are combined in `build/penalty-six-target-preview/six-target-grid.png`. They are desktop LVGL renders, not device photographs. No board was flashed.

```text
File: build/Penalty-six-target-v1.0-full.bin
Canonical copy: build/FoloToy-AI-Passport-full.bin
Size: 1288208 bytes
Flash offset: 0x0
SHA-256: c2f2ae3cb2b09c60fc0554bd5b9f32d167c1d9e35ff065f154f7f2f8609fd901
Build: PASS
Host tests: PASS
Device tests: NOT RUN
Unverified: physical six-cell readability and control feel, statistical sampling,
  audio/contact timing, input/display cadence, board heap/stack, and endurance
```

Use this versioned file for six-target device acceptance. The v0.9 file remains an unchanged historical artifact.

## v0.9 bilingual UI — historical local artifact

English and Simplified Chinese now cover the portrait entry prompt, title, Settings, Help, gameplay, results, summary, and low-memory fallback. The title contains Play, Settings, and Help; Settings contains Difficulty, Sound, Language, and Back. Changing Language refreshes the current page immediately and queues a non-blocking NVS save outside the LVGL/input path. English is the safe default if NVS is absent, invalid, or unavailable.

- Complete firmware build and merged-image verification: PASS with ESP-IDF 5.5.3. App size is 1,219,696 bytes and the unchanged factory partition remains 85% free.
- Static/host gate: PASS. Model, localization, preference encoding/fallback, asynchronous save lifecycle, audio mapping, navigation, font contract, rendering, and existing gameplay tests all pass.
- The generated 14 px and 20 px Source Han Sans SC subsets cover all 164 localized glyphs under SIL OFL 1.1. No complete CJK font or runtime loader is linked.
- Actual LVGL 9.5.0 rendering: PASS for Chinese title, Settings, Help, aim, summary, and portrait prompt, plus English coverage and 50 sessions/entry-exit cycles in the 24 KB pool. Real-font label-width checks pass; free memory after exit remains 12,248 bytes after warm-up. The largest tested moving-frame redraw is 27,535 pixels.
- No board was flashed. Physical Chinese legibility, NVS retention across a real restart, audible output, and timing remain device checks. Flashing this merged image at `0x0` resets the NVS area; use the documented segmented update when an existing preference must be preserved.

```text
File: build/Penalty-bilingual-v0.9-full.bin
Canonical copy: build/FoloToy-AI-Passport-full.bin
Size: 1285232 bytes
Flash offset: 0x0
SHA-256: 6eff89689f5d8a224726a0a1ee40894d336a5eacaa22417efe92d035b861e313
Build: PASS
Host tests: PASS
Device tests: NOT RUN
Unverified: physical Chinese legibility, language persistence after real reboot,
  audio/contact timing, input/display cadence, board heap/stack, and endurance
```

Use this versioned file for bilingual UI and hybrid-audio device acceptance. Selecting Chinese should survive leaving and re-entering the game and a normal reboot. Reflashing the merged image is a fresh NVS state and therefore returns to English.

## v0.8 dynamic copy — historical local artifact

The numeric perfect-line footer is replaced by state-aware copy. EASY retains `DARK LINE = PERFECT` on every kick; NORMAL/HARD show it only on kick one, then select one stable encouragement per kick without adjacent repeats. Their fifth kick has dedicated copy, and each outcome has a distinct light-commentary footer. Copy selection does not advance gameplay randomness or change any shot decision.

- Complete firmware build and merged-image verification: PASS with ESP-IDF 5.5.3. App size is 1,133,200 bytes and the unchanged factory partition remains 86% free.
- Static/host gate: PASS. Pure-C tests cover teaching boundaries, stable selection, adjacent non-repetition, the final kick, exact outcome mapping, and a 28-character cap.
- Actual LVGL 9.5.0 rendering: PASS for the normal interactive line, final-kick line, all result lines, and 50 sessions/entry-exit cycles in the 24 KB pool. Label-width checks pass and free memory after exit remains 12,248 bytes after warm-up.
- The v0.7 hybrid audio, visuals, shot rules, and community-review state are unchanged. No board was flashed.

```text
File: build/Penalty-dynamic-copy-v0.8-full.bin
Canonical copy: build/FoloToy-AI-Passport-full.bin
Size: 1198736 bytes
Flash offset: 0x0
SHA-256: c05a090460e578368e3b4c3cf74a2ecb94428b3cd92cae0b6d479421b7dcb83c
Build: PASS
Host tests: PASS
Device tests: NOT RUN
Unverified: physical copy readability/tone, audible output and timing,
  input/display cadence, board heap/stack, and endurance
```

Use this versioned file for dynamic-copy and hybrid-audio device acceptance.

## v0.7 hybrid audio — historical local artifact

The silent-output software defect is corrected by requiring a successful 60% volume setting after the 16 kHz, 16-bit mono format opens. UI, difficulty, mute, miss, reward, and full-time feedback use short synthesized cues. Kick, glove/save, and restrained goal impacts use original deterministic PCM assets streamed from Flash. The kick cue is emitted at the 100 ms visual-contact boundary rather than at shot commitment; timeout produces a miss cue without a kick.

- Complete firmware build and merged-image verification: PASS with ESP-IDF 5.5.3. App size is 1,132,576 bytes and the unchanged 8 MB partition layout has 86% of the factory partition free.
- Static/host gate: PASS. The new pure-C audio tests cover contact timing, timeout without a kick, every result family, difficulty/mute/full-time mapping, cue priority, and asset regeneration. Existing model, scene, navigation, shell lifecycle, display and firmware-layout checks remain green.
- Audio lifecycle: format and volume failures are logged separately; the UI reports sound available only when both succeed. A four-entry bounded semantic queue drops stale cues, lets action/result cues replace queued UI noise, and cooperatively cancels playback in 10 ms chunks on mute/exit.
- PCM footprint: 9,280 bytes total, 16 kHz signed 16-bit mono, with 80 ms kick, 100 ms save and 110 ms goal samples. Their deterministic source and CC0-1.0 terms are recorded in the asset documentation.
- No board was flashed. Successful compilation and PCM writes do not prove that the speaker is audible, clean, or correctly synchronized on hardware.

```text
File: build/Penalty-hybrid-audio-v0.7-full.bin
Canonical copy: build/FoloToy-AI-Passport-full.bin
Size: 1198112 bytes
Flash offset: 0x0
SHA-256: 4b8a2726a548bf4c8b03688c0bad091ab9bddffa970ef36ede80203a478f8563
Build: PASS
Host tests: PASS
Device tests: NOT RUN
Unverified: audible output, 40/60/80% calibration, timbre/clipping/crackle,
  contact sync, rapid exit/re-entry, board heap/stack, and endurance
```

Use this versioned file for the hybrid-audio device test. The default is deliberately provisional at 60%; compare dedicated 40% and 80% calibration builds only if the first device run is too loud, too quiet, or distorted. The cover, visual gameplay, rules, and community-review state are unchanged.

## v0.6 cover cleanup — historical local artifact

The portrait boot cover now hides the live battery label, while the landscape title, Help, aim, flight, result, and summary screens retain it. Gameplay artwork, mechanics, input mapping, and shot timing are unchanged from v0.5. The separate label-free community poster is documented in [the community record](community-submission.md) and was not uploaded.

- Complete `./tools/validate.sh`: PASS with ESP-IDF 5.5.3. App size is 1,120,784 bytes; the merged image and unchanged partition layout pass verification.
- Actual LVGL 9.5.0 rendering: PASS for 50 sessions/entry-exit cycles in the 24 KB pool, with 12,248 bytes free after exit following warm-up. The portrait cover is now compared pixel-for-pixel with the embedded asset across the entire image, including the former battery area; landscape label-width and game rendering checks still pass.
- Static repository, model, scene, navigation, shell-contract, firmware-layout, and resource-regeneration checks: PASS. No device was flashed.

```text
File: build/Penalty-cover-clean-v0.6-full.bin
Canonical copy: build/FoloToy-AI-Passport-full.bin
Size: 1186320 bytes
Flash offset: 0x0
SHA-256: b1031c4da997cce3a50a2f763cfbdff5ae93afff537b9288afaff45c6ae01b17
Build: PASS
Host tests: PASS
Device tests: NOT RUN
Unverified: physical cover/game readability, frame cadence, input response,
  audio/contact timing, landscape battery, board heap/stack/endurance, and host ASan
```

Use this versioned file for the cover-cleanup acceptance. The v0.5, v0.4, and v0.3 versioned firmware copies remain unchanged.

## v0.5 pixel artwork — historical local artifact

The approved stadium, three striker poses, four keeper poses and six ball sizes are integrated. v0.4 gameplay rules, portrait cover, input mapping and 600 ms shot-presentation duration are unchanged. There is no new community upload and no device flash in this task.

- Complete `./tools/validate.sh`: PASS with ESP-IDF 5.5.3. App size is 1,120,736 bytes; merged image and unchanged partition layout verified. Transcript: `build/penalty-art-validation.log`.
- Existing model tests and new pure scene tests: PASS; scene tests also pass UndefinedBehaviorSanitizer. Checks cover calibrated support-foot/contact anchors, all valid shot-direction/keeper/outcome combinations, lead-in and arrival boundaries, held saves, timeout without a kick, and high-shot bounds.
- Actual LVGL rendering: PASS for 50 sessions/entry-exit cycles, 24 KB pool and the existing 4,800-pixel buffer. Free memory after exit remains 12,248 bytes after warm-up. The largest tested consecutive motion-frame update is 8,272 pixels (host dirty-area accounting, not device FPS or transfer time).
- Every tested incremental render is compared pixel-for-pixel with a full redraw; foreground striker pixels occlude the ball correctly, save endpoints use real glove pixels, and high shots cannot overwrite the header. Rotation changes only at screen transitions in the harness. The previous harness reset rotation every frame and therefore could not prove partial-redraw correctness.
- Resource conversion `--check`: PASS, 264,180 const pixel bytes, no runtime PNG decoding or scaling. The 64 actual LVGL captures in `build/penalty-art-preview/` cover menus, all outcomes/directions, difficulty extremes and a 50 ms-step kick sequence. Native-size composites were visually reviewed. These are desktop renders, not device screenshots.
- ASan remains NOT RUN because of the previously recorded host-runtime startup failure. Device cadence, readability, audio/contact timing, power/battery and endurance remain unverified.

```text
File: build/Penalty-pixel-art-v0.5-full.bin
Canonical copy: build/FoloToy-AI-Passport-full.bin
Size: 1186272 bytes
Flash offset: 0x0
SHA-256: 87b51e76dc9e0afc04091805cda6dd13e95ef8bd55d60c22121dae30bcaba3ac
Build: PASS
Host tests: PASS
Device tests: NOT RUN
Unverified: physical readability, frame cadence, input response, audio/contact
  timing, battery, board heap/stack/endurance, and host ASan
```

Use this versioned file for new-art acceptance. In addition to the checklist below, verify the stationary ball meets the boot before launch, both saves meet gloves, no sprite trails remain, the green/dark-line UI stays readable, and long OK restores the unchanged cover. The visual lead-in is 100 ms within the original 600 ms budget; kick audio still begins at commitment. Both v0.4 and v0.3 versioned firmware copies remain unchanged.

## v0.4 difficulty update (historical local artifact)

Three difficulties, per-shot random green zones, a two-value guaranteed-goal line, ordinary-green probabilities, and one-fifth-speed green timing are implemented. Gameplay retains geometric sprites; the separated pixel-art layers are not integrated. No device was flashed, and this update was not uploaded or submitted to the community.

- Complete `./tools/validate.sh`: PASS with ESP-IDF 5.5.3; static/host checks, application fit, partition layout, and merged-image verification all pass. Transcript: `build/penalty-difficulty-validation.log`.
- Pure C model and UndefinedBehaviorSanitizer: PASS. Coverage includes all 101 powers, nine direction pairs and ten rolls, every green start/inner-line placement for all three widths, 80 ms perfect crossings, both meter directions, 30,000 generated shots, no reroll on input recovery, duplicate/stale events, five-shot accounting, and 100,000 event-sequence steps.
- Actual LVGL 9.5.0 rendering: PASS for 50 sessions/entry-exit cycles in the 24 KB pool. Free LVGL memory after exit stays at 12,248 bytes after warm-up. The tests check menu/result text width, exact green/dark-line pixels at both range extremes, all cursor values, and unchanged portrait cover pixels outside battery text. This is host evidence, not device memory or timing measurement.
- Twenty-eight PPM captures are in `build/penalty-difficulty-preview/`; menu, aim, charge, hard-mode edge placement, and Help were visually reviewed in `review.png`. These are actual desktop LVGL renders, not photographs or device screenshots.
- AddressSanitizer was not rerun: the earlier host-runtime startup problem remains unresolved. UndefinedBehaviorSanitizer does not replace ASan.

```text
File: build/Penalty-difficulty-v0.4-full.bin
Canonical copy: build/FoloToy-AI-Passport-full.bin
Size: 920928 bytes
Flash offset: 0x0
SHA-256: 2751391fa6a36790718479006bb1c3e37ad3218fd86142b9af14c9eb5a0ff92a
Build: PASS
Host tests: PASS
Device tests: NOT RUN
Unverified: 80 ms perfect-line timing, physical readability/input response,
  audio, battery, board heap/stack/endurance, and host ASan
```

Use the versioned file above for v0.4 device acceptance. Earlier v0.3 copies remain intact; the historical evidence below does not describe the current canonical binary.

## Milestone status

| Milestone | Status |
| --- | --- |
| M0 — design | Complete; landscape proposal incorporated |
| M1 — pure C rules | Implemented; host tests pass |
| M2 — input and screen flow | Implemented; host rendering passes; device input/rotation acceptance pending |
| M3 — pixels and sound | Implemented in firmware; screen captures inspected; speaker listening pending |
| M4 — acceptance | Firmware build and merged-image gate pass; user device acceptance and physical measurements remain open |

## v0.3 automated evidence (2026-09-15, historical)

- Repository static gate: PASS, including document links/languages, workflow lint, baseline tests, the penalty model, navigation regression, and standalone-shell lifecycle contracts.
- Model compiled with `-Wall -Wextra -Werror`; the current UndefinedBehaviorSanitizer run passes. The current AddressSanitizer run did not reach test `main`: a process sample showed a recursive lock during shadow-memory initialization in the host ASan runtime. Both stalled processes were stopped. The earlier MVP's ASan pass does not certify this revision; rerun ASan on a compatible host runtime.
- Rule coverage: all nine direction pairs and power boundaries; meter ascent/descent; 4,000 ms deadline; delayed frames; stale/duplicate events; exactly five attempts; 5/5 and 0/5 sessions; mute, retry, title, exit, and reproducible seeds.
- Navigation regression: CLICK/DOUBLE/LONG tails retain the original PRESS epoch, so release/click tails cannot leave the newly restored cover after long OK.
- Actual LVGL 9.5.0 renderer: PASS for 50 simulated sessions and 50 entry/exit cycles in a 24 KB LVGL pool, using a single 4,800-pixel RGB565 draw buffer. Free LVGL memory after exit stays at 12,248 bytes after warm-up on the desktop build. Desktop allocation sizes and timing are not ESP32-C3 measurements.
- Portrait/landscape corner classification is checked for every pixel under a quarter-turn. Label width checks cover orientation, title, aim, power, flight, all six results, mute, and summaries. Seventeen PPM captures (including Help and return-to-cover) are produced in `build/penalty-cover-preview/`; PNG conversions were visually inspected for the cover and Help. The cover's RGB565 pixels are checked against the embedded asset outside the live battery area, with the existing rounded mask applied; unknown/100% battery labels are checked for clipping.
- Complete `./tools/validate.sh` gate: PASS on macOS arm64 with ESP-IDF v5.5.3, GCC 14.2.0, and the unchanged component lock. Bootloader, partition-table MD5/layout, application fit, and merged bytes verified. The current image size and checksum are recorded below. This is build evidence, not a device test.
- `./tools/preview-penalty.sh` also passes through its CMake wrapper against the complete LVGL 9.5.0 package verified against the locked component hash.

Acceptance artifact, built 2026-09-15:

```text
File: build/FoloToy-AI-Passport-full.bin
Acceptance copy: build/Penalty-portrait-cover-v0.3-full.bin
Size: 919968 bytes
Flash offset: 0x0
SHA-256: a585b0be9389e8a9425f758580b0040804210a6ee1911b23b0229889c70ad7ab
Build: PASS
Host tests: PASS
Device tests: NOT RUN
Unverified: physical acceptance checklist below; current host ASan run
```

The local build transcript is `build/penalty-cover-validation.log`. Both the binary and log are generated, ignored artifacts; rebuilding may change the checksum. Verify the file you actually flash.

For the v0.3 community submission, the same source was rebuilt and the complete gate passed again. At submission time, the canonical file matched `build/Penalty-community-v0.3-full.bin`, with SHA-256 `26ca8313be811a0167fbec29b3881e94dfce50b27f03a749d63d017f9a7eaf60` and the same 919,968-byte size. Both v0.3 copies remain intact. See [community submission](community-submission.md) for the exact submitted artifact and historical receipt. No new review-status query was made for this local v0.4 update.

Reproduce from the repository root:

```bash
./tools/validate.sh --static
./tools/preview-penalty.sh /path/to/lvgl-9.5.0
# In an activated ESP-IDF 5.5.3 environment:
./tools/validate.sh
```

## Implementation details affecting acceptance

- All three game keys use PRESS; subsequent CLICK/DOUBLE events are ignored. Physical UP means logical right, DOWN means left. Boot enters the portrait cover directly; no hardware-demo menu is displayed.
- Button timestamps drive power locking. Events over 100 ms late resynchronize uncommitted aim/charge instead of producing a delayed shot. Timeout feedback may wait an extra 100 ms for an already queued press; the valid input window remains exactly 4,000 ms by event time.
- Difficulty and mute last until returning to the portrait cover; retry resets scores but retains both settings. Default difficulty is EASY. Language alone is stored under the existing NVS namespace/key and restored before the first cover; missing/invalid storage falls back to Simplified Chinese while a valid saved English choice remains English. No network access or new partitions are used.
- Green zones, the inner two-value dark line, the keeper's six-cell target, and the 0–99 outcome roll are chosen once per new shot. Input recovery preserves all four. Both aim and charge show the same zones and six cells; the cursor does not obscure the line. The green window slows to one-fifth speed in both directions.
- Audio uses a four-item semantic queue and 10 ms PCM chunks with cooperative stop. Format, explicit 60% volume, and write failures degrade to silent play and remain separately observable. Battery polling is in that worker, not the LVGL task. A missing worker leaves the game playable with unknown battery on landscape screens and no sound; the portrait cover never shows a battery label.
- Idle cover/title/Help/aim/summary screens dim to 15% backlight after 60 seconds; the first PRESS wakes without taking a shot. Exit restores 100% backlight.
- Rotation errors attempt rollback and keep the prompt available for retry. Worker-stop or rotation-restore failure retains the page; long OK retries cleanup. Physical failure injection is still required.

## Install the acceptance firmware

The deliverable is `build/FoloToy-AI-Passport-full.bin`, generated only after the firmware gate passes. It is a merged ESP32-C3 image for 8 MB Flash, containing the bootloader, partition table, and application. Flash it over a data-capable USB cable at **offset `0x0`**; copying the file onto the device as an ordinary file is not the installation procedure. The firmware opens the portrait Penalty cover immediately after display initialization; optional audio/battery initialization follows.

**Data warning:** flashing the merged image replaces the current firmware and can reset NVS settings. Back up any firmware/configuration you need before flashing. Do not choose an erase-all option. If existing device data must be retained, stop and arrange the appropriate segmented update instead.

For the ESP-IDF environment used here (esptool 4.12.0), close other serial monitors, replace `<port>` with the actual device port, and run from the repository root:

```bash
python -m esptool --chip esp32c3 -p <port> -b 460800 \
    write_flash 0x0 build/FoloToy-AI-Passport-full.bin
```

`<port>` is a placeholder, not a literal argument (macOS ports commonly start with `/dev/cu.usbmodem`; Windows uses `COM` ports). A graphical ESP32 flasher must use the same chip, file, and offset. Do not put an app-only image at `0x0`. This is the command the recorded device verification used.

For an initial smoke test, check correct orientation, LEFT/RIGHT mapping, one full five-shot session, sound/mute, and portrait restoration on long OK. If something fails, report the step, actual versus expected behavior, and a short video or sanitized serial log; note whether it happens every time.

## Device acceptance checklist

Use the verified merged image for a blank board; preserve existing NVS with segmented flashing when appropriate, following the repository [build instructions](../../docs/development/engineering/build-and-test.md). The v1.1 image passed this checklist on a real device.

1. After flashing the merged image, power on. Confirm the first application screen is the approved portrait pixel cover with the English turn prompt, without FoloToy or Display/Button menus and without any battery text. Rotate the device clockwise (original top to the right) and press OK once. The title must be upright, fill 320 × 240 logical pixels, have four correct black corners, and show the live battery label in its header.
2. Open Settings. Cycle Difficulty through Easy / Normal / Hard, toggle Sound, then change Language to Simplified Chinese. Confirm the current page changes immediately and all four rows fit. Return to the title and open Help; verify Chinese glyphs are present, legible, and unclipped. Return to Settings, select the intended difficulty, then choose Play. Confirm green widths of 20 / 11 / 6 values and a two-value inner dark line. From the initial top-center cell, press physical UP repeatedly and confirm top-center → top-right → bottom-left → bottom-center → bottom-right → top-left → top-center; physical DOWN follows the exact reverse cycle. Within either row, UP must always move right and DOWN left. OK starts the meter; a separate OK locks it. The entry gesture must not also start a shot.
3. Confirm all six cell outlines and the bright selected frame remain readable. The zones must be visible before charge, stay fixed during it, and change for the next kick. Check slow green traversal in both directions and practical readability/timing of the dark line. Exercise weak, dark-line goal/save, ordinary-green goal/save, outside-green goal/save, high, and timeout results. Confirm the displayed ball cell, keeper cell, result copy, sound family, and exactly one recorded slot per attempt agree. The exact 90% / 65% / 25% averages are automated model evidence; a few device outcomes cannot statistically revalidate them.
   On EASY, confirm the localized dark-line teaching message remains on every kick. On NORMAL/HARD it appears only on kick one; later localized encouragement stays unchanged through aim/charge, adjacent kicks do not repeat, and kick five uses the dedicated final-kick message. Result commentary must match the actual outcome and fit without clipping in both languages.
4. Finish five shots, replay, return to the title, toggle Sound, and replay muted. Difficulty and mute must survive replay/title; returning to the cover resets both. Language must remain Chinese on the cover, after re-entry, and after a normal power-cycle. Switch back to English and repeat the restart check. A session must not invent an opponent score or sudden death.
5. Open Help from the landscape menu; OK returns to the menu. Long OK during cover, title, Settings, Help, aim, charge, flight, result, and summary must stop sound and restore the portrait cover. Releasing that OK must not reopen the landscape menu. Re-enter immediately and confirm no old key/sound reaches the new session.
6. Wait 60 seconds on cover/title/Help/aim/summary: verify dimming and one-press wake without an accidental shot. Confirm the cover remains battery-free; on landscape screens, confirm battery/unknown-battery text fits and updates without disrupting play.
7. Play 20 consecutive sessions and perform 50 entry/exit cycles. Review `penalty` entry/exit heap, largest-block, minimum-heap, worker stack, `max_update_us`, and `max_input_age_ms` logs. Compare after warm-up, with no progressive heap loss, task leak, watchdog, or reboot.
8. Measure input-to-visible response against the 100 ms goal and animation against the provisional 20 FPS goal. `max_update_us` measures the model/UI update only, excluding final drawing/SPI transfer; neither it nor host screenshots proves display FPS. Check button reach, glare, readable text, and corner clipping at physical size.
9. Listen first at the built-in 60% volume. Confirm distinct charge, kick, ordinary goal, perfect goal, ordinary/green/dark-line save, weak shot, high/timeout, difficulty, mute, and full-time cues. Timeout must have no kick. Use slow-motion video to check kick onset within about 50 ms of the visual 100 ms contact point. Check clipping/crackle, SOUND-off's final cue, SOUND-on feedback, rapid exit/re-entry cancellation, and 20 complete sessions. Request 40%/80% calibration builds only if needed. Test missing audio/battery and deliberate worker/rotation failures only in a controlled development setup; confirm safe recovery and responsive long OK.

Remaining: actual orientation, physical Chinese legibility and preference persistence, input latency, display cadence, audio quality, battery readings, system heap/stack, failure injection, and physical endurance checks.
