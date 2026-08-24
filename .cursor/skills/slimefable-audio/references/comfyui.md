# ComfyUI 连接与故障

## 探活

`generate_audio.py` 对 `GET /system_stats` 按序探测：

1. 环境变量 `COMFYUI_URL`
2. `http://127.0.0.1:8000` — **ComfyUI Desktop 默认**
3. `http://127.0.0.1:8188` — `E:\AI\Comfy\run_comfy_gpu0.bat` 独立版

自定义：`--url http://127.0.0.1:PORT`。Desktop `portConflict: auto` 可能落到 8000–9000 其它口。

本机 UE MCP 用 **8010**，正是因为 8000 常被 Desktop 占用。不要把 UE MCP 改回 8000。

## 前提

- 保持 **ComfyUI Desktop 开着**；Skill **不**自动启动。
- 不要同时开 Desktop 与 `run_comfy_gpu0.bat`（抢 GPU / 端口）。
- 无头导入 UE 使用 `-nullrhi -nosound`，减轻与 Desktop 抢显卡。

## API 流程

1. `POST /prompt`（API 格式 graph）
2. 轮询 `GET /history/{prompt_id}`
3. `GET /view?filename=...&subfolder=...&type=output` 下载
4. 写入工程 `Content/...`（优先 wav；mp3 则保留并尝试导入）

## 工作流快照

目录：`scripts/workflows/`

| 文件 | 用途 |
|------|------|
| `audio_stable_audio_3_sfx_v2.json` | UI 快照：官方名存在于 Desktop 模板库，仓库快照时用 Medium 代替；见同目录 `README_sfx_v2.txt` |
| `audio_stable_audio_3_medium.json` | UI 快照：背景乐 |
| `audio_minimax_music_3.json` | UI 快照：人声+歌词 |
| `api_stable_audio_3_sfx_v2.json` | **实际提交** SFX（`stable_audio_3_small_sfx`） |
| `api_stable_audio_3_medium.json` | **实际提交** BGM（`stable_audio_3_medium`） |
| `api_minimax_music_3.json` | **实际提交** theme |

CLI 提交 API JSON（改 prompt/duration/seed/保存前缀），不直接 POST 带子图的 UI 模板。SFX 走 Stable Audio + 短时长 + SFX 向提示词。

## 超时建议

| kind | 超时 |
|------|------|
| sfx | ~120s |
| bgm | ~600s |
| theme | ~900s |

失败时打印 history 里的 node errors。

## 常见问题

| 现象 | 处理 |
|------|------|
| 探活失败 | 打开 Desktop；或 `--url` 指到实际端口 |
| 模型缺失 | 在 Desktop 打开对应模板，按提示下载 checkpoint / text encoder |
| UE 导入锁工程 | 关 GUI 编辑器再 `--import-ue`，或稍后手动 import |
| 输出是 mp3 | 脚本仍落盘；导入优先 wav，mp3 也可试 UE 导入 |
