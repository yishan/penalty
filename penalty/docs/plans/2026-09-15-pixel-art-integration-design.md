<p align="right">
  <a href="2026-09-15-pixel-art-integration-design.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Penalty pixel-art layers and device integration

## Scope and current status

The user approved the cover-like landscape direction and requested dark-green replacements for white field-side blocks, an explanation of ball animation, and asset separation. White blocks are interpreted as the advertising boards below the crowd; goalposts, net and necessary pitch lines stay white. The entire decorative advertising strip is now dark green, without its old colored blocks or football symbols.

The 2026-09-15 step prepared artwork only. On 2026-09-16 the user approved integration after accepting the v0.4 mechanics. Local v0.5 integrated these layers. The v1.1 deterministic style pass now changes every active pitch to horizontal grass bands, changes the keeper kit from blue to black, adds the fixed Chinese game mark to the portrait cover, and removes the landscape title slogan. Rules and input mapping are unchanged; the previous community submission remains untouched. The v1.1 image has since been verified on a real device. Generated source assets are artwork, not runtime screenshots.

The v1.1 pass is reproducible with `python3 tools/style-penalty-art.py`, followed by the existing cover and gameplay converters. It derives the active assets from preserved v1.0 sources instead of regenerating the composition, so the approved identities, poses, and layout stay stable. The new community-cover candidate is illustrative and has not been uploaded.

## Implemented v0.5 calibration and rendering

- `main/penalty_scene.c` owns pure, host-tested presentation calculations; `main/penalty_ui.c` renders them. The gameplay model is unchanged.
- Support feet stay at (120,177). The follow-through toe at frame-local (71,52) touches the near edge of the ball centered at (171,147). The same ball position is visible before charge and through contact; no teleport is used. Ready/back-swing/follow-through retain their measured support-foot anchors.
- Presentation remains 600 ms total: back-swing for 0–49 ms, follow-through/contact from 50 ms, ball launch after 100 ms and arrival at 600 ms. Sound still starts at shot commitment, before the short visual lead-in; listening acceptance is pending.
- Keeper contacts are left (8,32), right (49,27), low between palms (28,43), in their 56-pixel frames. The right glove's original provisional y=32 fell outside the sprite and was corrected against actual alpha/color pixels. Saves end at these contacts and remain there; goals enter the net, high shots stay clipped below the HUD, and timeout keeps the ready pose without a fake kick.
- The active field is clipped to x0–319/y18–182, target centers are x110/160/210, and the meter begins at y182. The ball uses six pre-sized images. Foreground striker pixels occlude a receding ball. The v1.1 portrait cover keeps the established composition and adds the fixed Chinese game mark.
- The converter generates 264,180 bytes of four-byte-aligned const pixel data: a 153,600-byte RGB565 background and RGB565A8 sprites. No runtime PNG decode, scaling, full-screen allocation or extra per-frame objects. Alpha 0–1 becomes 0, 254–255 becomes 255, and other edges are retained.
- Moving frames invalidate old/new sprite, ball and shadow areas. Dirty-region output is checked against a full redraw, and high shots cannot paint into the header. The host harness changes rotation only at transitions, matching device behavior; it no longer accidentally forces a full redraw every frame.

Reproduce resource conversion with `python3 tools/convert-penalty-gameplay.py`, or add `--check` to verify reproducibility (offline Pillow required). Actual LVGL frames are in `build/penalty-art-preview/`. See [validation](../validation.md) for the exact firmware and device checklist. On-device frame cadence, readability, audio timing and endurance still need acceptance.

## Selected approach

Use a static background, transparent character frames, and a time-driven independent ball. Draw the HUD and feedback dynamically. This keeps the visual style while letting aim, power, keeper direction and shot outcome remain live.

Alternatives considered: a full-screen frame sequence would duplicate the background and make outcomes less flexible; retaining geometric-only sprites is simpler but does not match the approved artwork. The layered approach is selected for the requested separation. No source commit or remote publication is part of this task.

## Prepared files

All files are in [gameplay-v2](../../../assets/images/penalty/gameplay-v2/manifest.json). The manifest records exact prompts, generation method, crop grids, dimensions and provisional anchors.

| Layer | Prepared exports | Runtime role |
| --- | --- | --- |
| Background | One 320 x 240 PNG | Static stadium, green boards, goal and grass; no people, ball, targets or baked HUD |
| Striker | Three transparent 88 x 88 frames | Ready, backswing, follow-through |
| Keeper | Four transparent 56 x 56 frames | Ready, screen-left dive, screen-right dive, low collection |
| Ball | Transparent 18, 16, 14, 12, 10 and 8 pixel squares | One independent ball at different apparent distances |
| HUD/shadows | No baked images | Score, exact power thresholds, target selection, result markers, shadow and messages drawn by code |

Source sheets and their individual unscaled cells are preserved. Exports use grid cropping and nearest-neighbor resizing; no gameplay logic is encoded in these PNGs. The striker's support-foot anchors were measured from the alpha mask. Keeper glove points are provisional and must be calibrated in motion. Check contact timing: add or refine a boot-contact pose if the three current striker frames cannot meet the stationary ball convincingly. Do not make the ball jump to an unrelated location merely to fit a pose.

## Ball movement and action sequence

The current `main/penalty_ui.c` already calculates a 600 ms flight from elapsed time, an arc, a decreasing ball radius, and keeper movement. Preserve that approach instead of generating full-screen flight images. Move these calculations into host-testable layout/animation code when integrating the new art.

For normalized elapsed time `t` clamped to 0..1, interpolate x from the kick point to the selected target. Interpolate y toward the result's destination and subtract an arc term proportional to `4*t*(1-t)`. Select a smaller pre-exported ball as it recedes. This is an arcade presentation curve, not a physical simulation.

| Phase | Rendering behavior |
| --- | --- |
| Aim/charge | Ready striker, keeper idle, stationary ball, live target and meter |
| Kick | Short backswing/contact/follow-through sequence; launch only at the calibrated contact point |
| Flight | Update ball position and size from time; move keeper using its already locked decision; optional separately drawn shadow |
| Goal/perfect | Ball reaches the interior of the net; perfect-shot keeper arrives late |
| Save/weak | Ball meets the glove/low-collection point, then stops or becomes occluded; no continuing flight through the keeper |
| High miss | Ball travels above the crossbar and is clipped to the field viewport, never drawn over the HUD |
| Timeout | No fictitious kick or flight; show timeout feedback directly |

Do not change shot accounting, the perfect band, or the keeper's locked decision to make an animation fit. The kick lead-in must be budgeted explicitly: either fit it within the current flight presentation duration or change the presentation timing constant with tests. Do not silently delay model results behind the animation.

Provisional new-layout coordinates: goal interior approximately x90..230/y44..91, three target centers around x110/160/210, starting ball around x162/y168, and striker support foot around x120/y176. Calibrate against the native background and boot-contact pose; do not reuse the old renderer's target x80/160/240 blindly. Draw target/HUD labels in code rather than extracting them from the AI composite.

## Original integration checklist (steps 1–5 completed locally; device step pending)

1. Validate every frame over light/dark backgrounds, remove alpha fringes if needed, calibrate common foot/torso/glove anchors, and review a composited contact/flight sequence at native size.
2. Add a reproducible converter for the prepared art. Use a const RGB565 background and compatible compact alpha-bearing sprite resources; pre-size the ball offline. A full 320 x 240 RGB565 background requires 153,600 bytes of Flash, not an added full-screen RAM buffer. Account for decoded formats, cache and drawing work before choosing sprite format.
3. Integrate in `main/penalty_ui.c` with unchanged gameplay rules. Preserve LVGL locking, bounded object counts, input responsiveness, old/new dirty-region redraw, safe teardown, and portrait restoration. Static background means fixed artwork, not that damaged regions can be left unredrawn behind moving sprites.
4. Extend pure animation tests and actual LVGL captures for start/mid/end times, all directions/outcomes, glove contact, clipping, five-shot flow and repeated entry/exit. Inspect real rendered frames and resource usage, not only artwork.
5. Run static, firmware and complete gates. Deliver a new versioned full `.bin` plus checksum. Keep the current v0.3 acceptance/community artifacts unchanged.
6. Flash the new full image at `0x0` using the documented installation method, not file copying; back up existing settings because a full-image refresh can reset them. Then check appearance, direction keys, power timing, sound, return-to-cover, frame cadence and endurance on the device. No device connection is needed until this stage.

## Original artwork-step validation (2026-09-15, historical)

Image checks: PASS for the 320 x 240 background and 13 RGBA exports with transparent exterior and near-opaque interior. Some generated ball interiors use alpha 254 rather than 255; preserve or deliberately quantize this during resource conversion. Native-size background, ball and character samples inspected. Repository static checks and host regression tests: PASS. Build: NOT RUN (artwork only). Device tests: NOT RUN. Unverified: firmware conversion, composited animation, new firmware build and physical acceptance. The previous community binary checksum is unchanged. Source assets alone are not a device-ready game revision.
