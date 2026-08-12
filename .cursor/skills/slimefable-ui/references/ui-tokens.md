# SlimeFable UI Tokens

实现以 `Source/SlimeFable/UI/MenuUIStyle.cpp` 为准；本表供 Agent 对齐，改色时先改 `FMenuUIStyle` 再扩散。

## 色板（线性近似）

| Token | RGB 约值 | 用途 |
|-------|----------|------|
| WarmTitle | `(0.97, 0.93, 0.84)` | 标题、月份 |
| WarmText | `(0.93, 0.90, 0.82)` | 按钮字、日格字 |
| WarmMuted | `(0.78, 0.74, 0.66)` | 次要/错误提示 |
| TodayFill | `(0.42, 0.32, 0.18)` α≈0.88 | 今日格底 |
| TodayEdge | `(0.92, 0.72, 0.32)` | 今日描边 / 强调 |
| DayCellFill | `(0.08, 0.07, 0.06)` α≈0.72 | 普通日格 |
| DayPanelFill | `(0.05, 0.045, 0.035)` α≈0.62 | 日期区半透底 |
| DayPanelEdge | `(0.72, 0.64, 0.46)` α≈0.35 | 日期区弱描边 |
| DimOverlay | `(0.05, 0.04, 0.03)` α≈0.45–0.48 | 全屏压暗 |
| BgTint | `(0.62, 0.58, 0.52)` | 背景 Texture 染色压暗 |

忌用：高饱和品红/电光蓝/纯霓虹描边。

## 字体资产

| 用途 | 资产 / 文件 |
|------|-------------|
| Marker（标题、数字） | `/Game/UIMaterialLab/Fonts/PermanentMarker-Regular_Font` |
| KuaiLe（中文） | `Content/UI/Fonts/ZCOOLKuaiLe-Regular.ttf`（运行时路径）；可选导入 `/Game/UI/Fonts/Font_ZCOOLKuaiLe` |
| 许可 | `Content/UI/Fonts/OFL-ZCOOLKuaiLe.txt`（OFL） |
| 混排 | `MakeMixedMenuFont`：Default=Marker，CJK ranges=KuaiLe |

## 材质与贴图

| 用途 | 路径 |
|------|------|
| 菜单背景 | `/Game/UI/Textures/T_MenuBackground`（源图可在 `Content/UI/Textures/Source/`） |
| 主按钮墨迹 | `/Game/UIMaterialLab/Widgets/ComponentMaterials/MaterialInstances/MI_UI_Button` |
| 生图参考脚本（非必须） | `D:\Softwares\Godot\OpenAI\scripts\gpt_image_2.py`；风格：偏写实略 NPR、低饱和土色、中心略暗便叠字 |

MaterialLab 其它 MI（Halftone、Button2、Slot 卡面）**不作为**菜单默认皮。

## 布局习惯

- 主菜单：垂直居中；标题 →（无说明行）→ 三等宽墨迹按钮（约 360×64）
- 选关：居中；紧凑月份行 → 仅日区 `DayPanel` → 框外「返回」
- 日格：约 72×72，圆角半透格，Marker 数字
- 暗层：`BackgroundImage` 下可加全屏 `DimOverlay`（Box 刷，勿用怪异大圆角造成光斑）

## 相关脚本

- `Content/Python/create_menu_wbps.py` — 空 WBP 壳
- `Content/Python/import_menu_background.py` — 背景导入辅助
- `Content/Python/open_level_select_pie.py` — PIE 打开选关（调试）
