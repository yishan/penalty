<p align="right">
  <strong>简体中文</strong> · <a href="build-and-test.md">English</a>
</p>

# 构建与验证（Build & Test）

使用 ESP-IDF 5.5.3。全新机器或缺少工具链时，先按
[环境引导](environment-setup.zh_CN.md)完成安装。

> 固件编译优先运行 `./tools/validate.sh --firmware`。空白设备初始化或有意完整
> 刷新时，把验证通过的 `build/FoloToy-AI-Passport-full.bin` 从 `0x0` 写入；
> 合并镜像可能重置 NVS，需要保留已有 NVS 状态时使用分段 `idf.py flash`。
> `idf.py build` 和
> `idf.py flash` 只作为增量开发命令，不作为默认交付方式。

```bash
source <ESP-IDF-v5.5.3-路径>/export.sh
idf.py --version             # 必须输出 ESP-IDF v5.5.3
./tools/validate.sh --firmware # 优先：编译并验证 0x0 合并固件
idf.py set-target esp32c3     # 配置目标芯片（fresh checkout 后/换 target 后运行）
idf.py build                  # 可选：增量 app 编译
idf.py flash monitor          # 可选：增量 app 烧录
idf.py fullclean              # 只清空过期生成状态（勿用于清理用户源码改动）
```

`idf.py fullclean` 不能让已有 `sdkconfig` 完整同步变更后的 defaults。需要重建
target 或已跟踪 defaults 时，先保留有意的本地设置，再运行
`idf.py set-target esp32c3`。

仓库提交 `dependencies.lock` 以固定 ESP-IDF Managed Components 的解析结果。修改 `idf_component.yml` 后必须使用 ESP-IDF 5.5.3 重新生成锁文件、review 版本变化并与 manifest 一起提交；普通构建不应产生未提交的锁文件差异。

固件门禁使用全新的临时构建目录，并从仓库 `sdkconfig.defaults` 生成隔离的 `sdkconfig`。它不会读取或覆盖开发者根目录的 `sdkconfig`。门禁保留验证通过的通用合并镜像 `build/FoloToy-AI-Passport-full.bin`，并根据 `penalty/VERSION` 为本项目生成内容完全一致的交付副本 `build/FoloToy-AI-Passport-Penalty-v<主版本.次版本.修订版本>-full.bin`。门禁同时验证[当前配置的固件布局](firmware-layout.zh_CN.md)：从 `flash_args` 读取镜像偏移，检查分区表 MD5、边界和不重叠，并确认应用从所配置的 app 分区起点开始且未超出分区。允许用户自定义分区布局。

门禁还会把对应的 ELF、MAP、合并／应用／bootloader／分区表镜像和 `flash_args` 保留在 `build/firmware/<full-bin-sha256>/`。应用描述符内嵌的 ELF SHA256 必须与保留的 ELF 一致。不可变归档把调试文件绑定到经过验证的固件，不能用后续重建的 ELF 替换。重复归档相同固件时沿用首次验证的 MAP，因为临时构建路径可能使 MAP 改变而固件不变。真机交接前验证归档：

```bash
python3 tools/archive_firmware.py verify build/firmware/<full-bin-sha256>
```

归档不会清理固件内容中的敏感信息。二进制与调试归档不提交。设备采用不同分区布局时，先确认兼容性，再仅写入获准的 app 范围；不默认用合并镜像替换设备布局。

当前基线含一个可独立运行的纯逻辑测试：

```bash
cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_ui_pixel_math.c main/ui_pixel_math.c \
  -o /tmp/test_ui_pixel_math
/tmp/test_ui_pixel_math
```

统一验证入口：

```bash
./tools/validate.sh --static    # 仓库一致性、workflow、文档链接、敏感信息、host tests
./tools/validate.sh --firmware  # ESP-IDF build、merge-bin、偏移与当前布局校验
./tools/validate.sh             # 完整验证
```

完整验证要求预先激活 ESP-IDF 5.5.3。CI 与本地使用同一脚本；若 CI 和本地行为不同，应先修复脚本或环境，而不是维护两份命令。

Penalty M4 受控断链开发版使用 `./tools/validate.sh --all --dev-ble-faults`。
门禁追加受版本管理的 `sdkconfig.dev-ble-faults` 配置，验证
`CONFIG_PENALTY_DEV_BLE_FAULTS=y`。普通构建默认关闭入口，并拒绝意外启用。
开发版交付副本保存在 `build/dev-ble-faults/`，使用
`-dev-ble-faults-full.bin` 后缀，不替换普通交付副本。两种构建的内容寻址调试归档
都保存在 `build/firmware/`。不得发布此开发版。刷机仍需单独授权；两台已有兼容
factory 布局的设备只更新 app。若 IDF Python 缺少仅用于宿主图片测试的依赖，
可将 `HOST_PYTHON` 指向已有且具备这些依赖的 Python；IDF 仍使用自身解释器。

涉及物理外设的改动必须在真机运行硬件指南验收清单，并把“编译通过”与“硬件验证通过”分开记录。

Penalty M5 的固件／完整门禁在归档前还运行实际 LVGL 9.5.0 双桌面渲染器及 `idf.py size`。单机渲染器覆盖三个模式；联机渲染器覆盖双语大厅／安全、难度、硬币、锁定等待、恢复和总结。两者均在 24 KiB 池内检查 50 次创建／销毁。这些生成截图是桌面证据，不是真机截图或运行时资源测量。仅静态验证检查固定状态预算和生命周期契约，不运行 CMake 渲染器；单独渲染检查可运行 `tools/preview-penalty.sh`。

交付时使用带项目名和版本号的副本；它与验证通过的通用镜像
`build/FoloToy-AI-Passport-full.bin` 完全一致。不得上传应用单镜像
`build/FoloToy-AI-Passport.bin`，后者不包含完整且经校验的固件布局。
