# Celestia for HarmonyOS（鸿蒙电脑版）

将开源天文模拟软件 [Celestia](https://github.com/CelestiaProject/Celestia)（Qt 版）移植到鸿蒙 HarmonyOS（OpenHarmony）PC 端。核心渲染/天体引擎基于 C++/Qt 编译为 `libcelestia.so`，应用壳为 ArkTS（`app/entry`），通过 ArkUI 承载 Qt 原生窗口。

## 当前进度

- ✅ 引擎与 Qt 前端成功交叉编译为鸿蒙 arm64-v8a 动态库
- ✅ 数据资源（星表、纹理、翻译）随 HAP 打包，启动时解压到应用沙箱
- ✅ 中文本地化（gettext `.mo` 翻译）已打通：菜单、浏览器面板、分类等显示中文
- ✅ 图标已替换为 Celestia 官方图标
- ✅ 已按华为要求配置单层图标，准备上架（当前为单机版，无联网依赖）

## 关键移植点

### 1. musl gettext 符号冲突（中文本地化根因）
鸿蒙底层为 musl，其动态链接器 `ld-musl-*.so.1` 自带 `gettext()` stub（原样返回 msgid），且加载优先级最高，会覆盖自研 libintl 实现。
**修复**：将整套 gettext API 重命名为 `cel_*` 前缀，避开 musl 符号抢占：
- `celestia-src/src/celutil/gettext.h`：`gettext→cel_gettext` 等宏映射
- `deps/libintl/src/libintl.c`：实现导出 `cel_gettext` 系列
- `deps/libintl/libintl.h`：`cel_*` 声明

### 2. po2mo.py 翻译键引号 bug
`deps/libintl/po2mo.py` 原实现首行 msgid 保留引号，导致 `.mo` 键为 `"&File"` 无法命中。已修复为首行同样去引号，生成 931 条正确翻译条目。

### 3. 沙箱数据目录
鸿蒙应用沙箱路径运行时才知道，`qtmain.cpp` 通过 `--dir` 传入 data 目录，并以**绝对路径**绑定 gettext locale 与数据资源。

## 目录结构

```
├── app/                    # 鸿蒙工程（ArkTS 壳 + entry 模块）
│   └── entry/src/main/
│       ├── ets/            # ArkTS UI
│       ├── resources/rawfile/celestia/  # 数据资源（星表、locale 等）
│       └── libs/arm64-v8a/ # 预编译 .so（libcelestia.so / libcelestiaqt.so）
├── celestia-src/           # Celestia 上游源码（含移植改动）
├── deps/
│   ├── libintl/            # 自研 libintl 实现（libintl.c / po2mo.py / libintl.h）
│   └── locale/             # 编译好的 .mo 翻译文件
├── configure.sh            # 交叉编译配置（含本机工具链绝对路径，clone 后需按需修改）
└── sign/                   # 签名产物（证书与 .hap 不入库）
```

## 构建

依赖：DevEco Studio（OpenHarmony NDK）、Qt 6.12.0 for HarmonyOS、CMake、Ninja。

1. 按 `configure.sh` 中的路径配置交叉编译工具链
2. 交叉编译 `celestia-src` 生成 `libcelestia.so`、`libcelestiaqt.so`
3. 用 DevEco Studio 打开 `app/`，hvigor 打包 → 签名 → `hdc install`

> 说明：`configure.sh` 含本机绝对路径（`/Users/jiexuanyang/...`），clone 后需修改为你的环境。

## 许可证

本项目基于 [Celestia](https://github.com/CelestiaProject/Celestia)（GPL-2.0），遵循原许可证。

## CLI 命令行工具（Mac 开发机，无头渲染）

`celestia-cli` 是一个在 Mac 开发机上运行的 Celestia 无头命令行工具：不弹窗口，直接离屏渲染图片、执行 celx 脚本、执行单条命令或进入交互式控制台。它复用同一份 `celestia-src` 源码，仅编译核心库 + 命令行入口。

### 构建

```bash
# 依赖：Qt 6.12.0 (macos)、brew 安装 lua libpng libjpeg libepoxy freetype fmt eigen
cmake -S celestia-src -B build-mac \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_NLS=OFF -DENABLE_QT6=OFF -DENABLE_FFMPEG=OFF -DENABLE_LIBAVIF=OFF \
  -DCMAKE_CXX_FLAGS="-Wno-register" \
  -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/eigen;/opt/homebrew/opt/fmt;/opt/homebrew/opt/lua;/opt/homebrew/opt/libpng;/opt/homebrew/opt/jpeg;/opt/homebrew/opt/libepoxy;/opt/homebrew/opt/freetype;/Users/jiexuanyang/Qt/6.12.0/macos"
cmake --build build-mac --target celestia-cli -j 10
```

### 使用

```bash
DATA=app/entry/src/main/resources/rawfile/celestia
CLI=build-mac/src/celestia/cli/celestia-cli
export DYLD_LIBRARY_PATH=build-mac/src/celestia:build-mac/src/celengine:/opt/homebrew/lib
export DYLD_FRAMEWORK_PATH=/Users/jiexuanyang/Qt/6.12.0/macos/lib

# 渲染一帧到 PNG（无窗口）
$CLI --dir $DATA --render-to-image /tmp/earth.png --select Earth --size 1920x1080

# 执行单条命令
$CLI --dir $DATA --exec "goto Mars"
$CLI --dir $DATA --exec "time scale 10000x"

# 执行 celx 脚本
$CLI --dir $DATA --script $DATA/demo.cel

# 交互式控制台（stdin 输入 goto/select/time/render/label/...，render 输出截图）
printf 'goto Saturn\nrender\nquit\n' | $CLI --dir $DATA
```

### 支持的命令

`goto/select/center/follow/syncorbit/home` · `time scale <倍率>` / `time <儒略日>` ·
`render <标记>` / `label <标记>`（stars/orbits/clouds/atmospheres/constellations...）·
`setsurface <表面>` · `location <纬度> <经度> [高度]` · `print` · `script <路径>` · `help`
