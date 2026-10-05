---
name: slimefable-morph-materials
description: >-
  Slime morph visual rules for multi-slot meshes: all slots wear Substrate slime
  skin during grow/blend/unblend, restore originals only when Morphed. Use when
  editing slime morph, devour-to-morph, WorldGrid Face/Up/Hair, Overlay mixing
  Substrate with DefaultLit, or 0815 samurai/Phoebe materials during 幻形.
---

# SlimeFable 幻形材质（多槽角色）

修吞噬后幻形外观、Face/Up 头发掉棋盘格或仍露原材质时读本文件。C++ 入口：`Source/SlimeFable/Slime/SlimeMorphComponent.cpp`。刷材质：`Content/Python/create_slime_morph_material.py`。本体：`Content/Python/create_slime_material.py`。

## 硬规则

1. **同一 `UMeshComponent` 过渡期非 Hair 槽必须同一着色家族。** 禁止 DefaultLit `M_SlimeMorph` 和 Substrate Toon 原皮混在一张网上（Face/Up 会变成 WorldGrid）。
2. **过渡期（Growing / Blending / Unblending / Shrinking）** 非 Hair 槽默认穿 `/Game/Characters/Slime/Materials/M_SlimeMorph_Luminous`（`bLuminousTransitionSkin`，默认开）。关掉或资产缺失、或该网格没有非 Nanite 回退时，回退 `/Game/Characters/Slime/Materials/M_SlimeMorph_Substrate`。**Hair VF 槽（`MSM_HAIR` 或槽名含 Hair）穿** `/Game/Characters/Slime/Materials/M_SlimeMorph_Hair`（Masked MSM_HAIR，宽彩边 + 细亮边）。主网格 + BP 附加网格 + Groom + `GeneratedParts`，跳过占位立方体。Growing/Shrinking 可隐藏 extra parts，一旦显示必须已是史莱姆皮。收集用 Actor 上全部 `UMeshComponent`，不要只扫 `GeneratedParts`。
3. **Morphed 才 `ApplyOriginalMaterials()`。** 不要在 Blending 入口还原原发/原脸再靠 Overlay。套皮时清掉 `OverlayMaterial`（受击闪盖不住 Hair VF）。**Morphed 时连同 `SavedOverlay` 一起还原**（菲比描边等常驻 Overlay）。
4. 非 Hair 默认皮是 **半透明荧光果冻** `M_SlimeMorph_Luminous`：`Opacity` 输出恒为 1，玻璃色走 Emissive 里的场景色（和 `M_SlimeBody_Luminous` 同一套，不吃半透光照，所以不会变成灰泥）。三条约束：
   - **只画最前表面**：过渡期网格写 CustomDepth（Stencil = 1），材质 `clip` 掉比 CustomDepth 远过 `FrontDepthBias`（默认 6cm）的片元。
   - **临时关 Nanite**：`SetForceDisableNanite(true)`，Morphed / 退出时还原。没有非 Nanite 回退的网格改穿 Substrate Toon 并打 Warning。
   - **不要**把这张皮改成半透 Substrate Slab / Colored Transmittance / Simple Volume。也不要开 Output Velocity：果冻反射要读 SceneDepth，半透材质同时写速度会写深度，引擎直接拒编。
   生长遮罩是 object-space `GrowProgress` 上的 `clip`，过渡期不要把 `ShellOpacity` 淡到 0。回退皮 `M_SlimeMorph_Substrate` 仍是 Masked Substrate Toon（OpacityMask）。Hair 槽生长走 OpacityMask。参数名对齐 C++：`GrowProgress` / `EdgeSoftness` / `ShellOpacity` / `BaseColor` / `EmissiveColor` / `MorphBrightness`。Luminous 父材质不要乘 `MorphBrightness`（那是给 Toon / Hair 提亮的）。
5. 不要按角色名写死 Phoebe / Samurai / 0815；按「多槽 + extra parts + Groom + Hair VF」处理。
6. 不要为了幻形去改角色原 Toon/Hair 资产。禁止「只退回 Masked Toon 而漏掉头发」。**仅 Hair 槽**套皮失败才 `HideMaterialSection`；禁止用指针比较藏 Face/Up。
7. **禁止**把 `/Game/FluidNinjaLive/UseCases/016_Caustics/MI_Water_SingleLayer_CausticsDemo` 当角色皮。本体 `M_SlimeBody` 保持原项目 Fresnel 果冻，不要加 PNO / 焦散 / Custom 法线。
8. **幻形皮必须带 `bUsedWithNanite`**（`_set_mesh_usage` 已设 `used_with_nanite`，含 `M_SlimeMorph_Luminous`）。Lyra `SKM_Manny` 是 Nanite 骨骼网格；缺这个标记 PIE 日志报 `missing usage flag Nanite! Default Material will be used`。Luminous 半透本身不能走 Nanite，过渡期另外 `SetForceDisableNanite`，Morphed 时还原。改了 usage 后必须无头重刷并重启编辑器。
9. **视觉验证归用户。** 描边、套皮、面部阴影等观感由用户在视口 / PIE 亲自确认。实现做完就停；禁止凭截图或自己判断去改线宽 / TwoSidedSign / 颜色 / 曝光。用户已说效果不错后，只改用户点名的问题，不要顺手「优化」同一视觉点。

## 资产

| 资产 | 用途 |
|------|------|
| `M_SlimeMorph_Luminous` | 幻形默认底皮（半透荧光果冻，非 Hair 槽，Opacity=1） |
| `M_SlimeMorph_Substrate` | 回退底皮（Masked Substrate Toon，非 Hair 槽） |
| `M_SlimeMorph_Hair` | Hair VF 槽史莱姆皮（Masked MSM_HAIR，荧光宽彩边 + 细亮边） |
| `M_SlimeMorph` | DefaultLit Overlay，**幻形底皮不用** |
| `M_SlimeMorph_Grow` | 旧 Masked DefaultLit，spawn 不用 |
| `M_SlimeBody` | 史莱姆本体（`D:\AI\SlimeFableProj` 原版 SIM Fresnel 果冻，无折射、无焦散） |

Reveal mask 用 object-space（LocalPosition + ObjectLocalBounds），不要 world-Z。

本体不要改 SceneColor PNO、Custom→Normal、焦散贴图。`ForEachVisualMesh` 必须跳过 `IsVisualizationComponent` 和挂在 `UCameraComponent` 下的网格（`MatineeCam_SM`），否则幻形会把相机轮廓套上史莱姆皮并强制显示。

## 改材质

```powershell
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\UE\SlimeFable\SlimeFable.uproject" -ExecutePythonScript="E:/UE/SlimeFable/Content/Python/create_slime_morph_material.py" -unattended -nop4 -nullrhi -nosound
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\UE\SlimeFable\SlimeFable.uproject" -ExecutePythonScript="E:/UE/SlimeFable/Content/Python/create_slime_material.py" -unattended -nop4 -nullrhi -nosound
```

无头日志必须出现 `Substrate Toon, masked` 和 `Luminous translucent`。

## 禁止

- 一张网上混 Substrate 与非 Substrate（Hair VF 专用皮除外）
- Blending 时只给身体换皮、头发/Face/Up 留原材质
- 用 `HideMaterialSection` + 指针比较判断套皮失败（Substrate 槽会误藏 Face/Up）
- 把幻形底皮改成半透 Substrate Slab / Adaptive 玻璃（荧光果冻是 Opacity=1 的 Emissive 场景色，不是 Slab）
- 整张替换 FluidNinja 单层水 MI 当史莱姆/幻形皮
