<p align="right">
  <strong>简体中文</strong> · <a href="2026-09-22-portrait-gameplay-design.md">English</a>
</p>

# 竖屏玩法实施计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**目标：** 将 Penalty 从“竖屏封面＋横屏游戏”改为全程 240 x 320 竖屏，同时保留六格规则、概率、难度、声音、多语言、持久保存与五球流程。

**架构：** 继续分离纯 C 模型、场景计算、LVGL 渲染和应用适配。将方向相关的状态／输入名称改成竖屏封面与上下选择语义，从已确认美术中导出居中的竖屏球场，并重新布局所有 LVGL 坐标，不增加运行时缩放或全屏缓冲。

**技术栈：** C11 纯模型与场景测试、LVGL 9.5.0、ESP-IDF 5.5.3、确定性 Pillow 素材转换、8 MB Flash 且无 PSRAM 的 ESP32-C3。

---

## 已确认行为

- 从封面、标题到结算、退出全程保持竖屏；不再提示旋转设备，也不再切换横屏。
- 保留现有封面美术，把旋转提示改成简短的“按 OK 开始”。
- 每球从中上开始。
- DOWN 按“中上→中下→左上→左下→右上→右下→中上”循环。
- UP 完全反向循环；同一列内 DOWN 向下、UP 向上。
- OK 仍负责锁定目标、开始蓄力、锁定力度和射门；长按 OK 仍返回封面。
- 门将六格、进球概率、难度绿区、绿区减速、五球流程、声音、中英文、语言持久保存、电量与结算规则均不改变。

## 竖屏布局

逻辑画面始终为 240 x 320。标题／游戏球场位于上部，使用从当前已确认 320 x 240 球场居中导出的 240 x 240 素材。角色仍以原尺寸从 Flash 读取，只重新校准竖屏坐标。HUD 使用剩余纵向空间：顶部状态栏、其下球场、192 像素宽力度条、结果／互动文案和底部长按 OK 提示。设置与说明利用新增纵向空间，不再压缩横屏文字。

不引入运行时图片缩放、PNG 解码、额外帧缓冲、网络能力、持久格式或分区变化。竖屏本来就是 BSP 默认方向，因此应用不再请求旋转。声音、电量、过期按键、内存不足等降级方式保持不变。

### 任务 1：用失败测试锁定上下按键与六格顺序

**文件：**
- 修改：`tests/test_penalty_model.c`
- 修改：`main/penalty_model.h`
- 修改：`main/penalty_model.c`

1. 将模型测试改用 `PENALTY_INPUT_UP`／`PENALTY_INPUT_DOWN`，验证从中上开始的完整正反循环。
2. 运行聚焦模型测试，确认旧的 LEFT／RIGHT 分行实现按预期失败。
3. 重命名封面状态与导航输入，使用明确查表实现按列纵向循环。
4. 重跑聚焦模型测试及全部纯 C 模型测试。

### 任务 2：从应用壳移除运行时旋转

**文件：**
- 修改：`main/penalty_app.c`
- 修改：`main/penalty_audio.c`
- 修改：`tests/test_penalty_shell_contract.py`
- 修改：`tests/test_penalty_audio.c`

1. 新增合约断言：Penalty 不再进入横屏，实体 UP／DOWN 直接映射到模型 UP／DOWN。
2. 运行壳层／声音测试，确认旧旋转路径不能满足新合约。
3. 删除进入横屏与旋转失败恢复；停止时仍可幂等恢复竖屏作为安全措施。
4. 同步封面进入标题的声音状态名并重跑聚焦测试。

### 任务 3：导出竖屏球场并重新校准场景

**文件：**
- 修改：`tools/convert-penalty-gameplay.py`
- 修改：`tools/style-penalty-art.py`
- 修改：`assets/images/penalty/gameplay-v2/manifest.json`
- 新建：`assets/images/penalty/gameplay-v2/background-240x240.png`
- 重新生成：`assets/images/penalty/gameplay-v2/gameplay_assets.c`
- 重新生成：`assets/images/penalty/gameplay-v2/gameplay_assets.h`
- 修改：`main/penalty_scene.c`
- 修改：`tests/test_penalty_art_contract.py`
- 修改：`tests/test_penalty_scene.c`

1. 先增加失败的素材／场景断言，覆盖 240 像素背景、竖屏目标中心、角色／足球边界及上下扑救接触点。
2. 从已确认球场居中裁切，确定性生成新背景；保留横向草坪条纹、球门、深绿广告板和黑色门将素材。
3. 更新转换器清单和只读资源，不增加运行时解码或缩放。
4. 重新校准目标、门将、球员、足球、阴影与局部刷新区域，并让场景测试通过。

### 任务 4：将全部 LVGL 状态重排为 240 x 320

**文件：**
- 修改：`main/penalty_ui.c`
- 修改：`main/penalty_i18n.c`
- 修改：`assets/images/penalty/cover-240x320.png`
- 重新生成：`assets/images/penalty/cover_rgb565.c`
- 修改：`tests/penalty_preview/preview.c`
- 修改：`tests/test_penalty_i18n.c`
- 修改：`tests/test_penalty_art_contract.py`

1. 先增加失败的预览／合约检查：全部页面保持竖屏，封面没有旋转提示，标题、设置、说明、游戏和结算文字均适配 240 像素。
2. 屏幕固定 240 x 320，重排球场／HUD，将力度条缩至 192 像素，并用竖屏常量替换横屏宽度。
3. 将说明和封面提示改为 UP／DOWN 竖屏操作；尽量复用现有中文字库，只有缺字时才重新生成。
4. 确定性修改封面提示并重新生成 RGB565 资源。
5. 运行实际 LVGL 预览，覆盖双语、六格、全部结果及连续 50 次进入退出。

### 任务 5：同步产品文档和验证记录

**文件：**
- 修改：`penalty/README.md`
- 修改：`penalty/README.zh_CN.md`
- 修改：`penalty/docs/game-design.md`
- 修改：`penalty/docs/game-design.zh_CN.md`
- 修改：`penalty/docs/validation.md`
- 修改：`penalty/docs/validation.zh_CN.md`
- 修改：`assets/README.md`
- 修改：`assets/README.zh_CN.md`

1. 将当前状态中的横屏说明改成已确认竖屏行为；旧版本记录仍作为历史保留。
2. 记录实际宿主渲染证据、固件标识与剩余真机检查。
3. 运行仓库链接／语言检查与 `git diff --check`。

### 任务 6：完整验证与固件打包

**文件：**
- 生成但不提交：`build/FoloToy-AI-Passport-full.bin`
- 生成但不提交：带版本名的竖屏验收固件

1. 实现过程中运行模型、场景、本地化、美术、壳层与 LVGL 预览聚焦检查。
2. 执行 `./tools/validate.sh --static`。
3. 激活 ESP-IDF 5.5.3 后执行 `./tools/validate.sh`。
4. 核对合并镜像大小、偏移、分区容量与 SHA-256；二进制不进入 Git。
5. 分别报告 Build、Host tests、Device tests 与 Unverified。实现完成后主动询问是否刷机，但刷写仍需用户明确授权。

本计划中的 Git 提交只作为检查点；按仓库规则，必须另行获得用户授权后才能执行。
