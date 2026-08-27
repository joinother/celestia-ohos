# Celestia 鸿蒙移植 — 资源完整性审计报告

> 审计日期：2026-08-27
> 审计脚本：`tools/check-assets.py`（可随时重跑）
> 审计对象：`app/entry/src/main/resources/rawfile/celestia/`

## 结论

**全部资源完整，0 必需缺失**。审计通过。

| 检查项 | 引用数 | 缺失 |
|---|---|---|
| celestia.cfg 引用路径 | 59 | 0（2 个可选忽略） |
| 所有 .ssc 引用的 Mesh/Texture | 9038 | 0 |

## 本次修复记录

### 1. `textures/medres/asteroid.jpg`（缺失，已修复）
- **现象**：`data/comets.ssc` 引用 `asteroid.jpg`（彗星表面纹理）23 次，但数据包与官方数据仓库均无此文件（旧版 comets.ssc 引用了已移除的纹理）
- **修复**：以 `mathilde.jpg`（小行星 Mathilde 岩石表面，视觉最接近）复制为 `asteroid.jpg`

### 2. `logo.png`（缺失，已修复）
- **现象**：cfg 的 `LogoTexture "logo.png"` 引用的启动 Logo 缺失
- **修复**：以 `splash/splash.png`（启动图）复制为 `logo.png`

### 3. 可选资产（运行时/扩展，不判缺失）
- `favorites.cel`：用户收藏文件，Celestia 启动时自动生成
- `extras/wostars.stc`：旧版 extras 扩展星表（w/o stars），非必需

## 体积统计（上架评估）

**总体积 247.6 MB**

| 目录 | 体积 | 说明 |
|---|---|---|
| extras-standard | 103.2 MB | 航天器模型/纹理（阿波罗、卡西尼、哈勃、ISS 等） |
| data | 67.9 MB | 星表（stars.dat 等）、SSO 目录 |
| models | 42.6 MB | 天体 3D 模型（.cmod x 7085 / .3ds x 26） |
| textures | 29.7 MB | 表面纹理 |
| fonts | 1.5 MB | 字体 |
| 其他 | ~2.6 MB | locale/help/splash/shaders/scripts/images |

**上架建议**：
- 247.6 MB 数据 + 引擎 .so，HAP 预计 ~280-300 MB（Stellarium 鸿蒙版 460 MB 为参照）
- 若需减体积，`extras-standard`（103 MB，航天器模型）优先级最低，可裁剪或做成二级包
- 当前单机版策略下数据必须全内置，无需网络下载

## 复跑命令

```bash
python3 tools/check-assets.py
```
