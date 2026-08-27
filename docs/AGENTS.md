# Celestia HarmonyOS 移植 — Agent 协作文档

> **最后更新：2026-08-27**
> **仓库：** https://github.com/joinother/celestia-ohos （fork of CelestiaProject/Celestia，GPL-2.0）
> **当前状态：** ArkTS 壳 + Qt-for-OHOS 渲染可运行；CLI 命令总线（方向 A/B/C）已完成；无头渲染验证通过

---

## 1. 项目概述

将 [Celestia](https://github.com/CelestiaProject/Celestia) 开源 3D 天文模拟软件移植到 HarmonyOS 电脑版，单 HAP 应用运行在 DevEco 模拟器 / 鸿蒙电脑上。

### 架构

```
ArkUI (ArkTS 壳)              ← 用户交互层（EntryAbility / 命令通道）
    ↓ 命令文件轮询通道
Qt-for-OHOS (libcelestiaqt.so) ← Qt 窗口承载 + CelestiaCore
    ↓
Celestia Core (C++)           ← 渲染 / 天体计算 / 仿真
    ↓
OpenGL ES → XComponent → Framebuffer
```

### 关键技术

- **渲染壳**：Celestia Qt 版移植到 HarmonyOS，Qt 主窗口渲染到 XComponent（Qt-for-OHOS 工具链，与 Stellarium 鸿蒙版同方案）
- **命令桥**：新增 `src/celestia/qt/qtcli.cpp` 命令分发器（goto/select/time/render/...），被 ①Qt CLI dock、②celestia-cli、③ArkTS 命令文件通道 三者共用——**同一命令总线，不产生三套行为**（借鉴 Stellarium 命令桥模式）
- **CLI（Mac 开发机）**：`celestia-cli` 无头离屏渲染（CGL + FBO），支持 `--render-to-image / --exec / --script / --batch / --list / 交互`

---

## 2. 文档即状态机

**核心原则：任何 Agent 做操作前先读文档，做完后立即写文档。**

| 操作 | 前置动作 | 后置动作 |
|------|----------|----------|
| 修改 C++/ArkTS 代码 | 读本文档 + `CHANGELOG.md` | 在 `CHANGELOG.md` 追加记录 |
| 构建/测试 | 读构建命令（第 4 节） | 记录结果到 `CHANGELOG.md` |
| 发现 Bug | 读 `CHANGELOG.md` 避免重复 | 在 `KNOWN-ISSUES.md` 追加 |
| 修复 Bug | 读 `KNOWN-ISSUES.md` | 标记已修复，更新 `CHANGELOG.md` |

### CHANGELOG.md 条目格式

```markdown
## [日期] - 简述
- **修改文件：** `相对路径`
- **修改内容：** 描述
- **修改原因：** 需求 / Bug 关联
- **构建结果：** BUILD SUCCESSFUL / FAILED
- **验证结果：** 通过 / 未验证 / 部分通过（附现象）
```

---

## 3. 源码路径

| 路径 | 用途 |
|------|------|
| `celestia-src/` | Celestia 源码（git 副本，来自上游 + 鸿蒙改动） |
| `celestia-src/src/celestia/qt/qtcli.cpp/.h` | **命令分发器**（鸿蒙 CLI 命令语言） |
| `celestia-src/src/celestia/cli/celestia-cli.cpp` | **Mac 无头 CLI**（离屏渲染 + 命令目录 + batch） |
| `app/entry/src/main/ets/` | ArkTS 壳（UI / 命令文件轮询） |
| `app/entry/src/main/resources/rawfile/celestia/` | Celestia 数据（celestia.cfg、star catalogs、textures） |
| `app/entry/src/main/cpp/` | N-API 原生桥（如有） |
| `build-mac/` | Mac CLI 构建目录（gitignore） |
| `build/` | 鸿蒙交叉编译目录（gitignore） |

---

## 4. 构建命令

### 4.1 鸿蒙 HAP（DevEco + hvigor）

```bash
cd /Users/jiexuanyang/celestia-ohos
# 交叉编译 celestia-src → libcelestiaqt.so（Qt-for-OHOS），同步到 entry/libs/arm64-v8a/
# 再用 DevEco Studio / hvigorw 打包签名
/Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw assembleHap --no-daemon
```

> 说明：`configure.sh` 含本机绝对路径，clone 后需修改。签名证书不入库（已在 git 中清除）。

### 4.2 Mac 开发机 CLI（无头渲染，无需鸿蒙设备）

```bash
cmake -S celestia-src -B build-mac \
  -DCMAKE_BUILD_TYPE=Release -DENABLE_NLS=OFF -DENABLE_QT6=OFF \
  -DENABLE_FFMPEG=OFF -DENABLE_LIBAVIF=OFF -DCMAKE_CXX_FLAGS="-Wno-register" \
  -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/eigen;/opt/homebrew/opt/fmt;/opt/homebrew/opt/lua;/opt/homebrew/opt/libpng;/opt/homebrew/opt/jpeg;/opt/homebrew/opt/libepoxy;/opt/homebrew/opt/freetype;/Users/jiexuanyang/Qt/6.12.0/macos"
cmake --build build-mac --target celestia-cli -j 10
```

运行时需 `DYLD_LIBRARY_PATH` 指向 build-mac 下的 celestia/celengine 与 `/opt/homebrew/lib`，`DYLD_FRAMEWORK_PATH` 指向 Qt macos lib。

---

## 5. 命令集（统一命令总线）

命令语言定义在 `qtcli.cpp`，CLI 目录定义在 `celestia-cli.cpp` 的 `kCommandCatalog`。两者必须保持一致。

| 命令 | 用途 |
|------|------|
| `goto/go <天体>` / `select` / `center` / `follow` / `syncorbit` | 天体导航 |
| `home` | 返回太阳系 |
| `time scale <倍率>` / `time <儒略日>` | 时间控制 |
| `setsurface <名称>` | 切换表面 |
| `render <标记>` / `label <标记>` | 渲染层 / 标签开关 |
| `location <纬度> <经度> [高度]` | 地表位置 |
| `print` / `script <路径>` | 消息 / celx 脚本 |
| `renderimg [WxH]` / `render` | CLI 离屏截图（交互模式） |
| `help` / `list` / `version` / `clear` | 帮助 / 目录 / 版本 |

CLI 机器可读目录：`celestia-cli --list --json`。

---

## 6. 上架前清单（单机版）

> 当前个人开发者未备案，上架版必须是**单机版**（无联网功能），与 Stellarium 鸿蒙版策略一致。

- [ ] 包名规范化
- [ ] 隐私政策页面
- [ ] 华为发布证书 + Release 签名
- [ ] 截图（横屏）与应用描述（中英文）
- [ ] GPL-2.0 许可证声明 + 源码链接
- [ ] 内容分级
- [ ] 应用体积评估（Celestia 数据 + 纹理）
- [ ] 权限清单核对（华为单层图标标准、文件选择器等）
- [ ] **联网功能清零**：确认无网络 API 调用 / 无外发数据 / 无未登记端口

---

## 7. 已知问题（详见 KNOWN-ISSUES.md）

1. 无头渲染 `--select` 后天体已选中但相机未完全居中（HUD 信息正确）——可加 `gotoSelection` 多帧推进
2. 鸿蒙渲染管线未做 PBO 异步回读 / 降采样优化（Stellarium 已验证 8→61 FPS 的方案可借鉴）
3. Qt CLI dock 中文输入法在模拟器下会组合拼音（系统行为）

---

## 8. 注意事项

1. **不要推送到上游 `CelestiaProject/Celestia`**，推送到 `joinother/celestia-ohos`
2. `entry/libs/` 下 `.so` 不入库，丢失后需用 Qt-for-OHOS 重新生成
3. hvigor 只构建 ArkTS + 打包，C++ 改动需 Qt 交叉编译
4. ArkTS 限制（继承 Stellarium 经验）：`@Builder` 参数不是响应式的；`@Builder` 内不能写 `let`；`Blank()` 只能放 `Row/Column/Flex`；`HitTestMode.None` 会让子树按钮全部失效
5. 坐标：`onAreaChange` 返回 vp；`uitest` 的 bounds/click 是设备像素（= vp × 密度）
6. **Mac 无头渲染关键坑**：`core.resize(w,h)` 必须调用（否则 metrics 1x1 黑屏）；cfg 相对路径需 chdir 到数据目录，输出路径基于原 cwd 解析
