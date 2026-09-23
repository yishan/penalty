<p align="right">
  <a href="README.md">English</a> · <strong>简体中文</strong>
</p>

# Penalty 点球决胜

Penalty 是为 [FoloToy AI Passport](https://github.com/folotoy/ai-passport) 制作的离线竖屏点球游戏。设备全程保持竖直，用三个实体按键体验五回合点球的两种角色：

- **射门挑战**：选择球门六格之一，把握时机条，击败黑衣门将。
- **守门挑战**：观察蓝衣射手的身体与脚部动作，选择黑衣门将的扑救方向并把握时机。

游戏包含简单、普通、困难三档难度，中英文切换、语言持久保存、短音效、像素美术和五回合总结。

## 下载

可安装固件发布在 [Releases 页面](https://github.com/yishan/penalty/releases)。请下载文件名以 `-full.bin` 结尾的文件；GitHub 自动生成的源码 ZIP/TAR 不是可刷写固件。

| 版本 | 状态 | 固件 SHA-256 |
| --- | --- | --- |
| [v1.4.2](https://github.com/yishan/penalty/releases/tag/v1.4.2) | 最新版；构建与宿主测试通过，真机验收待完成 | `c7f93051605164d6e26c479a63164bdb04e1c26d215bd9ea9e1637a494df4037` |
| [v1.1.0](https://github.com/yishan/penalty/releases/tag/v1.1.0) | 历史视觉更新版，已通过真机验证 | `98aeea498634309fe4d10248347c513dd7b212e3d9ce02d651a7400c5ba66607` |

每个 Release 同时附带 `SHA256SUMS.txt`。刷写前可在 macOS 校验：

```bash
shasum -a 256 FoloToy-AI-Passport-Penalty-v1.4.2-full.bin
```

合并镜像从偏移 `0x0` 刷入，内含 bootloader、分区表和应用。刷入该文件可能重置已保存的语言等设置。

## 玩法

在竖屏封面按 OK。UP／DOWN 选择射门、守门、设置或说明，OK 确认。

两种模式使用同一组六格顺序，每次默认从中上开始。按 DOWN 循环：

```text
中上 → 中下 → 左上 → 左下 → 右上 → 右下 → 中上
```

UP 按完全相反的顺序循环。这样同列上下移动始终与实体按键方向一致，访问完同列两格后才切换列。

### 射门挑战

1. 选择目标后按 OK。
2. 再按一次 OK 停止时机条并射门。

两格深绿线平均进球率为 90%，普通绿区为 65%，正常力度的绿区外为 25%；过弱和过强都会失败。简单、普通、困难的绿区宽度依次为 20、11、6。

### 守门挑战

1. 观察蓝衣射手的真实动作提示；简单、普通、困难分别显示 900／600／350 ms。
2. 选择黑衣门将的扑救格并按 OK。
3. 再按一次 OK 停止时机条并扑救。

完全读中格子时，深绿线／普通绿区／绿区外的扑救率为 90%／65%／25%；同列另一行分别为 35%／20%／5%；选择其他列则无法扑出。

五次尝试后显示进球或扑救总结。长按 OK 可取消本局并返回封面。

## 源码包

本仓库是应用层导出，不是独立 ESP-IDF 工程。当前内容由 Penalty 权威源码提交 `d5f08e7a1d345f9d9cac5552167eaf4166c8f5d0` 生成，固定 AI Passport 基线为 `ccd3576e304f7e17d9a0d4c12a2d05db379b14c0`。

请应用到完全一致的基线：

```bash
git clone https://github.com/folotoy/ai-passport.git
cd ai-passport
git checkout ccd3576e304f7e17d9a0d4c12a2d05db379b14c0
/path/to/penalty/apply.sh --dry-run .
/path/to/penalty/apply.sh .
```

再使用 ESP-IDF 5.5.3 构建：

```bash
source /path/to/esp-idf-v5.5.3/export.sh
./tools/validate.sh --static
./tools/validate.sh --firmware
```

`MANIFEST.md` 记录基线、权威源码提交、Launcher 合同版本和全部新增／替换路径。当前包于 2026-09-23 通过完整门禁；GitHub 发布的 v1.4.2 固件仍需真机确认首次默认中文，以及精简后的英文守门标题。

## 许可

MIT，FoloToy 基线版权声明保留在 [LICENSE](LICENSE)。中文字体子集来自思源黑体并遵循随附的 SIL Open Font License；音效为 CC0；美术来源与生成方式记录在 [`assets/README.zh_CN.md`](assets/README.zh_CN.md)。
