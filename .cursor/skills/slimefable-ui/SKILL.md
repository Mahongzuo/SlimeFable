---
name: slimefable-ui
description: >-
  SlimeFable UMG visual language: earthy NPR menu, Permanent Marker + ZCOOL KuaiLe
  fonts, ink-brush buttons, calendar day-select chrome. Use when editing menus,
  widgets, HUD, UI fonts/textures, FMenuUIStyle, MainMenu/LevelSelect/DaySlot, or
  when the user mentions 主菜单、选关、日历 UI、按钮样式、背景图.
---

# SlimeFable UI 风格（必遵）

做任何新 UI / 改现有菜单之前：读完本文件；配色与资产路径细节见 [references/ui-tokens.md](references/ui-tokens.md)。

实现入口以 C++ `FMenuUIStyle` + 菜单 Widget 为准，不要另起一套 MaterialLab 霓虹风格。

## 视觉方向（一句话）

偏写实略 NPR 的洞穴/遗迹氛围：土黄、苔绿、岩褐；UI 用暖米白字 + 墨迹笔刷按钮；忌霓虹粉紫、电光蓝、高饱和 Halftone。

## 硬规则

1. **统一走 `FMenuUIStyle`**：颜色、字体、背景、主按钮材质都经 [`Source/SlimeFable/UI/MenuUIStyle.h`](../../../Source/SlimeFable/UI/MenuUIStyle.h) 应用；禁止在 Widget 里硬编码另一套高饱和色或 Roboto/雅黑当正文默认。
2. **背景**：全屏菜单用 `/Game/UI/Textures/T_MenuBackground`（`ApplyMenuBackground`）。禁止把 MaterialLab Halftone 粉紫点阵当主菜单底。
3. **按钮**：主操作统一 `MI_UI_Button` 墨迹笔刷（`LoadButtonMaterial` + `ApplyMaterialButtonStyle`）。禁止蓝胶囊 `MI_UI_Button2` 当分主按钮。
4. **字体**
   - 英文标题 / 纯数字：`Permanent Marker`（`ApplyTitleFont` / `ApplyMarkerFont`）
   - 中文：`ZCOOL KuaiLe`（`ApplyBrushCJKFont`；缺省回退系统雅黑并打 Warning）
   - 中英混排（如 `进入今日关卡（0812）`）：`ApplyMixedMenuFont`（Marker 数字 + KuaiLe 汉字）
5. **文案克制**：主菜单不要「历史上的今天…」「选择进入今日…」类说明行；今日信息并进主按钮：`进入今日关卡（MMDD）`。错误提示可短暂显示，正常态 Collapsed。
6. **选关日历**
   - 有月初星期偏移的日期格；**不要**「一…日」星期表头
   - 月份行：`<` + 居中月份 + `>` **紧挨**（不要拉满两侧）
   - **只给日期区**半透圆角底 + 弱暖金描边；月份行不要外框
   - 今日格暖金高亮；格子数字用 Marker
7. **可读性**：背景可略压暗；可用轻 `DimOverlay`；正文用暖米白，勿纯炫白霓虹描边。
8. **UMG 时序**：`CreateWidget` 后立刻设文案时，Widget 树可能未建好——像 `DaySlot` 一样缓存状态并在 `RebuildWidget` / `NativeConstruct` / `ApplyVisuals` 再应用。

## 现有 Widget（扩展时对齐）

| Widget | 路径 / 类 | 职责 |
|--------|-----------|------|
| 主菜单 | `UMainMenuWidget`，`/Game/UI/WBP_MainMenu` | 标题 + 三墨迹按钮；今日并进主按钮 |
| 选关 | `ULevelSelectWidget`，`/Game/UI/WBP_LevelSelect` | 月份导航 + 日历格 + 返回 |
| 日格 | `UDaySlotWidget`，`/Game/UI/WBP_DaySlot` | 数字格 / 今日高亮 / 点击 DayId |

空 WBP 可走 C++ `RebuildWidget` 拼布局；Designer 绑定优先 `BindWidgetOptional`。

## 新 UI 检查清单

- [ ] 是否调用 `FMenuUIStyle`（背景 / 字体 / 按钮）？
- [ ] 是否避免 Halftone 霓虹与蓝胶囊主按钮？
- [ ] 数字是否 Marker、中文是否 KuaiLe/Mixed？
- [ ] 文案是否短、信息是否挂在按钮或必要标题上？
- [ ] 选关是否仍无星期表头、月份箭头是否紧凑、仅日区有半透底？
- [ ] PIE 中文无方框、主按钮含当日 `MMDD`？

## 不要做

- 紫粉点阵 / 高饱和渐变当默认菜单皮
- Inter/Roboto/系统雅黑当唯一正文字体（雅黑仅作 KuaiLe 缺失回退）
- 卡片式 `MI_UI_Slot_*` 贴图盖住日期数字
- 为「好看」并行第二套互不兼容的 UI 主题

手动调用：`/slimefable-ui`。
