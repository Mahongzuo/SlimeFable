#!/usr/bin/env python3
"""Generate SlimeFable backdrop images via the local ComfyUI HTTP API (Z-Image Turbo + style LoRA).

Examples:
  py Tools/ComfyUI/generate_image.py --preset 1949
  py Tools/ComfyUI/generate_image.py --out Content/Raw/X/sky.png --prompt "..." --width 2048 --height 768

ComfyUI must already be running (Desktop on 8000 or standalone on 8188); pass --url otherwise.
"""

from __future__ import annotations

import argparse
import json
import os
import random
import time
import urllib.parse
import urllib.request
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
PROBE_URLS = ("http://127.0.0.1:8000", "http://127.0.0.1:8188")

STYLE = (
    "hand-painted watercolor storybook illustration, earthy muted palette, soft paper texture, "
    "flat side view panorama for a 2.5D side-scrolling game background, no text, no people faces"
)

PRESETS = {
    "1949": {
        "dir": "Content/Raw/1949Runner/Backdrops",
        "images": (
            ("T_1949_Sky", 2048, 768,
             "early autumn morning sky over old Beijing in 1949, pale gold dawn light, long soft clouds, "
             "a few pigeons far away, empty sky without buildings"),
            ("T_1949_FarCity", 2048, 768,
             "distant silhouette of old Peiping city, endless grey tiled hutong rooftops and courtyard trees, "
             "a faint city gate tower, hazy morning mist, the buildings occupy only the lower half, "
             "isolated on a pure white empty background above the rooftops"),
            ("T_1949_NearCity", 2048, 768,
             "mid-distance old Beijing street in 1949, grey brick courtyard walls, red wooden doors, "
             "red lanterns and red festive banners, locust trees, celebratory mood, the buildings occupy only "
             "the lower half, isolated on a pure white empty background above the rooftops"),
        ),
    },
    "1969": {
        "dir": "Content/Raw/1969Metro/Backdrops",
        "style": (
            "hand-painted watercolor, muted earth pigments, soft paper grain, "
            "no text, no letters, no numbers, no logos, no people"
        ),
        "images": (
            ("T_1969_Blueprint", 1536, 1024,
             "top-down yellowed 1960s civil engineering blueprint of a subway line, "
             "faded indigo ink, stained tracing paper, empty of words"),
            ("T_1969_Tunnel", 1536, 768,
             "looking straight down a 1960s concrete subway tunnel, rounded arch, "
             "a row of warm wall lamps receding into darkness, steel rails, no train"),
        ),
    },
}


def log(message: str) -> None:
    print(f"[slimefable-image] {message}", flush=True)


def http_json(url: str, data: dict | None = None, timeout: float = 30.0):
    body = json.dumps(data).encode("utf-8") if data is not None else None
    headers = {"Accept": "application/json"}
    if body is not None:
        headers["Content-Type"] = "application/json"
    request = urllib.request.Request(url, data=body, headers=headers, method="POST" if body else "GET")
    with urllib.request.urlopen(request, timeout=timeout) as response:
        raw = response.read()
        return json.loads(raw.decode("utf-8")) if raw else None


def probe(explicit: str | None) -> str:
    candidates = [u for u in (explicit, os.environ.get("COMFYUI_URL"), *PROBE_URLS) if u]
    for base in candidates:
        base = base.rstrip("/")
        try:
            http_json(f"{base}/system_stats", timeout=5.0)
            log(f"ComfyUI ok: {base}")
            return base
        except Exception as exc:  # noqa: BLE001
            log(f"probe fail {base}: {exc}")
    raise SystemExit("ComfyUI not reachable. Start ComfyUI Desktop or run_comfy_gpu0.bat, or pass --url.")


def build_graph(prompt: str, width: int, height: int, seed: int, lora: str, lora_strength: float, prefix: str) -> dict:
    return {
        "1": {"class_type": "UNETLoader", "inputs": {"unet_name": "z_image_turbo_bf16.safetensors", "weight_dtype": "default"}},
        "2": {"class_type": "CLIPLoader", "inputs": {"clip_name": "qwen_3_4b.safetensors", "type": "lumina2", "device": "default"}},
        "3": {"class_type": "VAELoader", "inputs": {"vae_name": "ae.safetensors"}},
        "4": {"class_type": "LoraLoaderModelOnly", "inputs": {"model": ["1", 0], "lora_name": lora, "strength_model": lora_strength}},
        "5": {"class_type": "ModelSamplingAuraFlow", "inputs": {"model": ["4", 0], "shift": 3.0}},
        "6": {"class_type": "CLIPTextEncode", "inputs": {"clip": ["2", 0], "text": prompt}},
        "7": {"class_type": "ConditioningZeroOut", "inputs": {"conditioning": ["6", 0]}},
        "8": {"class_type": "EmptySD3LatentImage", "inputs": {"width": width, "height": height, "batch_size": 1}},
        "9": {"class_type": "KSampler", "inputs": {
            "model": ["5", 0], "positive": ["6", 0], "negative": ["7", 0], "latent_image": ["8", 0],
            "seed": seed, "steps": 9, "cfg": 1.0, "sampler_name": "res_multistep", "scheduler": "simple", "denoise": 1.0}},
        "10": {"class_type": "VAEDecode", "inputs": {"samples": ["9", 0], "vae": ["3", 0]}},
        "11": {"class_type": "SaveImage", "inputs": {"images": ["10", 0], "filename_prefix": prefix}},
    }


def run(base: str, graph: dict, out_path: Path, timeout: float) -> None:
    queued = http_json(f"{base}/prompt", {"prompt": graph, "client_id": "slimefable-image"})
    prompt_id = queued["prompt_id"]
    deadline = time.time() + timeout
    while time.time() < deadline:
        history = http_json(f"{base}/history/{prompt_id}") or {}
        entry = history.get(prompt_id)
        if entry:
            status = entry.get("status", {})
            if status.get("status_str") == "error":
                raise SystemExit(f"ComfyUI error: {json.dumps(status, ensure_ascii=False)[:2000]}")
            for node in entry.get("outputs", {}).values():
                for image in node.get("images", []):
                    query = urllib.parse.urlencode({
                        "filename": image["filename"], "subfolder": image.get("subfolder", ""), "type": image.get("type", "output")})
                    with urllib.request.urlopen(f"{base}/view?{query}", timeout=120) as response:
                        out_path.parent.mkdir(parents=True, exist_ok=True)
                        out_path.write_bytes(response.read())
                    log(f"saved {out_path}")
                    return
        time.sleep(1.5)
    raise SystemExit(f"Timed out waiting for {prompt_id}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--preset", choices=sorted(PRESETS))
    parser.add_argument("--prompt")
    parser.add_argument("--out", help="PNG path relative to repo root (single image mode)")
    parser.add_argument("--width", type=int, default=2048)
    parser.add_argument("--height", type=int, default=768)
    parser.add_argument("--seed", type=int)
    parser.add_argument("--lora", default="ZIT-Watercolor.safetensors")
    parser.add_argument("--lora-strength", type=float, default=0.8)
    parser.add_argument("--url")
    parser.add_argument("--timeout", type=float, default=300.0)
    parser.add_argument("--skip-existing", action="store_true")
    args = parser.parse_args()

    base = probe(args.url)
    jobs = []
    if args.preset:
        preset = PRESETS[args.preset]
        style = preset.get("style", STYLE)
        for name, width, height, prompt in preset["images"]:
            jobs.append((REPO_ROOT / preset["dir"] / f"{name}.png", width, height, prompt, style))
    elif args.prompt and args.out:
        jobs.append((REPO_ROOT / args.out, args.width, args.height, args.prompt, STYLE))
    else:
        raise SystemExit("Pass --preset, or --prompt with --out")

    for out_path, width, height, prompt, style in jobs:
        if args.skip_existing and out_path.exists():
            log(f"skip existing {out_path}")
            continue
        seed = args.seed if args.seed is not None else random.randint(1, 2**31 - 1)
        graph = build_graph(f"{prompt}, {style}", width, height, seed, args.lora, args.lora_strength, f"SlimeFable/{out_path.stem}")
        log(f"generating {out_path.name} {width}x{height} seed={seed}")
        run(base, graph, out_path, args.timeout)


if __name__ == "__main__":
    main()
