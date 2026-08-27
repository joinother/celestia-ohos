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
