# Manifest

Exactly which files this application layer touches in the FoloToy AI Passport baseline, and where each one lands.

- **Play ID / version**: `penalty` / `1.4.2`
- **Authoritative source commit**: `d5f08e7a1d345f9d9cac5552167eaf4166c8f5d0`
- **Launcher contract**: `1`
- **Base commit**: `ccd3576` (2026-09-14) — `fix(battery): restore previous CW2017 profile`
  Full hash: `ccd3576e304f7e17d9a0d4c12a2d05db379b14c0`
- **Added**: 136 files — new paths, no conflict risk when you update upstream.
- **Modified**: 22 files — baseline files this application rewrites. These are overwritten by `apply.sh`.

The base commit is the tree these files were cut against. `apply.sh` checks the target commit and stops on a mismatch unless the operator deliberately passes `--force`. Always run `--dry-run` before applying the layer.

## Modified baseline files

Four of these are in the board support layer, which is why this is not purely an application change. The display pair adds the landscape rotation entry point and makes the corner mask follow the active logical resolution; the audio pair turns a silent volume failure into a returned error.

```text
.gitignore
assets/README.md
assets/README.zh_CN.md
components/bsp/include/bsp_audio.h
components/bsp/include/bsp_display.h
components/bsp/src/bsp_audio.c
components/bsp/src/bsp_display_lvgl.c
docs/README.md
docs/README.zh_CN.md
docs/development/engineering/build-and-test.md
docs/development/engineering/build-and-test.zh_CN.md
docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md
docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md
main/CMakeLists.txt
main/demo.h
main/demo_audio.c
main/demo_navigation.c
main/demo_navigation.h
main/main.c
tests/test_bsp_display_rounding.c
tests/test_demo_navigation.c
tools/validate.sh
```

## Added files

```text
components/launcher_contract/CMakeLists.txt
components/launcher_contract/include/launcher_contract.h
components/launcher_contract/launcher_contract.c
assets/fonts/penalty/LICENSE-SourceHanSansSC.txt
assets/fonts/penalty/penalty_font_zh_14.c
assets/fonts/penalty/penalty_font_zh_20.c
assets/fonts/penalty/penalty_fonts.h
assets/images/penalty/community-cover-v0.3.png
assets/images/penalty/community-cover-v0.4-no-label.png
assets/images/penalty/community-cover-v1.1-horizontal.png
assets/images/penalty/community-cover-v1.2-horizontal-cn-wide.png
assets/images/penalty/cover-240x320.png
assets/images/penalty/cover-concept.png
assets/images/penalty/cover-source-v1.0.png
assets/images/penalty/cover-source.png
assets/images/penalty/cover-tagline-128x16.png
assets/images/penalty/cover-tagline-128x20.png
assets/images/penalty/cover_rgb565.c
assets/images/penalty/gameplay-concept-v1-320x240.png
assets/images/penalty/gameplay-concept-v1-source.png
assets/images/penalty/gameplay-concept-v1.json
assets/images/penalty/gameplay-v2/background-320x240.png
assets/images/penalty/gameplay-v2/background-240x240.png
assets/images/penalty/gameplay-v2/background-source-v1.0.png
assets/images/penalty/gameplay-v2/background-source.png
assets/images/penalty/gameplay-v2/blue-striker-0-88x88.png
assets/images/penalty/gameplay-v2/blue-striker-1-88x88.png
assets/images/penalty/gameplay-v2/blue-striker-2-88x88.png
assets/images/penalty/gameplay-v2/blue-striker-3-88x88.png
assets/images/penalty/gameplay-v2/blue-striker-4-88x88.png
assets/images/penalty/gameplay-v2/blue-striker-5-88x88.png
assets/images/penalty/gameplay-v2/blue-striker-6-88x88.png
assets/images/penalty/gameplay-v2/blue-striker-7-88x88.png
assets/images/penalty/gameplay-v2/blue-striker-8-88x88.png
assets/images/penalty/gameplay-v2/ball-10.png
assets/images/penalty/gameplay-v2/ball-12.png
assets/images/penalty/gameplay-v2/ball-14.png
assets/images/penalty/gameplay-v2/ball-16.png
assets/images/penalty/gameplay-v2/ball-18.png
assets/images/penalty/gameplay-v2/ball-8.png
assets/images/penalty/gameplay-v2/ball-source.png
assets/images/penalty/gameplay-v2/gameplay_assets.c
assets/images/penalty/gameplay-v2/gameplay_assets.h
assets/images/penalty/gameplay-v2/keeper-0-56x56.png
assets/images/penalty/gameplay-v2/keeper-1-56x56.png
assets/images/penalty/gameplay-v2/keeper-2-56x56.png
assets/images/penalty/gameplay-v2/keeper-3-56x56.png
assets/images/penalty/gameplay-v2/keeper-4-56x56.png
assets/images/penalty/gameplay-v2/keeper-5-56x56.png
assets/images/penalty/gameplay-v2/keeper-6-56x56.png
assets/images/penalty/gameplay-v2/keeper-extra-3-source.png
assets/images/penalty/gameplay-v2/keeper-sheet-source-v1.0.png
assets/images/penalty/gameplay-v2/keeper-sheet-source.png
assets/images/penalty/gameplay-v2/keeper-source-0-v1.0.png
assets/images/penalty/gameplay-v2/keeper-source-0.png
assets/images/penalty/gameplay-v2/keeper-source-1-v1.0.png
assets/images/penalty/gameplay-v2/keeper-source-1.png
assets/images/penalty/gameplay-v2/keeper-source-2-v1.0.png
assets/images/penalty/gameplay-v2/keeper-source-2.png
assets/images/penalty/gameplay-v2/keeper-source-3-v1.0.png
assets/images/penalty/gameplay-v2/keeper-source-3.png
assets/images/penalty/gameplay-v2/manifest.json
assets/images/penalty/gameplay-v2/striker-0-88x88.png
assets/images/penalty/gameplay-v2/striker-1-88x88.png
assets/images/penalty/gameplay-v2/striker-2-88x88.png
assets/images/penalty/gameplay-v2/striker-3-88x88.png
assets/images/penalty/gameplay-v2/striker-4-88x88.png
assets/images/penalty/gameplay-v2/striker-blue-9-source.png
assets/images/penalty/gameplay-v2/striker-red-5-source.png
assets/images/penalty/gameplay-v2/striker-sheet-source.png
assets/images/penalty/gameplay-v2/striker-source-0.png
assets/images/penalty/gameplay-v2/striker-source-1.png
assets/images/penalty/gameplay-v2/striker-source-2.png
assets/music/penalty/penalty_sfx_pcm.c
assets/music/penalty/penalty_sfx_pcm.h
main/penalty_app.c
main/penalty_app.h
main/penalty_audio.c
main/penalty_audio.h
main/penalty_copy.c
main/penalty_copy.h
main/penalty_i18n.c
main/penalty_i18n.h
main/penalty_model.c
main/penalty_model.h
main/penalty_preferences.c
main/penalty_preferences.h
main/penalty_scene.c
main/penalty_scene.h
main/penalty_ui.c
main/penalty_ui.h
penalty/README.md
penalty/README.zh_CN.md
penalty/VERSION
penalty/docs/community-submission.md
penalty/docs/community-submission.zh_CN.md
penalty/docs/game-design.md
penalty/docs/game-design.zh_CN.md
penalty/docs/localization.md
penalty/docs/localization.zh_CN.md
penalty/docs/plans/2026-09-15-pixel-art-integration-design.md
penalty/docs/plans/2026-09-15-pixel-art-integration-design.zh_CN.md
penalty/docs/plans/2026-09-16-audio-feedback-design.md
penalty/docs/plans/2026-09-16-audio-feedback-design.zh_CN.md
penalty/docs/plans/2026-09-16-dynamic-copy-design.md
penalty/docs/plans/2026-09-16-dynamic-copy-design.zh_CN.md
penalty/docs/plans/2026-09-22-portrait-gameplay-design.md
penalty/docs/plans/2026-09-22-portrait-gameplay-design.zh_CN.md
penalty/docs/plans/2026-09-23-goalkeeper-mode-design.md
penalty/docs/plans/2026-09-23-goalkeeper-mode-design.zh_CN.md
penalty/docs/plans/2026-09-23-goalkeeper-mode-implementation-plan.md
penalty/docs/plans/2026-09-23-goalkeeper-mode-implementation-plan.zh_CN.md
penalty/docs/validation.md
penalty/docs/validation.zh_CN.md
penalty/publishing/community-cover-update.json
penalty/publishing/receipt.json
penalty/publishing/submission.json
tests/penalty_preview/CMakeLists.txt
tests/penalty_preview/lv_conf.h
tests/penalty_preview/preview.c
tests/test_penalty_art_contract.py
tests/test_penalty_audio.c
tests/test_penalty_copy.c
tests/test_penalty_font_contract.py
tests/test_penalty_i18n.c
tests/test_penalty_model.c
tests/test_penalty_preferences.c
tests/test_penalty_scene.c
tests/test_penalty_shell_contract.py
tools/convert-penalty-cover.py
tools/convert-penalty-gameplay.py
tools/generate-penalty-fonts.py
tools/generate-penalty-sfx.py
tools/preview-penalty.sh
tools/prepare-penalty-action-art.py
tools/style-penalty-art.py
```

## Regenerating this manifest

In the source repository that produced this layer:

```bash
git diff --name-status ccd3576 d5f08e7
```
