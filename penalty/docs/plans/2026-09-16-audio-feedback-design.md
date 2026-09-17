<p align="right">
  <a href="2026-09-16-audio-feedback-design.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Penalty Hybrid Audio Feedback Design

Status: M0–M2 implemented locally on 2026-09-16; M3 device audio acceptance is pending. This document does not claim audible device validation.

## 1. Finding and objective

The board has a built-in speaker and microphone connected through an ES8311 codec. The current proven application path opens 16 kHz, 16-bit, mono PCM over I2S. Penalty already initializes the codec, opens the PCM format, runs blocking writes in a worker, and generates basic square-wave effects.

The current game omits an explicit output-volume setting. The codec-device object is zero-initialized and reapplies that zero volume when opened, which is sufficient to explain silent output. The Audio demo explicitly sets 80%, while Penalty does not. Device testing must still rule out a second board-specific fault after this software defect is fixed.

The objective is concise, readable feedback that supports play without becoming continuous noise or changing the difficulty rules. UI actions use synthesized pixel-style tones. Ball contact, saves, and goals use short PCM samples once the basic output path is verified.

## 2. Scope and non-goals

In scope:

- Explicit output volume and observable audio-path errors.
- Semantic sound events independent from waveform or sample choice.
- Short synthesized UI tones and short PCM action/result samples.
- Session mute, stale-event cancellation, and safe worker teardown.
- Device calibration for volume, timing, clipping, crackle, and endurance.

Not in the first audio release:

- Background music, continuous crowd ambience, streamed audio, voice control, recording, or persistent volume settings.
- Sounds on every aim movement or animation frame.
- Automatic cues for entering the green or dark precision line. Such cues would materially make the timing challenge easier and require a separate accessibility-mode decision.

## 3. Sound map

| Event | Treatment | Priority | Timing |
| --- | --- | --- | --- |
| Charge starts | Short low synthesized cue | P0 | On accepted first OK |
| Boot contacts ball | Dry 60–100 ms PCM kick | P0 | At the 100 ms visual contact point, not shot commitment |
| Ordinary goal | Short rising two-note result, optionally with a restrained PCM crowd accent | P0 | When the ball reaches the goal |
| Dark-line goal | Distinct three-note reward plus the same kick/goal family | P0 | At goal arrival |
| Save, weak shot, green save, dark-line save | Short low glove/impact PCM with a falling tail | P0 | At keeper contact or stopped-ball endpoint |
| High shot or timeout | Airy downward miss tone; timeout has no fake kick | P0 | At visible miss, or timeout result |
| Menu confirmation | Very short synthesized click | P1 | On accepted OK |
| Difficulty changes | Three ascending pitches for Easy, Normal, Hard | P1 | After the new value is applied |
| Sound on/off | Rising confirmation when enabling; one final falling cue before disabling | P1 | At toggle |
| Full time | Short score-dependent cadence; a distinct 5/5 finish | P1 | On summary entry |

Aim movement, automatic next-shot transitions, idle dimming, and cover display remain silent initially.

## 4. Playback design

Introduce a game-owned semantic event such as `PENALTY_SFX_KICK`, `GOAL`, `PERFECT`, `SAVE`, `MISS`, or `UI_CONFIRM`. The model remains independent of audio; the application adapter derives these events from accepted state transitions. The audio worker maps each event to a tone sequence or PCM asset.

After `bsp_audio_set_format(16000, 16, 1)` succeeds, set an explicit provisional volume of 60%. Compare 40%, 60%, and 80% on the device before selecting the final value. Make volume-setting failure observable rather than showing `SOUND` solely because the format opened. Keep PCM writes outside button callbacks and LVGL locks.

Use a small bounded queue. Critical result events may replace obsolete UI clicks, while teardown, mute, and session changes cancel queued and active playback. Each effect should normally stay below 300 ms; the full-time cadence may use up to 500 ms. Keep attack/release ramps to avoid clicks. Short assets are stored as 16 kHz, 16-bit mono PCM in read-only Flash and streamed in small chunks; no runtime decoder or full-file RAM copy is required.

## 5. Delivery sequence

### M0 — restore and diagnose output

Set explicit volume, add a development-only low/mid/high test sequence, and report codec init, format, volume, and first-write failures separately. On a board, confirm that the speaker works before changing the sound vocabulary.

### M1 — synthesized feedback

Add semantic events and implement charge, kick, goal, perfect, save, miss, menu-confirm, difficulty, mute, and full-time tone sequences. Align kick output to visual contact. Cover event mapping, priorities, stale-event handling, mute ordering, and timeout-without-kick in host tests.

### M2 — hybrid samples

Replace kick, glove/save, and optional restrained goal accent with approved PCM samples. Retain synthesized UI and reward tones. Record source/license, format, duration, byte size, peak level, and conversion command with the assets.

### M3 — device acceptance

Listen at 40%, 60%, and 80%; choose the lowest clear setting without clipping. Verify every outcome family, mute/unmute, rapid exits, immediate re-entry, 20 complete games, and 50 entry/exit cycles. Use slow-motion video to check that kick sound begins within about 50 ms of visual contact. Confirm no progressive heap loss, task leak, watchdog, reboot, or button-correlated crackle. Host tests and a successful build do not substitute for this listening test.

## 6. Acceptance boundary

The feature is complete only when the exact merged firmware passes the repository gate and the device checklist above. A visible `SOUND` label or successful PCM write proves only software state, not audible output. If the explicit-volume diagnostic remains silent, inspect ES8311 logs/address response, codec power, I2S clocks/data, speaker path, and the always-on external amplifier before tuning game sounds.

## 7. Local implementation record

- Output opens as 16 kHz, 16-bit mono and must also accept an explicit 60% volume before the UI reports the audio path available. Format, volume, and PCM-write failures have separate logs.
- `penalty_audio.c` derives semantic events from accepted model transitions. Host tests cover the 100 ms contact boundary, no timeout kick, every result family, difficulty, mute ordering, cue priority, and the 5/5 finish.
- A four-entry bounded queue drops stale feedback; action/result cues can replace queued low-priority UI feedback. Playback remains in the worker and checks stop/mute between 10 ms chunks.
- Original deterministic PCM assets provide 80 ms kick, 100 ms save, and 110 ms goal impacts. Synthesized tones remain responsible for UI, charge, difficulty, mute, miss, result melody, and full-time feedback.
- The complete ESP-IDF build and merged-image verification pass. This is software/build evidence only; volume, timbre, clipping, timing, crackle, and endurance still require the M3 board checklist.
