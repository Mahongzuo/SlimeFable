# 音频目录与提示词

## 全局路径

| 类别 | Content 路径 | 资产名前缀 |
|------|--------------|------------|
| 全局 BGM | `/Game/Audio/BGM/` | `bgm_global_*` |
| 战斗 SFX | `/Game/Audio/SFX/Combat/` | `sfx_*` |
| 移动 SFX | `/Game/Audio/SFX/Movement/` | `sfx_*` |
| 环境 SFX | `/Game/Audio/SFX/Ambient/` | `sfx_*` |
| 全局主题曲 | `/Game/Audio/Theme/` | `theme_global_*` |

已有示例：`/Game/Audio/SFX/sfx_fruitslice_01`（历史路径，新 SFX 放子目录）。

## 关卡路径

```text
/Game/_Slime/Days/{MM}/{MMDD}/Audio/
  BGM/     bgm_{MMDD}_explore, bgm_{MMDD}_combat
  SFX/     sfx_{MMDD}_{id}
  Theme/   theme_{MMDD}_*
```

多章日（如 0815）若指定年份：`.../0815/Y{Year}/Audio/`。

## 关卡默认包（`--pack day`）

1. `bgm_{day}_explore` — 探索 BGM，60s，乐器
2. `bgm_{day}_combat` — 战斗 BGM，60s，乐器

人声主题曲不在默认包内；用户说歌词/人声/主题曲再用 `--kind theme`。

## catalog.json 常用位（点名再生成）

- `slash_01` / `hit_01` — 挥砍、受击
- `footstep_01` / `jump_01` — 脚步、跳跃
- `bgm_global_explore` / `bgm_global_combat` / `bgm_global_menu`

## 提示词风格

- 英文；土色 NPR、洞穴/遗迹、史莱姆冒险感
- BGM：warm acoustic / soft percussion / adventure underscore；忌霓虹电子乐（除非用户要）
- SFX：短、清晰、游戏 one-shot；写材质与动作（slime slap、wet impact、stone footstep）
- Theme（MiniMax）：Caption 分 Global Metadata / Vocal Details / Arrangement；Lyrics 用 `[Intro]` `[Verse]` `[Chorus]` 等标签
