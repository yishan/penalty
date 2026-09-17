<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

### Penalty Simplified Chinese subset

- `fonts/penalty/penalty_font_zh_14.c` and `penalty_font_zh_20.c` are generated LVGL 4-bpp font sources for the two sizes used by the game; `penalty_fonts.h` exposes them to the renderer.
- The subset contains ASCII plus every non-ASCII character currently used by `main/penalty_i18n.c` and `main/penalty_copy.c`. `tests/test_penalty_font_contract.py` rejects missing glyphs, so adding Chinese copy requires regenerating both sizes.
- Source: `managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf` (Source Han Sans SC, SIL Open Font License 1.1). The redistributable license text is preserved at `fonts/penalty/LICENSE-SourceHanSansSC.txt`.
- Rebuild with `python3 tools/generate-penalty-fonts.py`; the script pins `lv_font_conv` 1.5.3 and keeps converter downloads outside the repository cache. Normal firmware builds use the tracked C output and need neither Node.js nor the source OTF at build time.
- The font bitmaps stay in read-only Flash. No complete CJK font or runtime font loader is added; the actual firmware size and physical-display legibility are acceptance items.

## Images

Store reusable source images and generated display assets in `images/`.

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Penalty portrait cover

- `images/penalty/cover-concept.png`: approved original portrait concept, including a drawn battery icon.
- `images/penalty/cover-source-v1.0.png`: preserved pre-refresh source with the drawn battery removed.
- `images/penalty/cover-source.png`: current source with horizontal pitch bands, a black keeper kit, and the exact four-glyph Chinese game mark above the rotation diagram. The portrait cover intentionally has no battery overlay; live battery text appears only after entering the landscape game.
- `images/penalty/cover-240x320.png`: nearest-neighbor 240 x 320 reference export.
- `images/penalty/cover-tagline-128x20.png`: exact rendered cover-mark region used by the art contract; cream block glyphs, dark outline, and offset shadow echo the `PENALTY` treatment.
- `images/penalty/cover_rgb565.c`: generated 153,600-byte, native RGB565 `const` pixel array in read-only Flash. LVGL uses the existing 4,800-pixel draw buffer; no full-screen RAM decode, alpha layer, or runtime resizing is introduced.
- Source: generated and edited with the built-in imagegen tool for this project from the user's approved brief. Original generic football artwork; no third-party reference photos, club marks, or font files were supplied. Not a separately licensed third-party asset pack.
- Rebuild the approved style pass with `python3 tools/style-penalty-art.py`, then run `python3 tools/convert-penalty-cover.py` (Pillow 9.1 or later). Normal firmware builds use the tracked C asset and do not require Pillow. Inspect actual RGB565/LVGL captures, not only this PNG.
- Original source-art edit prompt: "Edit this approved pixel-art PENALTY portrait game cover for firmware integration. Change ONLY the small battery icon at the extreme upper right: remove that entire cream/green/black battery graphic and restore the matching plain blue sky behind it, leaving this area clear so firmware can draw a live battery indicator. Keep absolutely everything else unchanged: portrait 3:4 composition, PENALTY lettering, characters, red kit, goalkeeper, goal, pitch, spectators, clockwise device diagram, bottom text TURN RIGHT 90° and THEN PRESS OK, color palette and pixel art. Do not add any symbols or text. Preserve the original artwork faithfully. This is a source-art edit, not a device mockup." The later v0.6 UI decision keeps that restored sky clear instead of adding battery text on the cover.

## Penalty community cover

The [submitted v0.3 community cover](images/penalty/community-cover-v0.3.png) and [unuploaded v0.4 label-free cover](images/penalty/community-cover-v0.4-no-label.png) are preserved as historical evidence. The current [v1.1 community-cover candidate](images/penalty/community-cover-v1.1-horizontal.png) applies the same horizontal pitch bands, black keeper kit, and Chinese game mark as the firmware art. It remains an illustrative asset, not a device screenshot, and has not been uploaded; project 402 and revision 735 are unchanged.

## Penalty gameplay visual proposal

- [Generated source](images/penalty/gameplay-concept-v1-source.png): 1448 x 1086 landscape concept using the community cover as a style reference. Built-in imagegen; same artwork provenance as the covers above.
- [Native-size preview](images/penalty/gameplay-concept-v1-320x240.png): 320 x 240 nearest-neighbor export for readability inspection. It is an AI-generated design mockup, not a renderer capture or device screenshot; no firmware or community submission has been changed.
- [Generation record and exact prompt](images/penalty/gameplay-concept-v1.json).
- Proposed composition: top shot/goal counter, blue stadium and goalkeeper, red striker and ball in the foreground, three targets, bottom power meter and five shot markers. Sample state is shot 3/5 with one goal. Simplify dense net/crowd detail during production; draw text, target states and exact 60-80/90 power boundaries in code rather than baking this composite into gameplay.

## Penalty separated gameplay layers

The [gameplay-v2 manifest](images/penalty/gameplay-v2/manifest.json) records a clean background, three striker frames, four keeper frames, and six ball sizes, together with the original built-in imagegen prompts and calibrated animation anchors. The deterministic v1.1 style pass changes the pitch to horizontal light/dark bands and the keeper kit to black/dark charcoal while retaining the goal, field lines, transparent sprite geometry, gloves, skin, and anchors. `*-v1.0.png` files preserve the pre-refresh source cells. These are not device screenshots.

Integrated into the renderer without changing the shooting rules. Run `tools/style-penalty-art.py` before `tools/convert-penalty-gameplay.py`; the converter reproducibly creates `gameplay_assets.c/.h`: 264,180 pixel bytes in read-only Flash, using native RGB565 for the background and planar RGB565A8 for sprites. Alpha 0–1 is normalized to 0 and 254–255 to 255; other edge alpha is retained. Run the converter with `--check` to verify generated files (Pillow is an offline conversion dependency only). No PNG decoder, runtime resizing, new full-screen RAM buffer, or image objects per frame are introduced. See the [integration design](../penalty/docs/plans/2026-09-15-pixel-art-integration-design.md) for calibrated boot/glove anchors, real-time flight, tests and pending device acceptance. Previous firmware/submission copies are preserved.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.

### Penalty hybrid sound effects

- `music/penalty/penalty_sfx_pcm.c/.h`: deterministic original kick, glove/save,
  and restrained goal-impact samples. They are 16 kHz, signed 16-bit mono PCM
  arrays streamed from read-only Flash in small chunks.
- Durations are 80 ms (kick), 100 ms (save), and 110 ms (goal); generated size
  is 9,280 PCM bytes plus small symbol/alignment overhead.
- Source/license: generated specifically for this repository from mathematical
  oscillators and seeded noise, with no recorded or third-party input; CC0-1.0.
- Rebuild with `python3 tools/generate-penalty-sfx.py`; verify tracked output with
  `python3 tools/generate-penalty-sfx.py --check`. The generated arrays are used
  by `main/penalty_app.c`; the script is not required for a normal firmware build.
