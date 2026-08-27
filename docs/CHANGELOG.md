# Celestia 鸿蒙移植 — 修改日志（CHANGELOG）

> 每次变更在此追加，格式见 `AGENTS.md` 第 2 节。

## 2026-08-27 - CLI 命令目录 --list/--batch（对齐 Stellarium 命令总线）
- **修改文件：** `celestia-src/src/celestia/cli/celestia-cli.cpp`、`README.md`
- **修改内容：** 新增 `kCommandCatalog` 命令目录表；`--list / --list --json` 打印目录（机器可读）；`--batch <file.json>` 批量执行 JSON 命令文件（单项失败不阻断）；交互模式新增 `list` 命令与 `renderimg [WxH]`
- **修改原因：** 借鉴 Stellarium `docs/harmonyos/CLI.md` 的命令总线模式，让 CLI 命令可被 AI/脚本发现与批量调用
- **构建结果：** BUILD SUCCESSFUL
- **验证结果：** 通过（`--list` 输出目录；`--batch` 3 项 0 失败；交互 `list` 正常）
- **备注：** 目录与 qtcli 分发器解耦，qtcli 仍是唯一执行路径

## 2026-08-27 - Mac 无头命令行工具 celestia-cli（方向 B/C）
- **修改文件：** `celestia-src/src/celestia/cli/celestia-cli.cpp`（新增）、`celestia-src/src/celestia/cli/CMakeLists.txt`（新增）、`celestia-src/src/CMakeLists.txt`
- **修改内容：** 离屏渲染（CGL 3.2 core + epoxy + FBO + glReadPixels）无窗口运行；支持 `--render-to-image / --exec / --script / 交互控制台`；`--select` 选择天体
- **修改原因：** 用户要求"cli 不能把无头模式做了吗"——开发机无需鸿蒙设备即可调试渲染/命令
- **构建结果：** BUILD SUCCESSFUL
- **验证结果：** 通过（无头渲染地球/木星/金星 PNG；goto/time 命令；demo.cel 脚本；交互模式）
- **备注：** 关键坑——必须调 `core.resize(w,h)` 否则 metrics 1x1 黑屏；cfg 相对路径需 chdir 到数据目录；输出路径基于原 cwd 解析

## 2026-08-27 - 应用内命令行控制台（方向 A）
- **修改文件：** `celestia-src/src/celestia/qt/qtcli.cpp/.h`（新增）、Qt 主窗口 CLI dock、ArkTS 命令文件轮询通道
- **修改内容：** 新增命令分发器（goto/select/center/follow/syncorbit/home/time/setsurface/render/label/location/print/script/help）；Qt 主窗口底部 CLI dock；ArkTS→Qt 命令文件通道备用
- **修改原因：** 用户要求"增加 cli 功能"，先做应用内控制台
- **构建结果：** BUILD SUCCESSFUL
- **验证结果：** 通过（注入 `goto Jupiter` 端到端生效；dock UI；IME 输入）
- **备注：** 命令语言成为统一命令总线核心，被 CLI/ArkTS 共用

## 2026-08-27 - 从仓库移除签名证书与 profile 配置
- **修改内容：** 清除 git 历史中的内嵌开发证书与 profile 配置（公开仓库不应包含）
- **修改原因：** 安全：公开仓库不得含签名私钥
- **备注：** 签名产物改存本地 sign/（.gitignore 排除）

## 2026-08-26 - 移植 Celestia Qt 版到鸿蒙 HarmonyOS 电脑版
- **修改内容：** 初始移植：Qt-for-OHOS 交叉编译 `libcelestiaqt.so`、ArkTS 壳、数据资源 rawfile/celestia、configure.sh 工具链配置
- **构建结果：** BUILD SUCCESSFUL
- **验证结果：** 可构建、可安装、可运行

## 2026-08-27 - 修复无头渲染 --select 未居中（goto + setOrientation 组合）
- **修改文件：** `celestia-src/src/celestia/cli/celestia-cli.cpp`
- **修改内容：** renderFrame 的 select 逻辑改为：`gotoSelection(0.0)` 飞到目标附近 + `obs->update(0,1)` 完成移动 + `math::LookAt` 计算朝向 + `setOrientation(q)` 直接对准（替代 centerSelection）
- **修改原因：** journey 的朝向在单次 update 中不生效，导致目标在视野边缘或不可见
- **构建结果：** BUILD SUCCESSFUL
- **验证结果：** 通过（渲染出居中的地球 55533km 与木星 4.45AU，均为满相位大球体）

## 2026-08-27 - 资源完整性审计与修复（asteroid.jpg / logo.png）
- **修改文件：** `tools/check-assets.py`（新增）、`docs/RESOURCE-AUDIT.md`（新增）、rawfile 数据（asteroid.jpg、logo.png）
- **修改内容：** 新增资源审计脚本（cfg 引用 + ssc Mesh/Texture 深度核对 + 体积统计）；修复缺失的 comet 纹理 `asteroid.jpg`（mathilde.jpg 替代）与启动 Logo `logo.png`（splash.png 替代）
- **修改原因：** 上架前资源完整性硬检查；旧 comets.ssc 引用已移除纹理
- **构建结果：** 无需重编译（纯数据）
- **验证结果：** 通过（cfg 0 缺失、ssc 9038 引用 0 缺失、渲染 Halley 无数据错误；总体积 247.6 MB）
