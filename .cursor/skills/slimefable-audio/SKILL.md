---
name: slimefable-audio
description: >-
  Generate SlimeFable game music and SFX via local ComfyUI Desktop (BGM, combat
  SFX, footsteps, theme songs with lyrics). Use when the user asks to generate
  music, 音效, BGM, SFX, 主题曲, day-level audio, or mentions ComfyUI audio.
---

# SlimeFable 音乐 / 音效生成

通过本机 **ComfyUI Desktop**（HTTP API）生成全局或关卡音频，落盘到 Content，可选无头导入 SoundWave。细节见 [references/catalog.md](references/catalog.md)、[references/comfyui.md](references/comfyui.md)。

## 何时用

用户说例如：

- 「生成 0815 关音乐」→ `--pack day --day 0815`（探索 BGM + 战斗 BGM）
- 「生成史莱姆挥砍音效」→ `--kind sfx --scope global --id slash_01`
- 「给 0815 做一首带歌词的主题曲」→ `--kind theme`（MiniMax Music 3）

## 开工清单

1. 确认 ComfyUI Desktop 已开（不自动拉起；勿同时开 `run_comfy_gpu0.bat`）。
2. 读 [catalog.json](scripts/catalog.json) 定 scope / id / 默认时长。
3. 用土色 NPR / 史莱姆冒险风格写 **英文** 提示词（忌霓虹电子乐，除非用户要）。
4. 跑 `scripts/generate_audio.py`（Agent 只调 CLI，不手写 HTTP）。
5. 回报 `/Game/...` 资产路径。用户明确要求接到角色/菜单时再改 C++；默认只生成不接线。

## CLI

```text
py E:/UE/SlimeFable/.cursor/skills/slimefable-audio/scripts/generate_audio.py ^
  --kind sfx|bgm|theme --scope global|day --day 0815 ^
  --id slash_01 --prompt "..." [--lyrics "..."] [--duration 2] [--import-ue]

py E:/UE/SlimeFable/.cursor/skills/slimefable-audio/scripts/generate_audio.py ^
  --pack day --day 0815 --import-ue
```

探活：`COMFYUI_URL` → `127.0.0.1:8000`（Desktop）→ `:8188`（独立版）。自定义端口用 `--url`。

## 工作流

| kind | 工作流 | 默认 |
|------|--------|------|
| `sfx` | Stable Audio Small-SFX（`api_stable_audio_3_sfx_v2`） | 2s |
| `bgm` | Stable Audio 3 Medium | 60s |
| `theme` | MiniMax Music 3 | 人声+歌词；仅用户明确要歌词/人声时用 |

关卡包默认：**探索 BGM + 战斗 BGM**（乐器，不走 theme）。

## 路径

```
/Game/Audio/BGM|SFX/Combat|SFX/Movement|SFX/Ambient|Theme/
/Game/_Slime/Days/{MM}/{MMDD}/Audio/BGM|SFX|Theme/
```

多章日点名年份 → `.../MMDD/Y{Year}/Audio/`。命名小写+下划线；同 id 覆盖。

## 不要做

- MCP / 循环扫 366 关批量出音频
- 用户没点名的 catalog 条目预生成
- 自动拉起 Comfy 或同时开 Desktop + `run_comfy_gpu0.bat`
- 擅自改运行时 AudioSubsystem / 战斗默认音效路径（除非用户明确要求接线）
