# Celestia 鸿蒙移植 — 汉化（本地化）专项说明

> 更新：2026-08-27

## 一、诊断结论（用户反馈：界面英文 + 小方块）

截图现象：
- 左上角天体名 `Earth`、信息面板英文
- 右上角时间流速处出现**两个小方块（□）**
- 日期 `Thu Aug 27 ...` 英文

### 根因（三层）

| 现象 | 根因 | 是否我们的问题 |
|---|---|---|
| 右上角两个小方块 | **中文字体缺失**：fonts/ 只有 DejaVuSans（无中文字形）。`Real time` 在中文 locale 下翻译为"实时"，缺字形 → 渲染成 □□ | **是**（未打包中文字体） |
| 天体名 `Earth` 英文 | **`celestia-data.mo` 缺失**：天体名用 `D_()` = `dgettext("celestia-data", name)`，独立 gettext 域，需单独 .mo。上游 CMake 只生成 celestia.mo，**从未提供 celestia-data.mo** | 上游缺陷，我们补齐 |
| 部分信息面板英文 | 早期误判翻译缺失；**实际 celestia.mo 翻译完整**（931 条），仅个别 key 格式瑕疵 | 已修正 |

### 汉化机制（重要）

- **Celestia 语言靠系统 locale**：`setlocale(LC_ALL,"")`，**cfg 里没有 Language 指令**（勿在 cfg 加，无效）。
- 鸿蒙构建 `ENABLE_NLS=ON` + `CELESTIA_OHOS=ON`，qtmain 正确 `bindtextdomain` 到 `<数据目录>/locale`。
- **双 gettext 域**：
  - `celestia` 域 → UI 文本（`_()`）→ `locale/zh_CN/LC_MESSAGES/celestia.mo`
  - `celestia-data` 域 → 天体名（`D_()`）→ `locale/zh_CN/LC_MESSAGES/celestia-data.mo`
- 天体名翻译时机：`Body::setName()` 调 `D_(name)`，**必须在 gettext 初始化之后**（鸿蒙 qtmain 顺序正确：initLocale → bindtextdomain → startAppCore）。

## 二、修复内容

### 1. 中文字体（解决小方块）
- 新增 `fonts/NotoSansCJKsc-Regular.otf`（16.4 MB，**SIL OFL 1.1 开源许可，上架合规**）
- `celestia.cfg` 配置 Font / LabelFont / TitleFont 全部指向 Noto（含 Latin + CJK 字形）
- 注意：DejaVuSans 无中文字形，**不可作为唯一字体**

### 2. `celestia-data.mo`（解决天体名英文）
- 上游缺失，自研生成 73 条标准天文学译名（太阳/八大行星/主要卫星/矮行星/小行星/彗星/大陆大洋）
- 生成工具：`tools/gen-celestia-data-mo.py`（自写 .mo 编译器，含 header + UTF-8）
- 仅影响显示，不影响内部引用（`findObjectFromPath` 用非本地化名）

### 3. `celestia.mo` 翻译修正
- 修正 `Equatorial (polar) radii` 翻译格式（丢失第二个占位符）
- 工具：`tools/patch-celestia-mo.py`（读全 931 条 → 修正 → 写回，备份 .bak）

## 三、验证

- `celestia-data.mo`：python gettext 校验 Earth→地球、Moon→月球、Mars→火星、Sun→太阳、Titan→土卫六、AFRICA→非洲 ✓
- `celestia.mo`：931 条完整，Equatorial radii/Distance/Real time 翻译正确 ✓
- mac 本机 NLS=OFF（无 libintl），无法本地复现中文渲染；**鸿蒙端 NLS=ON**，重新构建 HAP 后生效

## 四、重新构建后预期效果（鸿蒙系统语言为中文时）

- 天体名：地球、月球、火星……（中文）
- UI：距离/视直径/相位角/质量/密度……（中文）
- 时间流速：实时（中文，不再方块）
- 日期：由 Celestia 日期格式化决定（月份缩写可能仍英文，属上游限制）

## 五、相关工具

```bash
# 天体名翻译生成（改 TRANSLATIONS 表后）
python3 tools/gen-celestia-data-mo.py
# UI 翻译修正
python3 tools/patch-celestia-mo.py
```
