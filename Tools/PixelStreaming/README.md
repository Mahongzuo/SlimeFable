# 本机连接页 + been.chat `/play`

主站 `/www/wwwroot/been.chat/index.html` **不要动**。像素流送只走 `/play` 与 `/ps`。

## 本机（先做这个）

1. Infrastructure 已克隆到 `Tools/PixelStreaming/PixelStreamingInfrastructure/`（gitignore）。若缺失：`powershell -File Tools/PixelStreaming/clone_infra.ps1`
2. 复制 `worker.config.example.json` 为 `worker.config.json`，改口令。`target` 用 `local` 或 `cloud`。
3. 启动信令与连接页：

```text
py Tools/PixelStreaming/start_local.py
```

首次会弹出官方 `start.bat` 编译 Wilbur（需 Node）。完成后：

- 连接页：http://127.0.0.1:8090/
- 状态：http://127.0.0.1:8091/play/api/status
- 播放器：http://127.0.0.1:18880/player.html
- 推流：`ws://127.0.0.1:18888`

4. 心跳：`py Tools/PixelStreaming/worker.py --target local`（打包双开再加 `--launch`）
5. 游戏画质菜单打开「像素流送」，或 Worker 自动推流。
6. 浏览器打开连接页，选 **本机** 或 **云端**，填口令，再点游玩/观看。

Wilbur 端口固定 **18888 / 18880**，避开宝塔 8888。

## 轻量云 been.chat（主站不动）

1. 宝塔「been.chat → 配置文件」**只追加** `cloud/baota-append.conf`，保存前 `nginx -t`。
2. 上传 `cloud/play/index.html` → `/www/wwwroot/been.chat/play/index.html`
3. 服务器执行 `cloud/clone_infra.sh`，按官方脚本编一次 Wilbur，再把 `SignallingWebServer/www/` 拷到 `/www/wwwroot/been.chat/play/player/`
4. 环境变量 `SLIME_PLAY_TOKEN` 与本机 `worker.config.json` 的 `token` 相同。`bash cloud/start_signalling.sh`（或宝塔进程守护）
5. 安全组只开 80/443。不要放行 18888/18880/8091。
6. 本机改 `py Tools/PixelStreaming/worker.py --target cloud`，玩家打开 `https://been.chat/play?k=口令`

## 座位

座位 1/2 对应两张 GPU。同一座位：先点游玩的人操作，观看只看。
