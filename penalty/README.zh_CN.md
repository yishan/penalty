<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# Penalty 点球游戏

为 AI Passport 设计的三键、离线横屏点球小游戏。画面向左旋转 90°，设备向右旋转 90° 持握，此时 UP 对应右、DOWN 对应左，OK 仍为确认。

状态：v1.1 视觉更新已实现并完成构建，2026-09-17。竖屏封面在 `PENALTY` 下新增固定中文游戏名“点球决胜”，当前草坪素材统一改为横向明暗条纹，门将改穿黑色球衣；横屏标题页不再显示 `MAKE IT COUNT.`／“一球定胜负”。v1.0 的六格射门规则、90%／65%／25% 校准进球率、中英文界面与语言持久保存均保持不变。确切证据见[安装说明、验证记录与真机清单](docs/validation.zh_CN.md)，v1.1 版本已通过真机验证。

## 从这里开始

- [游戏策划大纲](docs/game-design.zh_CN.md)：范围、操作、规则、页面、技术结构、里程碑和验收标准。
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

开机显示带 `PENALTY`／“点球决胜”固定游戏名的竖屏像素封面，向右横握设备后按 OK。UP／DOWN 选择“开始”“设置”或“说明”。“设置”中包含难度、音效、语言和返回；选择“语言”可切换英文／简体中文，当前页面立即刷新，退出游戏或设备重启后仍保留。选择“难度”循环简单／普通／困难，返回后开始游戏。绿区宽度保持 20／11／6 格，每球随机换位置。门将等概率预判六格后，深绿线平均 90% 进球，其余绿区 65%，正常力度绿区外 25%。

UP 沿左上→中上→右上→右下→中下→左下顺时针切换，DOWN 反向切换。蓄力前即可观察区域；按 OK 锁定目标并开始蓄力，再按 OK 射门，游标进入绿区后减为原速的五分之一。五次尝试后结算，长按 OK 取消本局并返回竖屏封面。关闭音效时会先播一次关闭提示，重新开启时播放开启提示；“说明”显示玩法。重玩和返回标题保留难度及静音；返回封面则重置。只有语言持久保存，成绩、难度与静音均不保存；精确参数见[射门规则](docs/game-design.zh_CN.md)。

在仓库根目录执行：

```bash
./tools/validate.sh --static
./tools/validate.sh --firmware
./tools/validate.sh
```

静态门禁已包含游戏模型和导航回归测试。桌面安装 CMake、C 编译器并准备锁定版本 LVGL 9.5.0 源码后，可渲染实际游戏界面：

```bash
./tools/preview-penalty.sh /path/to/lvgl-9.5.0
```

源码路径默认使用 ESP-IDF 构建后的 `managed_components/lvgl__lvgl`。截图输出至 `build/penalty-preview/*.ppm`（v0.5 使用 `build/penalty-art-preview/`）；测试检查文字宽度、六格目标层、绿区／深绿线像素、上下球路与门将接触点、前景遮挡、局部／整屏刷新一致性，以及 24 KB LVGL 池内连续 50 局／50 次进入退出。这是界面渲染测试，不是硬件模拟器或可交互的桌面移植版。
