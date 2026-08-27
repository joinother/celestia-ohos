# Celestia 鸿蒙移植 — 已知问题（KNOWN-ISSUES）

> 修复时在对应条目打勾并注明修复提交；新增问题按此格式追加。

## 进行中 / 未验证

### 1. 无头渲染 `--select` 后天体未完全居中
- **现象：** `celestia-cli --render-to-image --select Earth` 渲染出的 HUD 目标信息正确，但相机未对准天体中心（视角停留在远处星空）
- **根因：** `centerSelection(0.0)` 立即执行但相机需要时间/多帧推进才稳定
- **方向：** 渲染前用 `gotoSelection` 多帧推进，或手动计算 observer→target 方向对准
- **状态：** ⚠️ 进行中

### 2. 鸿蒙渲染管线未做性能优化
- **现象：** 鸿蒙端渲染未验证 PBO 异步回读 / 降采样 / VSync 禁用
- **参考：** Stellarium 鸿蒙版已验证：`glBlitFramebuffer` 降采样 50% + 3 个 PBO 轮换异步回读 + `eglSwapInterval(0)`，回读 34ms→1ms，8→61 FPS
- **状态：** ⚠️ 待做（需鸿蒙设备验证）

## 已修复

### 3. 无头渲染黑屏（已修复）
- **修复：** `core.resize(w,h)` 设置 metrics（默认 1x1 导致渲染区域 1 像素）
- **修复提交：** 3e45b61

### 4. CLI 截图写到数据目录（已修复）
- **现象：** 交互模式 `render` 的 screenshot.png 写到 chdir 后的数据目录
- **修复：** 输出路径基于原 cwd 解析（`resolveOutputPath`）
- **修复提交：** 3e45b61

### 5. Qt CLI dock 中文输入法组合（已确认系统行为）
- **现象：** 模拟器下中文输入法会先组合拼音再上屏（IME 行为）
- **状态：** 系统行为，非 bug；后续可考虑 ArkTS 原生输入 + 命令通道替代
