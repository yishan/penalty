<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 点球决胜 v2.0.0 — 联机公开测试版

为 [FoloToy AI Passport](https://github.com/folotoy/ai-passport) 设计的三键竖屏点球游戏。挑战 AI、练习射门或守门，或与附近另一台设备上的朋友对战。游戏无需互联网连接。

## 下载

下载 [v2.0.0 Pre-release](https://github.com/yishan/penalty/releases/tag/v2.0.0)。已完成部分双机检查，耐久与完整异常矩阵尚未完成。[v1.4.2](https://github.com/yishan/penalty/releases/tag/v1.4.2) 仍可下载。

| 附件 | 用途 |
| --- | --- |
| `FoloToy-AI-Passport-Penalty-v2.0.0-full.bin` | 从 `0x0` 安装独立玩法，1,638,400 字节 |
| `FoloToy-AI-Passport-Penalty-v2.0.0-app.bin` | 更新已核验兼容的应用分区，1,572,864 字节 |
| `SHA256SUMS.txt` | 下载完整性校验 |
| 三张 PNG | 实际界面桌面截图，非实机照片 |

**完整镜像会替换启动程序／分区布局，可能重置设置或使原应用／资源不可访问。不要用它更新 Launcher 槽位。app-only 文件禁止写入 `0x0`。** 请核对实际设备布局，备份需要保留的数据。本版 Launcher 槽位／回滚／返回行为尚未实机验证。

下载对应文件与 `SHA256SUMS.txt` 后运行 `shasum -a 256 -c SHA256SUMS.txt`。自动源码压缩包不是可刷写固件。

## 如何游玩

设备保持竖直，在封面按确认。上下键选择菜单，两端循环；确认键进入，长按确认返回封面。

- **点球决胜**：挑战 AI，或选择“创建联机／加入联机”。双方各射一球计一回合。常规五回合，无法追平时提前结束，平分后成对突然死亡。
- **点球练习**：选方向，按确认开始时机条，再按确认射门。每局五次。
- **守门练习**：观察 AI 射手起手动作，选择扑救方向并控制时机。每局五次。
- **设置**：难度、声音及中英文；仅语言跨重启保留。

下键按 `中上 → 中下 → 左上 → 左下 → 右上 → 右下` 循环，上键反向。首次确认锁定方向并开始时机条，再次确认完成射门或扑救。

### 附近双人联机

1. 两台进入“点球决胜”。一台“创建联机”，另一台“加入联机”，找到相同四位大厅码。
2. 核对两端六位安全码，只在一致时确认，再确认创建方选择的难度。
3. 创建方按确认抛硬币。创建或加入身份不决定先手。
4. 双方独立选择，没有对手方向提示。两端统一显示创建方红衣、加入方蓝衣、门将黑衣。
5. 断线后保持两台通电。30 秒内恢复保留锁定动作和比分，不重新抛硬币、不重复结算；超时无胜负中止，设备重启不恢复原比赛。

![选中创建联机](assets/images/penalty/releases/v2.0.0/shootout-create-zh.png)
![选中加入联机](assets/images/penalty/releases/v2.0.0/shootout-join-zh.png)

## 更新与验证

v2 新增附近双人对战，统一球衣与配对回合计数，加大行距、强化选中标记，调整封面文字和人物比例，并无损精简体积。详见[版本说明](penalty/docs/releases/v2.0.0.zh_CN.md)。

- Build：PASS，ESP-IDF 5.5.3 完整门禁及固件归档校验通过。
- Host tests：PASS，规则／协议／恢复、双语实际 LVGL 渲染及五十次界面生命周期检查通过。
- Device tests：仅本次精确普通版 v2.0.0 app 的安装、独立 Flash 校验和有界启动 PASS。此前开发版完成部分双机对局／恢复检查。
- Unverified：普通发布版实体游戏、耐久、全部断线／安全／兼容矩阵、多大厅、资源／时延／功耗测量及 Launcher 验收。详见[验证证据](penalty/docs/validation.zh_CN.md)。

GitHub 分发与 FoloToy 官方社区相互独立，v2.0.0 尚未提交官方社区。

## 构建开源应用层

本仓库是生成的应用层导出，**不是独立 ESP-IDF 工程**。[MANIFEST.zh_CN.md](MANIFEST.zh_CN.md) 记录源码提交、精确基线、导出路径及许可。开发仍在权威 Penalty 工作树；公开仓库改动须先回收到权威源码再导出。

```bash
git clone https://github.com/folotoy/ai-passport.git
cd ai-passport
git checkout ccd3576e304f7e17d9a0d4c12a2d05db379b14c0
/path/to/penalty/apply.sh --dry-run .
/path/to/penalty/apply.sh .
source /path/to/esp-idf-v5.5.3/export.sh
HOST_PYTHON=/path/to/python-with-pillow ./tools/validate.sh
```

请先审阅 dry run 和目标中的改动；脚本会覆盖列出的路径。脚本核对基线，只复制 `export-files.txt`。普通构建禁用开发断线注入，不得分发 `--dev-ble-faults` 版本。重建时间戳／路径／ELF 哈希可能不同，Release 校验值标识精确分发文件。

## 许可

代码采用 MIT，保留 [LICENSE](LICENSE) 中 FoloToy 与 Yishan 版权声明。思源黑体子集保留随附 SIL Open Font License，合成音效为 CC0。原创 AI 生成美术来源见 [assets/README.zh_CN.md](assets/README.zh_CN.md)。
