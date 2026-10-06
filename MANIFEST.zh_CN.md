<p align="right">
  <strong>简体中文</strong> · <a href="MANIFEST.md">English</a>
</p>

# 导出清单

这是生成的应用层快照，不是第二套独立源码。

- Play ID / version: `penalty` / `2.0.0`
- Source commit: `18265d7f41a176d0a519de171340ce8c92b31f9d`
- AI Passport base: `ccd3576e304f7e17d9a0d4c12a2d05db379b14c0`
- Launcher contract: `1`

2026-10-06 导出复现：从固定基线新建副本，依次通过 dry run（未改文件）、白名单应用及 ESP-IDF 5.5.3 完整门禁。204 个导出文件均匹配记录的源码哈希，错误基线的 dry run 被拒绝。复现完整镜像为 `06f1be31466fa3849ea13a97a78469597b2a56b8590e66690d975e650b7ae312`。Release 仍分发此前实机核验的 app 及完整镜像 `6dd9489d301cb73e61e65f76e78f5deb680e23315c17732246b7d69b5b2a42ee`；重建仅作为复现证据，不替换发布二进制。

仅将下列精确文件应用到固定基线。先运行 `apply.sh --dry-run`，再应用；默认拒绝基线不符。脚本只复制 `export-files.txt`，完整 SHA-256 在 `export-manifest.json`。不要在存在未审阅本地改动的目标上应用。

Build / Host tests: PASS（源码完整门禁）；精确发布 app 的安装、独立校验及有界启动 PASS。普通版游戏、耐久、异常矩阵与 Launcher 验收仍未完成，详见 [验证记录](penalty/docs/validation.zh_CN.md)。

代码沿用 MIT；思源黑体为随附 OFL，合成音效为 CC0，美术为原创 AI 生成。详见 [素材说明](assets/README.zh_CN.md)。

## 替换的基线文件 (24)

```text
.gitignore
CMakeLists.txt
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
sdkconfig.defaults
tests/test_bsp_display_rounding.c
tests/test_demo_navigation.c
tools/validate.sh
```

## 新增文件 (180)

```text
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
assets/images/penalty/gameplay-v2/background-240x240.png
assets/images/penalty/gameplay-v2/background-320x240.png
assets/images/penalty/gameplay-v2/background-source-v1.0.png
assets/images/penalty/gameplay-v2/background-source.png
assets/images/penalty/gameplay-v2/ball-10.png
assets/images/penalty/gameplay-v2/ball-12.png
assets/images/penalty/gameplay-v2/ball-14.png
assets/images/penalty/gameplay-v2/ball-16.png
assets/images/penalty/gameplay-v2/ball-18.png
assets/images/penalty/gameplay-v2/ball-8.png
assets/images/penalty/gameplay-v2/ball-source.png
assets/images/penalty/gameplay-v2/blue-striker-0-88x88.png
assets/images/penalty/gameplay-v2/blue-striker-1-88x88.png
assets/images/penalty/gameplay-v2/blue-striker-2-88x88.png
assets/images/penalty/gameplay-v2/blue-striker-3-88x88.png
assets/images/penalty/gameplay-v2/blue-striker-4-88x88.png
assets/images/penalty/gameplay-v2/blue-striker-5-88x88.png
assets/images/penalty/gameplay-v2/blue-striker-6-88x88.png
assets/images/penalty/gameplay-v2/blue-striker-7-88x88.png
assets/images/penalty/gameplay-v2/blue-striker-8-88x88.png
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
assets/images/penalty/releases/v2.0.0/cover-zh.png
assets/images/penalty/releases/v2.0.0/shootout-create-zh.png
assets/images/penalty/releases/v2.0.0/shootout-join-zh.png
assets/music/penalty/penalty_sfx_pcm.c
assets/music/penalty/penalty_sfx_pcm.h
components/launcher_contract/CMakeLists.txt
components/launcher_contract/include/launcher_contract.h
components/launcher_contract/launcher_contract.c
main/Kconfig.projbuild
main/penalty_app.c
main/penalty_app.h
main/penalty_audio.c
main/penalty_audio.h
main/penalty_ble_diag.c
main/penalty_ble_diag.h
main/penalty_ble_fault.c
main/penalty_ble_fault.h
main/penalty_ble_protocol.c
main/penalty_ble_protocol.h
main/penalty_ble_reliable.c
main/penalty_ble_reliable.h
main/penalty_ble_session.c
main/penalty_ble_session.h
main/penalty_ble_session_core.c
main/penalty_ble_session_core.h
main/penalty_copy.c
main/penalty_copy.h
main/penalty_duel_model.c
main/penalty_duel_model.h
main/penalty_i18n.c
main/penalty_i18n.h
main/penalty_model.c
main/penalty_model.h
main/penalty_multiplayer.c
main/penalty_multiplayer.h
main/penalty_multiplayer_core.c
main/penalty_multiplayer_core.h
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
penalty/docs/plans/2026-09-23-shootout-mode-design.md
penalty/docs/plans/2026-09-23-shootout-mode-design.zh_CN.md
penalty/docs/plans/2026-09-23-shootout-mode-implementation-plan.md
penalty/docs/plans/2026-09-23-shootout-mode-implementation-plan.zh_CN.md
penalty/docs/plans/2026-09-24-ble-duel-v2-design.md
penalty/docs/plans/2026-09-24-ble-duel-v2-design.zh_CN.md
penalty/docs/releases/v2.0.0.md
penalty/docs/releases/v2.0.0.zh_CN.md
penalty/docs/validation.md
penalty/docs/validation.zh_CN.md
penalty/publishing/community-cover-update.json
penalty/publishing/receipt.json
penalty/publishing/submission-v2.0.0.json
penalty/publishing/submission.json
sdkconfig.dev-ble-faults
tests/penalty_preview/CMakeLists.txt
tests/penalty_preview/lv_conf.h
tests/penalty_preview/mp_preview.c
tests/penalty_preview/preview.c
tests/test_penalty_art_contract.py
tests/test_penalty_audio.c
tests/test_penalty_ble_fault.c
tests/test_penalty_ble_fault_contract.py
tests/test_penalty_ble_protocol.c
tests/test_penalty_ble_reliable.c
tests/test_penalty_ble_session_core.c
tests/test_penalty_copy.c
tests/test_penalty_duel_model.c
tests/test_penalty_font_contract.py
tests/test_penalty_i18n.c
tests/test_penalty_m5_contract.py
tests/test_penalty_m5_resources.c
tests/test_penalty_model.c
tests/test_penalty_multiplayer_core.c
tests/test_penalty_multiplayer_recovery.c
tests/test_penalty_preferences.c
tests/test_penalty_scene.c
tests/test_penalty_shell_contract.py
tools/archive_firmware.py
tools/convert-penalty-cover.py
tools/convert-penalty-gameplay.py
tools/generate-penalty-fonts.py
tools/generate-penalty-sfx.py
tools/prepare-penalty-action-art.py
tools/preview-penalty.sh
tools/style-penalty-art.py
```
