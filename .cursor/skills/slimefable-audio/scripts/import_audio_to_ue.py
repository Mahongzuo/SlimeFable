#!/usr/bin/env python3
"""Import generated audio files into Unreal as SoundWave assets.

Invoked by generate_audio.py via UnrealEditor-Cmd -ExecutePythonScript.
Reads SLIMEFABLE_AUDIO_IMPORT_LIST JSON: {"files": [...], "looping": bool}
"""

from __future__ import annotations

import json
import os
from pathlib import Path

try:
    import unreal  # type: ignore
except ImportError:
    unreal = None


PREFIX = "[slimefable-audio-import]"


def report(msg: str) -> None:
    if unreal is not None:
        unreal.log(f"{PREFIX} {msg}")
    else:
        print(f"{PREFIX} {msg}")


def content_root() -> Path:
    if unreal is not None:
        return Path(unreal.Paths.project_content_dir())
    # Fallback when run outside editor (should not import)
    return Path(__file__).resolve().parents[4] / "Content"


def disk_to_game_dir(disk_file: Path) -> tuple[str, str]:
    """Return (/Game/.../parent, asset_name)."""
    content = content_root().resolve()
    resolved = disk_file.resolve()
    try:
        rel = resolved.relative_to(content)
    except ValueError as exc:
        raise RuntimeError(f"{PREFIX} file not under Content: {resolved}") from exc
    parts = list(rel.parts)
    if not parts:
        raise RuntimeError(f"{PREFIX} empty relative path")
    asset_name = Path(parts[-1]).stem
    parent = parts[:-1]
    game_dir = "/Game" if not parent else "/Game/" + "/".join(parent)
    return game_dir.replace("\\", "/"), asset_name


def ensure_dir(game_dir: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(game_dir):
        unreal.EditorAssetLibrary.make_directory(game_dir)


def import_one(path: Path, looping: bool, trim_seconds: float | None = None) -> str:
    game_dir, asset_name = disk_to_game_dir(path)
    ensure_dir(game_dir)
    destination = f"{game_dir}/{asset_name}"

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(path))
    task.set_editor_property("destination_path", game_dir)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset = unreal.EditorAssetLibrary.load_asset(destination)
    if asset is None:
        asset = unreal.EditorAssetLibrary.load_asset(f"{destination}.{asset_name}")
    if asset is None:
        raise RuntimeError(f"{PREFIX} import failed for {path} -> {destination}")

    if isinstance(asset, unreal.SoundWave):
        # UE5 SoundWave looping flag
        for prop in ("looping", "b_looping", "bLooping"):
            try:
                asset.set_editor_property(prop, bool(looping))
                break
            except Exception:  # noqa: BLE001
                continue
        if trim_seconds and trim_seconds > 0.05:
            # Soft trim: clamp playback duration used by some systems; full wave remains.
            try:
                duration = float(asset.get_editor_property("duration"))
                if duration > trim_seconds:
                    # SoundWave has no public trim API in all builds; log intent.
                    report(f"note trim desired={trim_seconds:.2f}s actual={duration:.2f}s {destination}")
            except Exception:  # noqa: BLE001
                pass
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
        report(f"imported looping={looping} {destination}")
    else:
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
        report(f"imported {destination}")

    return destination


def main() -> None:
    if unreal is None:
        raise SystemExit(f"{PREFIX} must run inside UnrealEditor-Cmd")

    list_path = os.environ.get("SLIMEFABLE_AUDIO_IMPORT_LIST", "")
    if not list_path or not Path(list_path).is_file():
        raise SystemExit(f"{PREFIX} missing SLIMEFABLE_AUDIO_IMPORT_LIST")

    payload = json.loads(Path(list_path).read_text(encoding="utf-8"))
    files = [Path(p) for p in payload.get("files", [])]
    looping = bool(payload.get("looping", False))
    trim_seconds = payload.get("trim_seconds")
    trim_val = float(trim_seconds) if trim_seconds is not None else None
    # Optional per-file: [{"path": "...", "looping": false, "trim_seconds": 0.35}]
    entries = payload.get("entries")

    imported = []
    if entries:
        for entry in entries:
            f = Path(entry["path"])
            if not f.is_file():
                report(f"skip missing {f}")
                continue
            imported.append(
                import_one(
                    f,
                    looping=bool(entry.get("looping", False)),
                    trim_seconds=(
                        float(entry["trim_seconds"]) if entry.get("trim_seconds") is not None else None
                    ),
                )
            )
    else:
        for f in files:
            if not f.is_file():
                report(f"skip missing {f}")
                continue
            imported.append(import_one(f, looping=looping, trim_seconds=trim_val))

    report(f"done count={len(imported)} -> {imported}")


if __name__ == "__main__":
    main()
