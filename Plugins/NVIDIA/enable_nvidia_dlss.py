# Enable NVIDIA DLSS / Streamline plugins dropped into this folder
# and apply the UE 5.8 DepthInverted ghosting hotfix if plugin source is present.
# Download the official UE 5.8 DLSS 4.5 pack from https://developer.nvidia.com/rtx/dlss
# and extract it here (DLSS, Streamline, NGX, ...).

from __future__ import annotations

import json
from pathlib import Path

PLUGIN_ROOT = Path(__file__).resolve().parent
ROOT = PLUGIN_ROOT.parents[1]
UPROJECT = ROOT / "SlimeFable.uproject"
DEPTH_OLD = "DLSSFeatureFlags = NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;"
DEPTH_NEW = "DLSSFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;"

# Runtime SR + FG + Reflex. Skip deprecated Streamline umbrella (pulls DeepDVC),
# NIS, DeepDVC, MoviePipeline, samples.
ENABLE_PLUGINS = (
    "StreamlineNGXCommon",
    "StreamlineCore",
    "StreamlineReflex",
    "StreamlineDLSSG",
    "DLSS",
)


def find_uplugins() -> list[Path]:
    if not PLUGIN_ROOT.is_dir():
        return []
    return sorted(p for p in PLUGIN_ROOT.rglob("*.uplugin") if p.name != "enable_nvidia_dlss.py")


def module_plugin_name(path: Path) -> str:
    return path.stem


def enable_in_uproject(names: list[str]) -> None:
    data = json.loads(UPROJECT.read_text(encoding="utf-8-sig"))
    plugins = data.setdefault("Plugins", [])
    existing = {p.get("Name"): p for p in plugins if isinstance(p, dict)}
    changed = False
    for name in names:
        if name in existing:
            if not existing[name].get("Enabled", False):
                existing[name]["Enabled"] = True
                changed = True
        else:
            plugins.append({"Name": name, "Enabled": True})
            changed = True
    if changed:
        UPROJECT.write_text(json.dumps(data, indent="\t") + "\n", encoding="utf-8")
        print(f"Updated {UPROJECT.name} with: {', '.join(names)}")
    else:
        print("uproject already lists the NVIDIA plugins as enabled.")


def patch_ghosting() -> int:
    patched = 0
    for cpp in PLUGIN_ROOT.rglob("NGXRHI.cpp"):
        text = cpp.read_text(encoding="utf-8", errors="replace")
        if DEPTH_NEW in text:
            print(f"Already patched: {cpp}")
            continue
        if DEPTH_OLD not in text:
            print(f"No DepthInverted assign in {cpp}")
            continue
        cpp.write_text(text.replace(DEPTH_OLD, DEPTH_NEW, 1), encoding="utf-8")
        print(f"Patched DepthInverted |= in {cpp}")
        patched += 1
    return patched


def main() -> int:
    uplugins = find_uplugins()
    if not uplugins:
        print("No .uplugin under Plugins/NVIDIA yet.")
        print("Extract the official UE 5.8 DLSS pack there, then re-run this script.")
        return 1
    found = {module_plugin_name(p) for p in uplugins}
    print("Found plugins: " + ", ".join(sorted(found)))
    names = [name for name in ENABLE_PLUGINS if name in found]
    missing = [name for name in ENABLE_PLUGINS if name not in found]
    if missing:
        print("Missing expected plugins: " + ", ".join(missing))
        return 1
    enable_in_uproject(names)
    patch_ghosting()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
