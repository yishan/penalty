<p align="right">
  <a href="README.md">English</a> · <strong>简体中文</strong>
</p>

# Penalty

一个用三个按键操作的离线点球游戏,运行在 [FoloToy AI Passport](https://github.com/folotoy/ai-passport) 可穿戴设备上。每局五球、六个球门格,以及一个决定射门质量的力度表。

把设备顺时针转过来,按 OK,竖屏封面就会进入游戏。

## 玩法

开机后是竖屏封面。将设备顺时针转动,让原来的顶部朝右,然后按 OK。

标题页有 Play、Settings、Help 三项,UP/DOWN 移动,OK 选择。

每一次射门需要按两次:

1. **瞄准。** UP 沿顺时针前进,DOWN 反向。顺序是 左上 → 中上 → 右上 → 右下 → 中下 → 左下,两端循环。按 OK 锁定目标并启动力度表。
2. **射门。** 力度表在 0 → 100 → 0 之间往返,再按一次 OK 停下并出脚。

力度表常规档每格 8 ms,进入绿区后降为每格 40 ms,让有效窗口清晰可辨,而不是一闪而过。

任意界面长按 OK 都可以退出游戏,回到封面。

## 规则

难度只改变绿区宽度,力度表节奏和各档概率都不变。

| 难度 | 绿区宽度 | 完整周期 | 每次穿过的完美窗口 |
| --- | --- | --- | --- |
| EASY(默认) | 20 格 | 2,880 ms | 80 ms |
| NORMAL | 11 格 | 2,304 ms | 80 ms |
| HARD | 6 格 | 1,984 ms | 80 ms |

第二次按键落在哪里决定一切:

| 力度 | 结果 |
| --- | --- |
| 0–29 | SAVE —— 力度不足,被门将没收 |
| 91–100 | MISS —— 打飞出横梁 |
| 绿区内两格暗线 | 精准射门,平均进球率 90% |
| 绿区其余部分 | 平均进球率 65% |
| 30–90 绿区之外 | 平均进球率 25% |

球门是上下两排共六格,守门员独立预测其中一格。守门员格子与玩家格子的关系经过标定,使得在均匀预测下,**无论你选哪一格**都能得到上面的平均值。这是长期概率,不是每局的配额——五球连起来可能走向任何结果。

绿区、守门员的隐藏选择和结果掷骰都在每次射门开始时一次性生成,之后永不重掷:即使输入丢失,也会回到同一球,而不是改变你的命运。动画呈现的,是你在按下那一刻就已经定下的结果。

五球结束后进入总结页,显示进球数、每球结果和精准命中次数,可选择重玩或回标题。

## 设置

| | |
| --- | --- |
| Difficulty | 在 EASY → NORMAL → HARD 之间循环 |
| Sound | 静音 / 取消静音,两端各有提示音 |
| Language | 立即切换 English / 简体中文 |
| Back | 返回标题页 |

语言是唯一跨重启保留的设置。比分、难度和声音按设计只存在于本局,重玩会保留当前难度。

游戏内的 Help 页会说明操作方式。

## 声音

踢球、手套、进球三类短提示音,均远短于一秒,16 kHz 单声道。音频跑在一个有界 worker 上:如果处理不过来,过期提示音会被丢弃而不是排队堆积;如果音频根本没起来,游戏依然可以完全静音地玩下去。

## 构建

这是一个应用层,不是独立工程——`main` 依赖 `bsp` 组件,构建还需要基线的 CMake 工程、分区表和依赖锁定文件。先把这些文件铺到基线副本上:

```bash
git clone https://github.com/folotoy/ai-passport.git
cd ai-passport
/path/to/this/repository/apply.sh .
```

然后构建。需要 ESP-IDF 5.5.3 与 ESP32-C3(8 MB Flash):

```bash
source /path/to/esp-idf-5.5.3/export.sh
./tools/validate.sh --static     # 仓库检查与宿主机测试
./tools/validate.sh --firmware   # ESP-IDF 构建与合并镜像校验
```

固件阶段产出 `build/FoloToy-AI-Passport-full.bin`,烧录到偏移 `0x0`。也可以用浏览器烧录器 <https://ai-passport.folotoy.cn/tools/web-flasher/>(固件在本地写入,不会上传)。

另有一个桌面 harness,用真实 LVGL 9.5.0 渲染界面,便于在没有硬件时检查布局:

```bash
./tools/preview-penalty.sh /path/to/lvgl-9.5.0
```

`MANIFEST.md` 列出了本应用层具体替换了基线的哪些文件。

游戏可以构建、宿主机测试通过,并且 v1.1 版本已在真机上刷写并实际运行过。

## 目录结构

```text
main/penalty_*.{c,h}         规则、场景、界面、本地化、音频、应用适配层
main/main.c                  竖屏封面 -> 游戏 -> 封面
assets/{images,fonts,music}/penalty/
                             美术、字体子集、音效
tests/                       宿主机测试与桌面 LVGL 渲染 harness
tools/                       资产生成、预览、验证门禁
penalty/docs/                设计、本地化与验证记录
```

`main/penalty_model.c` 以纯 C 承载规则,不依赖 ESP-IDF 或 LVGL,因此概率可以在宿主机上测试。

## 许可

MIT,基线为 FoloToy 的版权声明,按要求保留在 [LICENSE](LICENSE) 中。中文字体子集由思源黑体 Source Han Sans SC 生成(SIL Open Font License),许可原文随附;音效为 CC0;美术由 AI 绘图工具生成。详见 [`assets/README.md`](assets/README.md)。
