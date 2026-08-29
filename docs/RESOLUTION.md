# 渲染清晰度（分辨率）专项说明

> 记录 Celestia 各版本"清晰度差异"的根因、以及本项目的修复方案。

## 问题现象

用户反馈：
- 我们移植的鸿蒙版 / Mac 官网桌面版 → **分辨率偏低、发糊**
- App Store 的 Mobile 版（MobileCelestia）→ **清晰**

## 根因（Celestia 的纹理三级体系）

Celestia 的星球表面纹理按分辨率分三档（`textures/` 下三个目录），由渲染器的
`TextureResolution` 决定加载哪一档：

| 目录 | 纹理尺寸 | 用途 |
|---|---|---|
| `textures/lores` | 1K（1024x512） | 低清 |
| `textures/medres` | 2K（2048x1024） | **桌面版默认** |
| `textures/hires` | 4K（4096x2048） | 高清，**Mobile 版使用** |

### 三个叠加因素

1. **桌面版默认 medres（2K）**：Qt 前端 `qtglwidget.cpp` 的
   `DEFAULT_TEXTURE_RESOLUTION = medres`，在电脑高分屏/大屏上放大后发糊。
   这是官网 Mac 版"低清"的直接原因。
2. **medres 数据包缺少地球表面颜色纹理**：官方 `CelestiaContent` 的 medres
   目录**没有 `earth.jpg/earth.png`**（只有法线贴图 `earth-normal`）。因此
   medres 下地球只能用法线+漫反射色渲染成"蓝色圆盘"——没有大陆细节。
3. **MobileCelestia 用 hires（4K）+ 高分屏原生渲染**，所以 App Store 版清晰。

## 本项目修复（提交 bc8a9e1）

1. **打包 12 张 hires（4096x2048）纹理**（约 32MB）：
   地球（表面/云层/夜景）+ 八大行星 + 冥王星 + 月球。
   来源：官方 `CelestiaProject/CelestiaContent` 仓库 `textures/hires/`。
2. **medres 兜底**：把 hires 地球纹理复制为 `textures/medres/earth.jpg`，
   即使用户手动切回 Medium 纹理，地球也保留大陆。
3. **Qt 前端默认 hires**：`qtglwidget.cpp` 的
   `DEFAULT_TEXTURE_RESOLUTION` 由 `medres` 改为 `hires`，鸿蒙版开箱即用 4K。
4. **修复 CLI 异步纹理 bug**：`CelestiaCore::drainResources()` 新增公开接口，
   上传后台解码完成的纹理到 GPU；`celestia-cli` 改为"先渲染一帧触发加载 →
   等待解码 → 上传 GPU → 再渲染最终帧"。此前 CLI 单帧渲染从不 drain
   就绪队列，永远看不到表面纹理（只出纯色球）。

## 验证

- CLI 渲染地球（hires 4K 纹理）→ 明确可见北美/南美大陆轮廓（修复前为纯蓝球）。
- 纹理路径解析确认：`textures/medres/earth.jpg` 与 `textures/hires/earth.png`
  均被 `TexturePaths::checkPath` 正确解析（通配符 `earth.*` → ResolveWildcard）。

## 备注

- 用户可在 Qt 菜单 渲染→纹理分辨率 中在 低/中/高 之间切换（仍会保存设置）。
- hires 纹理只覆盖主要天体；卫星/矮行星的 hires 纹理可按需从
  `CelestiaProject/CelestiaContent` 补充（注意体积）。
- 移动端（未来鸿蒙手机/平板）可参考 MobileCelestia 的触屏+原生分辨率策略，
  但引擎仍复用同一份 master（见版本选型说明）。
