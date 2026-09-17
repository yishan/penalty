<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

### Penalty 简体中文字库子集

- `fonts/penalty/penalty_font_zh_14.c` 与 `penalty_font_zh_20.c` 是游戏两种字号对应的 LVGL 4-bpp 生成字库，`penalty_fonts.h` 供渲染器引用。
- 子集包含 ASCII，以及 `main/penalty_i18n.c`、`main/penalty_copy.c` 当前使用的全部非 ASCII 字符。`tests/test_penalty_font_contract.py` 会拒绝缺字；新增中文文案后必须重新生成两种字号。
- 来源：`managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf`（思源黑体简体中文，SIL Open Font License 1.1）。可再分发的许可全文保存在 `fonts/penalty/LICENSE-SourceHanSansSC.txt`。
- 使用 `python3 tools/generate-penalty-fonts.py` 重建；脚本固定使用 `lv_font_conv` 1.5.3，并将转换器下载缓存放在仓库外。普通固件构建直接使用已跟踪的 C 文件，不依赖 Node.js，也不要求构建时存在源 OTF。
- 字形位图只占用只读 Flash，不引入完整 CJK 字库或运行时字体加载器；实际固件容量和实物屏幕清晰度仍属于验收项。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## Penalty 竖屏封面

- `images/penalty/cover-concept.png`：已确认的原始竖屏视觉稿，含绘制的电池图标。
- `images/penalty/cover-source-v1.0.png`：保留的美术刷新前源图，已去除固定电池图标。
- `images/penalty/cover-source.png`：当前源图，草坪改为横向明暗带，门将改为黑色球衣，并在旋转图示上方加入精确中文标识“点球决胜”。竖屏封面刻意不叠加电量；实时电量仅在进入横屏游戏后显示。
- `images/penalty/cover-240x320.png`：最近邻缩放的 240 x 320 参考导出图。
- `images/penalty/cover-tagline-128x20.png`：美术契约使用的精确封面字样区域；米白粗字、深色描边和错位阴影呼应 `PENALTY` 风格。
- `images/penalty/cover_rgb565.c`：生成的原生 RGB565 `const` 像素数组，在只读 Flash 中占 153,600 字节。LVGL 沿用 4,800 像素绘制缓冲，不新增整屏 RAM 解码、透明图层或运行时缩放。
- 来源：使用内置 imagegen 工具，依据用户确认的方案为本项目生成并编辑。为中性足球题材原创生成图，未提供第三方参考照片、俱乐部标志或字体文件，不属于另行许可的第三方素材包。
- 先用 `python3 tools/style-penalty-art.py` 重建已确认的风格调整，再执行 `python3 tools/convert-penalty-cover.py`（需要 Pillow 9.1 或以上）。普通固件构建直接使用已纳入仓库的 C 资源，无需 Pillow。验收应查看实际 RGB565／LVGL 截图，而不只是 PNG。
- 原始源图编辑提示词："Edit this approved pixel-art PENALTY portrait game cover for firmware integration. Change ONLY the small battery icon at the extreme upper right: remove that entire cream/green/black battery graphic and restore the matching plain blue sky behind it, leaving this area clear so firmware can draw a live battery indicator. Keep absolutely everything else unchanged: portrait 3:4 composition, PENALTY lettering, characters, red kit, goalkeeper, goal, pitch, spectators, clockwise device diagram, bottom text TURN RIGHT 90° and THEN PRESS OK, color palette and pixel art. Do not add any symbols or text. Preserve the original artwork faithfully. This is a source-art edit, not a device mockup." 后续 v0.6 界面决定让这块还原的天空保持干净，不再在封面添加电量文字。

## Penalty 社区封面

[已提交的 v0.3 社区封面](images/penalty/community-cover-v0.3.png)和[未上传的 v0.4 无底部标识封面](images/penalty/community-cover-v0.4-no-label.png)均作为历史证据保留。当前的 [v1.1 社区封面候选](images/penalty/community-cover-v1.1-horizontal.png)同步采用横向草坪明暗带、黑色门将球衣及“点球决胜”字样。它仍是示意素材，不是实机截图，也尚未上传；项目 402 和修订 735 均未改变。

## Penalty 游戏画面提案

- [生成源图](images/penalty/gameplay-concept-v1-source.png)：1448 x 1086 横屏稿，以社区封面为画风参考，用内置 imagegen 生成；作品来源同上方封面。
- [原尺寸预览](images/penalty/gameplay-concept-v1-320x240.png)：最近邻导出的 320 x 240 图，用于检查可读性。这是 AI 生成的设计示意，不是渲染器或实机截图；未修改固件或社区提交内容。
- [生成记录与完整提示词](images/penalty/gameplay-concept-v1.json)。
- 布局提案：顶部球次／进球数，蓝色球场与门将，近景红衣射手和足球，三个落点，底部力度条和五球记录。示例状态为第 3/5 球、已进一球。正式制作时简化密集球网／观众细节；文字、落点状态及精确的 60–80／90 力度边界由代码绘制，不将这张合成图直接作为游戏画面。

## Penalty 已拆分游戏图层

[gameplay-v2 清单](images/penalty/gameplay-v2/manifest.json)记录纯背景、射手三帧、门将四帧、足球六种尺寸，以及原始内置 imagegen 提示词和已校准动画锚点。确定性的 v1.1 风格处理把草坪改为横向明暗带、门将球衣改为黑／深灰色，同时保留球门、场地线、透明精灵几何、手套、肤色及锚点；`*-v1.0.png` 文件保留刷新前源图。这些不是实机截图。

当前渲染接入不改变射门规则。先执行 `tools/style-penalty-art.py`，再运行 `tools/convert-penalty-gameplay.py`；转换器可复现生成 `gameplay_assets.c/.h`：264,180 字节像素存于只读 Flash，背景为原尺寸 RGB565，角色为分平面的 RGB565A8。Alpha 0–1 归零，254–255 归为 255，其余边缘透明度保留。转换器加 `--check` 可校验生成文件，Pillow 仅为离线转换依赖。没有新增 PNG 解码、运行时缩放、整屏 RAM 缓冲或每帧图片对象。校准后的脚／手套锚点、实时飞行、测试与待完成的真机验收见[接入设计](../penalty/docs/plans/2026-09-15-pixel-art-integration-design.zh_CN.md)。旧固件及社区提交副本保留。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。

### Penalty 混合音效

- `music/penalty/penalty_sfx_pcm.c/.h`：以固定算法生成的原创踢球、手套扑救和克制的进球冲击短样本；格式为 16 kHz、有符号 16 位单声道 PCM，由只读 Flash 分块送出。
- 时长分别为 80 ms（踢球）、100 ms（扑救）和 110 ms（进球）；PCM 总大小 9,280 字节，另有少量符号／对齐开销。
- 来源／许可：专为本仓库用数学振荡器和固定种子噪声生成，不含录音或第三方输入；CC0-1.0。
- 用 `python3 tools/generate-penalty-sfx.py` 重建，用 `python3 tools/generate-penalty-sfx.py --check` 校验已跟踪输出。生成数组由 `main/penalty_app.c` 使用，普通固件构建不需要运行该脚本。
