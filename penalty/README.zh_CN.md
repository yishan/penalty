<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# Penalty 点球游戏

为 AI Passport 设计的三键竖屏点球小游戏，包含离线单机模式与实验性的双设备 BLE 对战。设备保持竖直：UP／DOWN 按列上下轮询六个目标，OK 仍为确认。

状态：v2.0.0 定位为**联机公开测试版**。开源应用层与下载入口为 [yishan/penalty](https://github.com/yishan/penalty)，版本化文件位于 [v2.0.0 GitHub Release](https://github.com/yishan/penalty/releases/tag/v2.0.0)。GitHub 发布与官方社区分开，本版本尚未提交官方社区。“点球决胜”支持与 AI 单机对战及附近双设备对战。“点球练习”和“守门练习”保留五次尝试规则。抛硬币决定先后手，双方轮流攻守；落后方无法追平时提前结束，五回合平分后进入成对突然死亡。已验证范围与待验收项见 [v2.0.0 版本说明](docs/releases/v2.0.0.zh_CN.md)及[验证记录](docs/validation.zh_CN.md)。

“点球决胜”二级菜单包含单机对战、创建联机、加入联机和返回。创建方选择统一难度，双方核对六位安全码，由创建方触发随机抛硬币；BLE 角色不决定先手。双方同时盲选，门将没有射手动作提示。两台保持通电的已认证比赛可在 30 秒内恢复；超过截止时间则无胜负中止。两端统一显示创建方红衣、加入方蓝衣、门将黑衣。双方各射门一次计为一个回合。精确规则和验收边界见 [BLE v2 设计与里程碑记录](docs/plans/2026-09-24-ble-duel-v2-design.zh_CN.md)。M6 耐久与异常覆盖尚未完成。

## 从这里开始

- [游戏策划大纲](docs/game-design.zh_CN.md)：范围、操作、规则、页面、技术结构、里程碑和验收标准。
- [BLE v2 对战设计](docs/plans/2026-09-24-ble-duel-v2-design.zh_CN.md)：大厅／安全、盲选、恢复、里程碑和已记录的真机验收。
- [守门模式设计](docs/plans/2026-09-23-goalkeeper-mode-design.zh_CN.md)：已确认的视角、提示时长、扑救概率、动作素材和验收边界。
- [点球决胜模式设计](docs/plans/2026-09-23-shootout-mode-design.zh_CN.md)：抛硬币、轮流攻守、提前结束、突然死亡、固定内存模型与验收边界。
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

开机显示带 `PENALTY`／“点球决胜”固定游戏名的竖屏像素封面，设备不用旋转，直接按 OK。UP／DOWN 选择“点球决胜”“点球练习”“守门练习”“设置”或“说明”。“设置”中包含难度、音效、语言和返回；没有有效已保存偏好时默认显示简体中文。选择“语言”可切换英文／简体中文，当前页面立即刷新，有效选择在退出游戏或设备重启后仍保留。选择“难度”循环简单／普通／困难。

在“点球决胜”二级菜单选择“单机对战”即可挑战 AI。下面的硬币／交接流程描述玩家与 AI 对局；创建／加入使用上文独立的盲选 PvP 流程。

进入“点球决胜”后按 OK 抛出金色像素硬币：足球面表示玩家先射门，手套面表示玩家先守门；结果在动画开始前已经锁定。落定后再按 OK 开赛，之后每次攻守切换都先显示交接页并等待确认，守门提示不会在页面外提前计时。双方常规阶段各最多五球；落后方已不可能追平时立即结束。五轮平分后进入成对突然死亡，必须完成本轮双方各一球后才判胜负。顶栏与固定大小标记显示比分，不保存无限历史；重试会重新抛硬币。

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
生成交付文件 `build/FoloToy-AI-Passport-Penalty-v2.0.0-full.bin`。固件内部版本号也读取 `penalty/VERSION`。后续版本沿用
同一命名格式，只调整语义化版本号。

静态门禁已包含游戏模型和导航回归测试。桌面安装 CMake、C 编译器并准备锁定版本 LVGL 9.5.0 源码后，可渲染实际游戏界面：

```bash
./tools/preview-penalty.sh /path/to/lvgl-9.5.0
```

预览脚本现同时运行单机与联机渲染器。固件／完整门禁在归档前也执行两者。联机截图另检查双语大厅／安全／难度、硬币、锁定等待、重连及赢／输／无胜负面板；宿主渲染不代表实体设备验收。

源码路径默认使用 ESP-IDF 构建后的 `managed_components/lvgl__lvgl`。截图输出至 `build/penalty-preview/*.ppm`（v0.5 使用 `build/penalty-art-preview/`）；测试检查文字宽度、六格目标层、绿区／深绿线像素、上下球路与门将接触点、前景遮挡、局部／整屏刷新一致性，以及 24 KB LVGL 池内连续 50 局／50 次进入退出。这是界面渲染测试，不是硬件模拟器或可交互的桌面移植版。
