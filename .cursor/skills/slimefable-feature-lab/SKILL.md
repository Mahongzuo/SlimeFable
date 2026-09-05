---
name: slimefable-feature-lab
description: >-
  Clone SlimeLab into a per-feature sandbox map with a large floor and only the
  new test content. Use when testing a new gameplay feature, spawning a new
  enemy/gadget into a lab, or when the user mentions FeatureLab, 功能沙盒,
  复制 SlimeLab, 测新功能, or falling through the default lab floor.
---

# SlimeFable 功能沙盒图

测新功能时**不要往原版 `SlimeLab` 里堆东西**，也不要重跑 `create_slime_lab.py`（它会清空整张图）。按 SlimeLab 的约定新建 `/Game/Maps/Sandbox/FeatureLabs/<Name>Lab`（同一套 GameMode / 日光 / PlayerStart），换成大地板，只摆这次要测的 Actor。不要 `duplicate_asset` 整张 umap（编辑器会因 World GC 崩掉）。

## 硬规则

1. 原版 `/Game/Maps/Sandbox/SlimeLab` 是挤压走廊基线，禁止当新功能堆场。
2. 每测一个功能一张图：`/Game/Maps/Sandbox/FeatureLabs/<Name>Lab`。
3. 图里只放**这次更新的内容**。旧走廊、旧箱子、上一次功能的 pawn 都清掉。
4. 地板默认 **12000×12000** uu（中心在原点）。PlayerStart 放在地板上 `(0, -400, 92)`，避免再掉到虚空。
5. 刷图用无头 Python，有 GUI 时加 `-nullrhi`。

## 脚本

`Content/Python/create_feature_lab.py`

```powershell
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\UE\SlimeFable\SlimeFable.uproject" "-ExecutePythonScript=E:/UE/SlimeFable/Content/Python/create_feature_lab.py --name GaspMover --spawn /Game/_Slime/Enemies/GASP/BP_GaspMoverEnemy --count 2 --patch-slimelab" -unattended -nop4 -nullrhi -nosound
```

| 参数 | 作用 |
|------|------|
| `--name` | 功能名，生成 `/Game/Maps/Sandbox/FeatureLabs/<Name>Lab` |
| `--spawn` | 可重复。要摆的 Blueprint 包路径 |
| `--count` | 每个 `--spawn` 生成几只，默认 1 |
| `--patch-slimelab` | 额外把原版 SlimeLab 地板加大（不删原图其它 Actor） |

Agent 测新功能：先跑这个脚本建/刷新对应 FeatureLab，再在那张图上 PIE。需要对照官方 GASP 关（DefaultLevel / NPCLevel / RagdollLevel）时另开官方图，不要把官方关改脏。

## 现成图

| 功能 | 地图 |
|------|------|
| GASP Mover 可吞噬敌人 | `/Game/Maps/Sandbox/FeatureLabs/GaspMoverLab` |

## 禁止

- 重跑 `create_slime_lab.py` 来「顺便」测新功能
- 在 366 日关卡或 `Main` 上堆沙盒 Actor
- 把上一功能的测试物留在新 FeatureLab 里
