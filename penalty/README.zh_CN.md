<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# Penalty 点球游戏

为 AI Passport 设计的三键、离线竖屏点球小游戏。设备保持竖直：UP／DOWN 按列上下轮询六个目标，OK 仍为确认。

状态：v1.4 在原有射门挑战旁新增独立“守门挑战”。两种模式均保持 240 × 320 竖屏，复用六格循环和两次确认的时机条。守门模式中，蓝衣 AI 射手用真实的身体／脚部预备动作提示六个方向；简单／普通／困难分别显示 900／600／350 ms，玩家选择黑衣门将扑救方向并把第二次 OK 按在合适时机。宿主与固件证据见[验证记录与真机清单](docs/validation.zh_CN.md)；本版本仍需实机验收。

## 从这里开始

- [游戏策划大纲](docs/game-design.zh_CN.md)：范围、操作、规则、页面、技术结构、里程碑和验收标准。
- [守门模式设计](docs/plans/2026-09-23-goalkeeper-mode-design.zh_CN.md)：已确认的视角、提示时长、扑救概率、动作素材和验收边界。
- [像素素材分层与接入](docs/plans/2026-09-15-pixel-art-integration-design.zh_CN.md)：已接入素材、触球／飞行校准、资源转换和实机检查。
- [混合声音反馈设计](docs/plans/2026-09-16-audio-feedback-design.zh_CN.md)：已确认的声音风格、静音原因、事件映射、交付里程碑和真机验收。
- [动态文案设计](docs/plans/2026-09-16-dynamic-copy-design.zh_CN.md)：教学边界、鼓励文案、结果解说、固定选择及界面检查。
- [多语言设计](docs/localization.zh_CN.md)：语言菜单、持久保存边界、中文字库子集、溢出检查与降级方式。
- [验证记录与真机清单](docs/validation.zh_CN.md)：验证证据、限制和试玩方法。
- [社区提交记录](docs/community-submission.zh_CN.md)：中英文资料、示意封面与实际审核状态。
- [仓库 Agent 约定](../AGENTS.zh_CN.md)：开发前的必读入口。
- [AI 开发指南](../docs/development/ai-guide.zh_CN.md)：应用与 BSP 边界及生命周期约定。
- [构建与测试](../docs/development/engineering/build-and-test.zh_CN.md)：统一验证命令。

## 项目结构

```text
penalty/
  README.md / README.zh_CN.md
  docs/
    game-design.md / game-design.zh_CN.md
    validation.md / validation.zh_CN.md
```

按用户要求，以 `penalty/` 作为项目目录，集中保存策划与后续游戏专属设计素材。有实际内容时再创建素材、原型子目录。

游戏复用现有仓库构建：[规则模型](../main/penalty_model.c)、[音频事件映射](../main/penalty_audio.c)、[界面](../main/penalty_ui.c)和[应用适配](../main/penalty_app.c)位于 `main/`，可复用旋转及可观测音量控制位于 `components/bsp/`。[规则／音频测试](../tests/test_penalty_audio.c)和[实际 LVGL 渲染测试](../tests/penalty_preview/preview.c)位于 `tests/`。`penalty/` 中没有复制 BSP 或另建 ESP-IDF 工程。

## 试玩与验证

开机显示带 `PENALTY`／“点球决胜”固定游戏名的竖屏像素封面，设备不用旋转，直接按 OK。UP／DOWN 选择“射门”“守门”“设置”或“说明”。“设置”中包含难度、音效、语言和返回；没有有效已保存偏好时默认显示简体中文。选择“语言”可切换英文／简体中文，当前页面立即刷新，有效选择在退出游戏或设备重启后仍保留。选择“难度”循环简单／普通／困难。

“射门”保留原有规则：绿区宽度为 20／11／6 格，每球随机换位置。门将等概率预判六格后，深绿线平均 90% 进球，其余绿区 65%，正常力度绿区外 25%。

“守门”沿用相同的五球流程和射手背后视角。先观察蓝衣射手的起始动作，选择远端黑衣门将的扑救方向，第一次按 OK 开始时机条，第二次按 OK 扑救。同格时深绿线／普通绿区／绿区外分别有 90%／65%／25% 扑出；同列另一行分别为 35%／20%／5%；不同列无法扑出。0–29 为起跳过早，91–100 为起跳过晚。AI 射门方向和判定随机数在操作前锁定，不会重抽。

每球默认从中上开始。DOWN 按“中上→中下→左上→左下→右上→右下→中上”循环；UP 完全反向。这样同一列内的目标移动与实体 UP／DOWN 方向一致，每列的上下两格走完后才切换到下一列。第一次按 OK 锁定目标并开始时机条，第二次按 OK 执行动作；游标进入绿区后减为原速的五分之一。五次尝试后按进球数或扑救数结算，长按 OK 取消本局并返回竖屏封面。关闭音效时会先播一次关闭提示，重新开启时播放开启提示；“说明”同时解释两种玩法。重玩和返回标题保留当前模式、难度及静音；返回封面则重置本局。只有语言持久保存，成绩、难度与静音均不保存；精确参数见[游戏规则](docs/game-design.zh_CN.md)。

在仓库根目录执行：

```bash
./tools/validate.sh --static
./tools/validate.sh --firmware
./tools/validate.sh
```

项目版本记录在 `penalty/VERSION`。固件门禁成功后，会保留通用合并镜像，并同时
生成交付文件 `build/FoloToy-AI-Passport-Penalty-v1.4.2-full.bin`。后续版本沿用
同一命名格式，只调整语义化版本号。

静态门禁已包含游戏模型和导航回归测试。桌面安装 CMake、C 编译器并准备锁定版本 LVGL 9.5.0 源码后，可渲染实际游戏界面：

```bash
./tools/preview-penalty.sh /path/to/lvgl-9.5.0
```

源码路径默认使用 ESP-IDF 构建后的 `managed_components/lvgl__lvgl`。截图输出至 `build/penalty-preview/*.ppm`（v0.5 使用 `build/penalty-art-preview/`）；测试检查文字宽度、六格目标层、绿区／深绿线像素、上下球路与门将接触点、前景遮挡、局部／整屏刷新一致性，以及 24 KB LVGL 池内连续 50 局／50 次进入退出。这是界面渲染测试，不是硬件模拟器或可交互的桌面移植版。
