#!/usr/bin/env python3
"""Generate SlimeFable music/SFX via local ComfyUI Desktop HTTP API.

Examples:
  py generate_audio.py --kind sfx --scope global --id slash_01 --prompt "wet slime slash"
  py generate_audio.py --pack day --day 0815 --import-ue
  py generate_audio.py --kind theme --scope day --day 0815 --id theme --prompt "..." --lyrics "..."
"""

from __future__ import annotations

import argparse
import copy
import json
import os
import random
import subprocess
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
SKILL_DIR = SCRIPT_DIR.parent
WORKFLOWS_DIR = SCRIPT_DIR / "workflows"
CATALOG_PATH = SCRIPT_DIR / "catalog.json"
REPO_ROOT = SKILL_DIR.parents[2]  # .cursor/skills/slimefable-audio -> repo
CONTENT_ROOT = REPO_ROOT / "Content"
DEFAULT_UE_CMD = r"D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
DEFAULT_UPROJECT = str(REPO_ROOT / "SlimeFable.uproject")


def log(msg: str) -> None:
    print(f"[slimefable-audio] {msg}", flush=True)


def load_catalog() -> dict[str, Any]:
    with CATALOG_PATH.open(encoding="utf-8") as f:
        return json.load(f)


def http_json(url: str, data: dict | None = None, timeout: float = 30.0) -> Any:
    body = None
    headers = {"Accept": "application/json"}
    if data is not None:
        body = json.dumps(data).encode("utf-8")
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(url, data=body, headers=headers, method="POST" if data is not None else "GET")
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        raw = resp.read()
        if not raw:
            return None
        return json.loads(raw.decode("utf-8"))


def http_bytes(url: str, timeout: float = 120.0) -> bytes:
    req = urllib.request.Request(url, headers={"Accept": "*/*"})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return resp.read()


def probe_comfy(explicit_url: str | None) -> str:
    candidates: list[str] = []
    if explicit_url:
        candidates.append(explicit_url.rstrip("/"))
    env = os.environ.get("COMFYUI_URL", "").strip()
    if env:
        candidates.append(env.rstrip("/"))
    catalog = load_catalog()
    for u in catalog.get("comfy", {}).get("probe_urls", []):
        candidates.append(str(u).rstrip("/"))
    # de-dupe preserve order
    seen: set[str] = set()
    ordered: list[str] = []
    for u in candidates:
        if u and u not in seen:
            seen.add(u)
            ordered.append(u)

    last_err = None
    for base in ordered:
        try:
            http_json(f"{base}/system_stats", timeout=5.0)
            log(f"ComfyUI ok: {base}")
            return base
        except Exception as exc:  # noqa: BLE001
            last_err = exc
            log(f"probe fail {base}: {exc}")
    raise SystemExit(
        "ComfyUI not reachable. Open ComfyUI Desktop (usually http://127.0.0.1:8000), "
        f"or pass --url. Last error: {last_err}"
    )


def day_month(day_id: str) -> str:
    if len(day_id) != 4 or not day_id.isdigit():
        raise SystemExit(f"Invalid DayId {day_id!r}; expected MMDD")
    return day_id[:2]


def resolve_paths(
    *,
    scope: str,
    day: str | None,
    year: str | None,
    subdir: str,
    asset: str,
) -> tuple[Path, str]:
    """Return (disk_dir, game_dir_without_asset)."""
    if scope == "global":
        game_dir = f"/Game/Audio/{subdir}".replace("\\", "/")
        disk_dir = CONTENT_ROOT / "Audio" / Path(subdir)
    else:
        if not day:
            raise SystemExit("--day required for scope=day")
        mm = day_month(day)
        base = f"/Game/_Slime/Days/{mm}/{day}"
        if year:
            base = f"{base}/Y{year}"
        game_dir = f"{base}/Audio/{subdir}".replace("\\", "/")
        rel = game_dir[len("/Game/") :]
        disk_dir = CONTENT_ROOT / Path(rel.replace("/", os.sep))
    disk_dir.mkdir(parents=True, exist_ok=True)
    return disk_dir, game_dir


def guess_sfx_subdir(asset_id: str, catalog: dict[str, Any]) -> str:
    low = asset_id.lower()
    for cat, keys in catalog.get("sfx_categories", {}).items():
        for k in keys:
            if str(k).lower() in low:
                return f"SFX/{cat}"
    return "SFX/Combat"


def load_api_workflow(name: str) -> dict[str, Any]:
    path = WORKFLOWS_DIR / name
    with path.open(encoding="utf-8") as f:
        data = json.load(f)
    data.pop("_meta", None)
    return data


def build_stable_audio_prompt(
    *,
    text: str,
    duration: float,
    seed: int,
    filename_prefix: str,
    workflow_file: str = "api_stable_audio_3_medium.json",
) -> dict[str, Any]:
    graph = load_api_workflow(workflow_file)
    graph = copy.deepcopy(graph)
    graph["3"]["inputs"]["text"] = text
    graph["5"]["inputs"]["seconds"] = float(duration)
    graph["6"]["inputs"]["seed"] = int(seed)
    graph["8"]["inputs"]["filename_prefix"] = filename_prefix
    return graph


def build_minimax_prompt(
    *,
    caption: str,
    lyrics: str,
    duration: float,
    seed: int,
    filename_prefix: str,
) -> dict[str, Any]:
    graph = load_api_workflow("api_minimax_music_3.json")
    graph = copy.deepcopy(graph)
    graph["4"]["inputs"]["caption"] = caption
    graph["4"]["inputs"]["lyrics"] = lyrics
    graph["4"]["inputs"]["seed"] = int(seed)
    graph["4"]["inputs"]["max_duration"] = float(duration)
    graph["7"]["inputs"]["seed"] = int(seed)
    # If EmptyMiniMaxMusic3LatentAudio rejects linked seconds, fall back to float.
    # Prefer linked seconds from text encode when node supports it.
    graph["9"]["inputs"]["filename_prefix"] = filename_prefix
    return graph


def queue_prompt(base: str, prompt: dict[str, Any]) -> str:
    payload = {"prompt": prompt, "client_id": "slimefable-audio"}
    try:
        result = http_json(f"{base}/prompt", data=payload, timeout=60.0)
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        raise SystemExit(f"Comfy /prompt HTTP {exc.code}: {detail}") from exc
    if not result or "prompt_id" not in result:
        raise SystemExit(f"Unexpected /prompt response: {result}")
    if result.get("node_errors"):
        raise SystemExit(f"Comfy node_errors: {json.dumps(result['node_errors'], ensure_ascii=False)}")
    return str(result["prompt_id"])


def wait_history(base: str, prompt_id: str, timeout_sec: float) -> dict[str, Any]:
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        hist = http_json(f"{base}/history/{prompt_id}", timeout=30.0) or {}
        if prompt_id in hist:
            entry = hist[prompt_id]
            status = entry.get("status") or {}
            if status.get("completed") or entry.get("outputs"):
                if status.get("status_str") == "error":
                    raise SystemExit(f"Comfy job error: {json.dumps(status, ensure_ascii=False)}")
                return entry
            msgs = status.get("messages") or []
            for m in msgs:
                if isinstance(m, list) and m and m[0] == "execution_error":
                    raise SystemExit(f"Comfy execution_error: {m}")
        time.sleep(1.5)
    raise SystemExit(f"Timed out after {timeout_sec}s waiting for prompt {prompt_id}")


def collect_audio_outputs(entry: dict[str, Any]) -> list[dict[str, str]]:
    found: list[dict[str, str]] = []
    outputs = entry.get("outputs") or {}
    for _nid, out in outputs.items():
        for key in ("audio", "audios"):
            items = out.get(key) or []
            for item in items:
                if isinstance(item, dict) and item.get("filename"):
                    found.append(
                        {
                            "filename": item["filename"],
                            "subfolder": item.get("subfolder") or "",
                            "type": item.get("type") or "output",
                        }
                    )
    return found


def download_output(base: str, meta: dict[str, str], dest: Path) -> Path:
    qs = urllib.parse.urlencode(
        {
            "filename": meta["filename"],
            "subfolder": meta.get("subfolder") or "",
            "type": meta.get("type") or "output",
        }
    )
    data = http_bytes(f"{base}/view?{qs}")
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(data)
    return dest


def run_import_ue(paths: list[Path], looping: bool) -> None:
    if not paths:
        return
    import_script = SCRIPT_DIR / "import_audio_to_ue.py"
    payload = {
        "files": [str(p) for p in paths],
        "looping": looping,
    }
    with tempfile.NamedTemporaryFile(
        mode="w", encoding="utf-8", suffix=".json", delete=False
    ) as tmp:
        json.dump(payload, tmp, ensure_ascii=False)
        list_file = tmp.name
    ue_cmd = os.environ.get("SLIMEFABLE_UE_CMD", DEFAULT_UE_CMD)
    uproject = os.environ.get("SLIMEFABLE_UPROJECT", DEFAULT_UPROJECT)
    env = os.environ.copy()
    env["SLIMEFABLE_AUDIO_IMPORT_LIST"] = list_file
    cmd = [
        ue_cmd,
        uproject,
        f"-ExecutePythonScript={import_script.as_posix()}",
        "-unattended",
        "-nop4",
        "-nullrhi",
        "-nosound",
    ]
    log("UE import: " + " ".join(cmd))
    try:
        proc = subprocess.run(cmd, env=env, check=False)
    finally:
        try:
            Path(list_file).unlink(missing_ok=True)
        except OSError:
            pass
    if proc.returncode != 0:
        raise SystemExit(
            f"UE import failed (exit {proc.returncode}). "
            "Close the GUI editor if the project is locked, then retry --import-ue."
        )


def enhance_sfx_prompt(prompt: str, category: str) -> str:
    prefix = {
        "SFX": "Game sound effect, short one-shot, no music, no vocals: ",
        "One-shot": "Short one-shot game SFX, no music: ",
        "Music": "",
    }.get(category, "Game sound effect, short one-shot, no music: ")
    if prompt.lower().startswith("game sound") or "one-shot" in prompt.lower():
        return prompt
    return prefix + prompt


def generate_one(
    *,
    base: str,
    kind: str,
    scope: str,
    day: str | None,
    year: str | None,
    asset: str,
    subdir: str,
    prompt: str,
    lyrics: str | None,
    duration: float,
    seed: int,
    timeout_sec: float,
    import_ue: bool,
) -> dict[str, Any]:
    disk_dir, game_dir = resolve_paths(scope=scope, day=day, year=year, subdir=subdir, asset=asset)
    prefix = f"slimefable/{scope}/{asset}"
    if kind in ("sfx", "bgm"):
        text = prompt
        if kind == "sfx":
            text = enhance_sfx_prompt(prompt, "SFX")
        wf = "api_stable_audio_3_sfx_v2.json" if kind == "sfx" else "api_stable_audio_3_medium.json"
        # Fall back to medium if sfx API file missing
        if kind == "sfx" and not (WORKFLOWS_DIR / wf).is_file():
            wf = "api_stable_audio_3_medium.json"
        graph = build_stable_audio_prompt(
            text=text,
            duration=duration,
            seed=seed,
            filename_prefix=prefix,
            workflow_file=wf,
        )
    elif kind == "theme":
        if not lyrics:
            raise SystemExit("--lyrics required for --kind theme")
        graph = build_minimax_prompt(
            caption=prompt,
            lyrics=lyrics,
            duration=duration,
            seed=seed,
            filename_prefix=prefix,
        )
    else:
        raise SystemExit(f"Unknown kind {kind}")

    log(f"queue {kind} asset={asset} duration={duration}s seed={seed}")
    prompt_id = queue_prompt(base, graph)
    log(f"prompt_id={prompt_id}")
    entry = wait_history(base, prompt_id, timeout_sec)
    outs = collect_audio_outputs(entry)
    if not outs:
        raise SystemExit(f"No audio in outputs: {json.dumps(entry.get('outputs'), ensure_ascii=False)[:800]}")

    saved: list[Path] = []
    for i, meta in enumerate(outs):
        ext = Path(meta["filename"]).suffix or ".mp3"
        name = asset if i == 0 else f"{asset}_{i}"
        dest = disk_dir / f"{name}{ext}"
        download_output(base, meta, dest)
        log(f"saved {dest}")
        saved.append(dest)

    game_paths = [f"{game_dir}/{p.stem}" for p in saved]
    if import_ue:
        run_import_ue(saved, looping=(kind in ("bgm", "theme")))

    return {
        "kind": kind,
        "asset": asset,
        "files": [str(p) for p in saved],
        "game_paths": game_paths,
        "seed": seed,
    }


def find_global_entry(catalog: dict[str, Any], entry_id: str) -> dict[str, Any] | None:
    for e in catalog.get("global_entries", []):
        if e.get("id") == entry_id or e.get("asset") == entry_id:
            return e
    return None


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="SlimeFable ComfyUI audio generator")
    p.add_argument("--url", default=None, help="ComfyUI base URL (skip probe)")
    p.add_argument("--pack", choices=["day"], default=None, help="Generate day pack (explore+combat BGM)")
    p.add_argument("--kind", choices=["sfx", "bgm", "theme"], default=None)
    p.add_argument("--scope", choices=["global", "day"], default="global")
    p.add_argument("--day", default=None, help="MMDD DayId")
    p.add_argument("--year", default=None, help="Optional chapter year for multi-chapter days")
    p.add_argument("--id", dest="asset_id", default=None, help="Logical id / asset stem hint")
    p.add_argument("--prompt", default=None)
    p.add_argument("--lyrics", default=None)
    p.add_argument("--duration", type=float, default=None)
    p.add_argument("--seed", type=int, default=None)
    p.add_argument("--import-ue", action="store_true")
    p.add_argument("--subdir", default=None, help="Override subdir under Audio/")
    return p.parse_args()


def main() -> None:
    args = parse_args()
    catalog = load_catalog()
    defaults = catalog.get("comfy", {}).get("defaults", {})
    base = probe_comfy(args.url)
    seed = args.seed if args.seed is not None else random.randint(0, 2**31 - 1)
    results: list[dict[str, Any]] = []

    if args.pack == "day":
        if not args.day:
            raise SystemExit("--day required for --pack day")
        for item in catalog.get("day_pack", {}).get("items", []):
            kind = item["kind"]
            d = defaults.get(kind, {})
            duration = args.duration if args.duration is not None else float(d.get("duration", 60))
            timeout = float(d.get("timeout_sec", 600))
            asset = str(item["asset_template"]).format(day=args.day)
            prompt = str(item["prompt_template"]).format(day=args.day)
            if args.prompt and item.get("id") == "explore":
                prompt = args.prompt
            results.append(
                generate_one(
                    base=base,
                    kind=kind,
                    scope="day",
                    day=args.day,
                    year=args.year,
                    asset=asset,
                    subdir=item.get("subdir", "BGM"),
                    prompt=prompt,
                    lyrics=None,
                    duration=duration,
                    seed=seed + len(results),
                    timeout_sec=timeout,
                    import_ue=args.import_ue,
                )
            )
    else:
        if not args.kind:
            raise SystemExit("--kind or --pack required")
        kind = args.kind
        d = defaults.get(kind, {})
        duration = args.duration if args.duration is not None else float(d.get("duration", 2 if kind == "sfx" else 60))
        timeout = float(d.get("timeout_sec", 120))
        scope = args.scope
        if scope == "day" and not args.day:
            raise SystemExit("--day required for scope=day")

        entry = find_global_entry(catalog, args.asset_id) if args.asset_id and scope == "global" else None
        prompt = args.prompt or (entry.get("default_prompt") if entry else None)
        if not prompt:
            raise SystemExit("--prompt required (or catalog global id with default_prompt)")

        if entry:
            asset = entry["asset"]
            subdir = entry.get("subdir") or "SFX/Combat"
            kind = entry.get("kind", kind)
        else:
            asset_id = args.asset_id or "clip_01"
            if scope == "global":
                if kind == "sfx":
                    subdir = args.subdir or guess_sfx_subdir(asset_id, catalog)
                    asset = asset_id if asset_id.startswith("sfx_") else f"sfx_{asset_id}"
                elif kind == "bgm":
                    subdir = args.subdir or "BGM"
                    asset = asset_id if asset_id.startswith("bgm_") else f"bgm_global_{asset_id}"
                else:
                    subdir = args.subdir or "Theme"
                    asset = asset_id if asset_id.startswith("theme_") else f"theme_global_{asset_id}"
            else:
                day = args.day or "0000"
                if kind == "sfx":
                    subdir = args.subdir or "SFX"
                    asset = asset_id if asset_id.startswith("sfx_") else f"sfx_{day}_{asset_id}"
                elif kind == "bgm":
                    subdir = args.subdir or "BGM"
                    asset = asset_id if asset_id.startswith("bgm_") else f"bgm_{day}_{asset_id}"
                else:
                    subdir = args.subdir or "Theme"
                    asset = asset_id if asset_id.startswith("theme_") else f"theme_{day}_{asset_id}"

        results.append(
            generate_one(
                base=base,
                kind=kind,
                scope=scope,
                day=args.day,
                year=args.year,
                asset=asset,
                subdir=subdir,
                prompt=prompt,
                lyrics=args.lyrics,
                duration=duration,
                seed=seed,
                timeout_sec=timeout,
                import_ue=args.import_ue,
            )
        )

    print(json.dumps({"ok": True, "results": results}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
