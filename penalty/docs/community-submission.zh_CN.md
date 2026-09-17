<p align="right">
  <strong>简体中文</strong> · <a href="community-submission.md">English</a>
</p>

# 社区提交记录

2026-09-15 为 FoloToy AI Passport 官方社区准备。本地游戏版本：**v0.3**。分类：**games（游戏）**。已获提交授权，但完成资料准备不代表已公开。

## 已准备的封面更新——均未上传

2026-09-16 先生成了[无底部标识的 v0.4 社区封面](../../assets/images/penalty/community-cover-v0.4-no-label.png)，尺寸 1086 x 1448，严格竖版 3:4；之后的 [v1.1 候选封面](../../assets/images/penalty/community-cover-v1.1-horizontal.png)不覆盖该历史文件，继续采用横向草坪明暗带、黑色门将球衣及精确“点球决胜”字样。两者都属于玩法示意，不是实机截图，也均**尚未上传**；项目 402、修订 735 及下方最后确认的 `pending（待审核）` 状态均未改变。

## 公开资料

- 精确的中英文标题与简介、文件路径、SHA-256 校验值以及最终生图提示词保存在[提交清单](../publishing/submission.json)，不含凭据。
- 中文标题：**Penalty 五球点球挑战**；英文标题：**Penalty: Five-Shot Challenge**。
- [社区封面](../../assets/images/penalty/community-cover-v0.3.png)：1086 x 1448 PNG，严格竖版 3:4。用内置 imagegen 工具基于原封面生成，已移除旋转指引，并标有 `AI ILLUSTRATION`。这不是实机截图，也不会替换固件中的开机操作指引。
- 固件：`build/Penalty-community-v0.3-full.bin`，919,968 字节，已重新构建并完成 `0x0` 偏移完整镜像校验。SHA-256：`26ca8313be811a0167fbec29b3881e94dfce50b27f03a749d63d017f9a7eaf60`。完整门禁通过，日志为 `build/penalty-publish-validation.log`。此副本与重新生成的 `build/FoloToy-AI-Passport-full.bin` 一致；之前的 portrait-cover 验收副本保持不变。
- 不添加额外玩法图片。本次提交不要求连接设备、刷机或拍摄实机截图。
- 未填写源码地址：当前公开远程地址指向上游模板，本地 Penalty 改动尚未发布到该仓库。本次未新建或推送 Git 仓库。

## 验证边界

Build: PASS。Host tests: PASS。Device tests: NOT RUN。公开简介已注明实机验收尚未完成；实机表现和当前宿主机 ASan 运行仍待验证，详见[验收记录](validation.zh_CN.md)。

## 提交回执

已通过 `submit --auto` 成功提交，并用 `projects` 独立查询核实。服务器返回一个项目及一致的提交回执，固件校验值与清单一致。

- 项目 ID：**402**；标识：**penalty**。
- 修订 ID：**735**；本地游戏版本：**v0.3**；服务器状态版本：**1**。
- 提交时间：**2026-09-15 04:49:10 UTC**（北京时间 12:49:10）。
- 确认状态：项目和修订均为 **pending（待审核）**，`reviewedAt` 为空。**已提交审核，尚未公开。** 这是提交后核验时的状态，不代表持续监控。
- [脱敏回执](../publishing/receipt.json)不含授权信息，凭据在仓库外以仅所有者可访问的权限保存。
