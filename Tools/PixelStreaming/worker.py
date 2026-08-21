#!/usr/bin/env python3
"""Local Pixel Streaming worker: heartbeat two seats, optionally launch packaged game."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_CONFIG = HERE / "worker.config.json"


def load_config(path: Path) -> dict:
    if not path.is_file():
        example = HERE / "worker.config.example.json"
        raise SystemExit(
            f"Missing {path}. Copy {example.name} to worker.config.json and fill domain/token."
        )
    return json.loads(path.read_text(encoding="utf-8"))


def resolve_endpoints(cfg: dict, target_override: str | None) -> tuple[str, str, str]:
    target = (target_override or cfg.get("target") or "local").strip().lower()
    block = cfg.get(target) if isinstance(cfg.get(target), dict) else {}
    status_url = str(block.get("status_url") or cfg.get("status_url") or "")
    signalling_url = str(block.get("signalling_url") or cfg.get("signalling_url") or "")
    if not status_url or not signalling_url:
        raise SystemExit(f"worker config missing status_url/signalling_url for target={target}")
    return target, status_url, signalling_url


def post_status(url: str, token: str, payload: dict) -> None:
    body = json.dumps({"token": token, **payload}).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=8) as resp:
        resp.read()


def process_alive(proc: subprocess.Popen | None) -> bool:
    return proc is not None and proc.poll() is None


def launch_seat(cfg: dict, seat: dict) -> subprocess.Popen:
    exe = Path(cfg["game_exe"])
    if not exe.is_file():
        raise FileNotFoundError(exe)
    args = [
        str(exe),
        f"-graphicsadapter={int(seat['gpu'])}",
        f"-PixelStreamingID={seat['streamer_id']}",
        f'-PixelStreamingConnectionURL="{cfg["signalling_url"]}"',
        "-PixelStreamingAutoStartStream=1",
        "-PixelStreamingInputController=Host",
    ]
    creation = 0
    if sys.platform == "win32":
        creation = getattr(subprocess, "CREATE_NEW_CONSOLE", 0)
    return subprocess.Popen(args, creationflags=creation)


def main() -> int:
    parser = argparse.ArgumentParser(description="SlimeFable Pixel Streaming worker")
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    parser.add_argument("--target", choices=("local", "cloud"), help="Override config target")
    parser.add_argument("--launch", action="store_true", help="Launch packaged seats now")
    args = parser.parse_args()

    cfg = load_config(args.config)
    target, url, signalling_url = resolve_endpoints(cfg, args.target)
    cfg["signalling_url"] = signalling_url
    token = cfg["token"]
    interval = float(cfg.get("heartbeat_seconds", 10))
    seats = cfg.get("seats") or []
    if len(seats) < 1:
        raise SystemExit("worker.config.json needs at least one seat")

    procs: dict[int, subprocess.Popen | None] = {int(s["id"]): None for s in seats}
    should_launch = bool(args.launch or cfg.get("launch_game"))
    if should_launch:
        for seat in seats:
            seat_id = int(seat["id"])
            procs[seat_id] = launch_seat(cfg, seat)
            print(f"launched seat {seat_id} gpu={seat['gpu']} id={seat['streamer_id']}")

    print(f"target={target}  heartbeat -> {url}  streamer -> {signalling_url}  every {interval}s  (Ctrl+C to stop)")
    while True:
        seat_payload = []
        for seat in seats:
            seat_id = int(seat["id"])
            alive = process_alive(procs.get(seat_id))
            if should_launch and not alive:
                try:
                    procs[seat_id] = launch_seat(cfg, seat)
                    alive = True
                    print(f"relaunch seat {seat_id}")
                except Exception as exc:
                    print(f"relaunch seat {seat_id} failed: {exc}")
            seat_payload.append(
                {
                    "id": seat_id,
                    "streamerId": seat["streamer_id"],
                    "online": True,
                    "streaming": alive if should_launch else True,
                    "pid": procs[seat_id].pid if alive else None,
                }
            )
        try:
            post_status(url, token, {"seats": seat_payload})
        except urllib.error.URLError as exc:
            print(f"heartbeat failed: {exc}")
        time.sleep(interval)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(0)
