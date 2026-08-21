# Place SlimeLab devour / phantom test dummies.
# Editor console: py E:/UE/SlimeFable/Tools/Editor/place_slimelab_devour_dummies.py
# Requires the devour C++ flags (bPassive / bHarmless / DebugStartHealthPercent).

import unreal

LEVEL = "/Game/Maps/Sandbox/SlimeLab"
ORIGIN = unreal.Vector(400.0, 0.0, 100.0)
SPACING = 400.0
TAG = "SlimeLabDevourDummy"
TAG_PASSIVE = "SlimeLabPassive"

CANDIDATES = {
    "Watchdog": [
        "/Game/_Slime/Days/08/0815/Y1945/Enemies/BP_0815_Watchdog.BP_0815_Watchdog_C",
        "/Game/_Slime/Days/08/0815/Y1945/Actors/BP_0815_Watchdog.BP_0815_Watchdog_C",
        "/Game/_Slime/Days/08/0815/Enemies/BP_0815_Watchdog.BP_0815_Watchdog_C",
        "/Game/Blueprints/Enemies/BP_0815_Watchdog.BP_0815_Watchdog_C",
    ],
    "Samurai": [
        "/Game/_Slime/Days/08/0815/Y1945/Enemies/BP_0815_Samurai.BP_0815_Samurai_C",
        "/Game/_Slime/Days/08/0815/Y1945/Actors/BP_0815_Samurai.BP_0815_Samurai_C",
        "/Game/_Slime/Days/08/0815/Enemies/BP_0815_Samurai.BP_0815_Samurai_C",
    ],
    "Gunner": [
        "/Game/_Slime/Days/08/0815/Y1945/Enemies/BP_0815_Gunner.BP_0815_Gunner_C",
        "/Game/_Slime/Days/08/0815/Y1945/Actors/BP_0815_Gunner.BP_0815_Gunner_C",
        "/Game/_Slime/Days/08/0815/Enemies/BP_0815_Gunner.BP_0815_Gunner_C",
    ],
    "Foreman": [
        "/Game/_Slime/Days/08/0815/Y1920/Enemies/BP_0815_Foreman.BP_0815_Foreman_C",
        "/Game/_Slime/Days/08/0815/Y1945/Enemies/BP_0815_Foreman.BP_0815_Foreman_C",
        "/Game/_Slime/Days/08/0815/Y1945/Actors/BP_0815_Foreman.BP_0815_Foreman_C",
        "/Game/_Slime/Days/08/0815/Enemies/BP_0815_Foreman.BP_0815_Foreman_C",
    ],
    "Emperor": [
        "/Game/_Slime/Days/08/0815/Y1945/Enemies/BP_0815_Emperor.BP_0815_Emperor_C",
        "/Game/_Slime/Days/08/0815/Y1945/Actors/BP_0815_Emperor.BP_0815_Emperor_C",
        "/Game/_Slime/Days/08/0815/Enemies/BP_0815_Emperor.BP_0815_Emperor_C",
    ],
}

LAYOUT = [
    ("Watchdog", True, 0.08),
    ("Samurai", True, 0.08),
    ("Samurai", True, 0.0),  # Gunner is EnemyTower, cannot be swallowed
    ("Foreman", True, 0.0),
    ("Emperor", True, 0.0),
    ("Watchdog", True, 0.0),
    ("Watchdog", False, 0.0),  # live target for phantom AI
]

SEARCH_ROOTS = [
    "/Game/_Slime/Days/08/0815",
    "/Game/Blueprints",
]
RESULT_LOG = r"E:\UE\SlimeFable\Saved\Logs\place_slimelab_devour_dummies.txt"


def _write_result(lines):
    import os
    os.makedirs(os.path.dirname(RESULT_LOG), exist_ok=True)
    with open(RESULT_LOG, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")


def _find_generated_class(asset_name):
    for root in SEARCH_ROOTS:
        if not unreal.EditorAssetLibrary.does_directory_exist(root):
            continue
        for asset_path in unreal.EditorAssetLibrary.list_assets(root, True, False):
            leaf = asset_path.split(".")[-1] if "." in asset_path else asset_path.rsplit("/", 1)[-1]
            if leaf != asset_name:
                continue
            generated = "{}_C".format(asset_path)
            cls = unreal.load_class(None, generated)
            if cls:
                unreal.log("place_slimelab_devour_dummies: listed {}".format(generated))
                return cls
    return None


def _load_class(key, paths):
    for path in paths:
        cls = unreal.load_class(None, path)
        if cls:
            unreal.log("place_slimelab_devour_dummies: using {}".format(path))
            return cls
    return _find_generated_class("BP_0815_{}".format(key))


def _try_set(actor, names, value):
    if isinstance(names, str):
        names = [names]
    for name in names:
        try:
            actor.set_editor_property(name, value)
            return name
        except Exception:
            continue
    try:
        setattr(actor, names[0], value)
        return "attr:{}".format(names[0])
    except Exception as exc:
        unreal.log_warning("place_slimelab_devour_dummies: failed to set {} = {} ({})".format(names, value, exc))
        return None


def _is_tower(cls):
    tower = getattr(unreal, "EnemyTower", None)
    if tower is None:
        tower = unreal.load_class(None, "/Script/SlimeFable.EnemyTower")
    if tower and hasattr(cls, "is_child_of"):
        try:
            return bool(cls.is_child_of(tower))
        except Exception:
            pass
    return "EnemyTower" in str(cls)


def _configure(actor, passive, start_hp_pct):
    notes = []
    used = _try_set(actor, ["b_passive", "bPassive"], passive)
    notes.append("b_passive via {}".format(used))
    used = _try_set(actor, ["b_harmless", "bHarmless"], True if passive else False)
    notes.append("b_harmless via {}".format(used))
    if passive:
        used = _try_set(actor, ["b_wander_when_idle", "bWanderWhenIdle"], False)
        notes.append("b_wander_when_idle via {}".format(used))
    _try_set(actor, ["b_devourable", "bDevourable"], True)
    _try_set(actor, ["max_hp", "MaxHP"], 200.0)
    used = _try_set(actor, ["debug_start_health_percent", "DebugStartHealthPercent"], start_hp_pct)
    notes.append("debug_hp via {}".format(used))
    tags = [str(t) for t in actor.tags]
    if TAG not in tags:
        actor.tags.append(TAG)
    if passive and TAG_PASSIVE not in tags:
        actor.tags.append(TAG_PASSIVE)
    return notes


def main():
    notes = []
    editor_actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if level_sub:
        level_sub.load_level(LEVEL)

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        msg = "place_slimelab_devour_dummies: no editor world"
        unreal.log_error(msg)
        _write_result([msg])
        return

    for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
        tag_names = [str(t) for t in actor.tags]
        if TAG in tag_names or TAG_PASSIVE in tag_names:
            editor_actor.destroy_actor(actor)

    resolved = {}
    for key, paths in CANDIDATES.items():
        cls = _load_class(key, paths)
        if cls:
            resolved[key] = cls
            notes.append("resolved {} -> {}".format(key, cls.get_name() if hasattr(cls, "get_name") else cls))
        else:
            notes.append("missing BP for {}".format(key))
            unreal.log_warning("place_slimelab_devour_dummies: missing BP for {}".format(key))

    if "Watchdog" not in resolved:
        msg = "place_slimelab_devour_dummies: Watchdog BP missing, abort"
        unreal.log_error(msg)
        notes.append(msg)
        _write_result(notes)
        return

    spawned = 0
    for index, (key, passive, hp_pct) in enumerate(LAYOUT):
        cls = resolved.get(key) or resolved["Watchdog"]
        if _is_tower(cls):
            notes.append("{} is EnemyTower, substituting Watchdog".format(key))
            cls = resolved["Watchdog"]
        loc = ORIGIN + unreal.Vector(0.0, float(index) * SPACING, 0.0)
        actor = editor_actor.spawn_actor_from_class(cls, loc, unreal.Rotator(0.0, 0.0, 0.0))
        if not actor:
            notes.append("failed to spawn {}".format(key))
            unreal.log_warning("place_slimelab_devour_dummies: failed to spawn {}".format(key))
            continue
        cfg = _configure(actor, passive, hp_pct)
        notes.append("spawned {} at {} passive={} hp%={} [{}]".format(
            actor.get_name(), loc, passive, hp_pct, ", ".join(cfg)))
        spawned += 1

    unreal.EditorLevelLibrary.save_current_level()
    done = "place_slimelab_devour_dummies: spawned {} actors in {}".format(spawned, LEVEL)
    unreal.log(done)
    notes.append(done)
    _write_result(notes)


main()
