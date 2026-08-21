#!/usr/bin/env python3
"""Start local status_server, Wilbur (18888/18880), and the connect page (8090)."""

from __future__ import annotations

import os
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
INFRA = HERE / "PixelStreamingInfrastructure"
WILBUR = INFRA / "SignallingWebServer"
STATUS = HERE / "cloud" / "status_server.py"
SERVE = HERE / "local" / "serve.py"
START_BAT = WILBUR / "platform_scripts" / "cmd" / "start.bat"


def spawn(args: list[str], cwd: Path | None = None, env: dict[str, str] | None = None) -> subprocess.Popen:
    creation = getattr(subprocess, "CREATE_NEW_CONSOLE", 0) if sys.platform == "win32" else 0
    merged = os.environ.copy()
    if env:
        merged.update(env)
    return subprocess.Popen(args, cwd=str(cwd) if cwd else None, env=merged, creationflags=creation)


def main() -> int:
    if not INFRA.is_dir():
        print("Missing PixelStreamingInfrastructure. Run Tools/PixelStreaming/clone_infra.ps1")
        return 1

    token = os.environ.get("SLIME_PLAY_TOKEN", "change-me")
    kids: list[subprocess.Popen] = []
    kids.append(
        spawn(
            [sys.executable, str(STATUS)],
            cwd=HERE / "cloud",
            env={
                "SLIME_PLAY_TOKEN": token,
                "SLIME_STATUS_HOST": "127.0.0.1",
                "SLIME_STATUS_PORT": "8091",
            },
        )
    )
    kids.append(spawn([sys.executable, str(SERVE), "--host", "127.0.0.1", "--port", "8090"]))

    wilbur_args = [
        "--config_file",
        str(HERE / "local" / "wilbur.config.json"),
        "--streamer_port",
        "18888",
        "--player_port",
        "18880",
        "--sfu_port",
        "18889",
        "--serve",
        "--http_root",
        "www",
        "--homepage",
        "player.html",
    ]
    if START_BAT.is_file() and sys.platform == "win32":
        kids.append(spawn(["cmd", "/c", str(START_BAT), "--", *wilbur_args], cwd=START_BAT.parent))
    else:
        dist = WILBUR / "dist" / "index.js"
        if not dist.is_file():
            print("Wilbur is not built. On Windows run SignallingWebServer/platform_scripts/cmd/start.bat once.")
        else:
            kids.append(spawn(["node", str(dist), *wilbur_args], cwd=WILBUR))

    print("status  http://127.0.0.1:8091/play/api/status")
    print("connect http://127.0.0.1:8090/")
    print("player  http://127.0.0.1:18880/player.html")
    print("streamer ws://127.0.0.1:18888")
    print("Ctrl+C stops the Python helpers. Close the Wilbur window separately.")
    try:
        while True:
            time.sleep(1)
            if any(p.poll() is not None and p is kids[0] for p in kids[:1]):
                break
    except KeyboardInterrupt:
        pass
    for proc in kids:
        if proc.poll() is None:
            proc.terminate()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
