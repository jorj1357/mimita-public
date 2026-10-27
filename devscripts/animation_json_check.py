"""Validate authored clips, avatar assignments, and weapon action references."""

import json
import math
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIBRARY = ROOT / "config" / "animations" / "library"
MASTER = ROOT / "config" / "animations" / "masterlist" / "animation-masterlist.json"
SETS = ROOT / "config" / "animations" / "masterlist" / "animation-sets.json"
SETTINGS = ROOT / "config" / "animations" / "settings.json"
IMPORTS = ROOT / "config" / "animations" / "imports"


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def load_include_clips(include, errors, owner):
    target = ROOT / include
    if include.endswith("/") and target.is_dir():
        clips = set()
        for path in sorted(target.glob("*.json")):
            clips.update(read(path).get("clips", {}))
        return clips
    if not target.is_file():
        errors.append(f"{owner}: missing include {include}")
        return set()
    return set(read(target).get("clips", {}))


def state_clip_ids(states):
    selected = [value for key, value in states.items()
                if key != "locomotion" and isinstance(value, str)]
    selected += list(states.get("locomotion", {}).values())
    return selected


def main():
    master = read(MASTER)
    errors = []
    ids = defaultdict(list)
    checked = 0
    profile_count = 0
    for path in sorted(LIBRARY.rglob("*.json")):
        document = read(path)
        if document.get("version") not in (1, master["clip_format_version"]):
            errors.append(f"{path}: wrong format version")
        for clip_id, clip in document.get("clips", {}).items():
            checked += 1
            ids[clip_id].append(path)
            duration = clip.get("durationTicks")
            if not isinstance(duration, int) or isinstance(duration, bool) or duration <= 0:
                errors.append(f"{path}: {clip_id} has invalid durationTicks")
                continue
            if not isinstance(clip.get("loop", True), bool) or not isinstance(clip.get("aimLayer", True), bool):
                errors.append(f"{path}: {clip_id} has invalid loop or aimLayer")
            for keyframe in clip.get("keyframes", []):
                tick = keyframe.get("tick")
                if not isinstance(tick, int) or tick < 0 or tick > duration:
                    errors.append(f"{path}: {clip_id} has invalid keyframe tick")
                for part, channels in keyframe.get("parts", {}).items():
                    if "rotationEulerDegrees" in channels and "rotationQuaternionWxyz" in channels:
                        errors.append(f"{path}: {clip_id}/{part} has two rotation encodings")
                    for field in ("translation", "rotationEulerDegrees", "rotationQuaternionWxyz", "scale"):
                        if field in channels and not all(isinstance(v, (int, float)) and math.isfinite(v)
                                                         for v in channels[field]):
                            errors.append(f"{path}: {clip_id}/{part}/{field} has non-finite values")
            markers = {marker.get("name"): marker.get("tick") for marker in clip.get("markers", [])}
            if "phase.windup" in markers:
                names = ("phase.windup", "phase.active", "phase.recovery", "action.end")
                try:
                    ticks = [markers[name] for name in names]
                    if ticks[0] != 0 or ticks[-1] != duration or not all(a < b for a, b in zip(ticks, ticks[1:])):
                        raise ValueError()
                except (KeyError, TypeError, ValueError):
                    errors.append(f"{path}: {clip_id} has invalid Melee markers")

    sets = read(SETS).get("sets", {}) if SETS.is_file() else {}
    for set_id, set_doc in sets.items():
        available = set(set_doc.get("clips", {}))
        for include in set_doc.get("include", []):
            available.update(load_include_clips(include, errors, f"set {set_id}"))
        for clip_id in state_clip_ids(set_doc.get("states", {})):
            if clip_id and clip_id not in available:
                errors.append(f"set {set_id}: state references missing clip {clip_id}")

    for path in sorted((ROOT / "assets" / "avatars").glob("*/avatar.json")):
        profile = read(path).get("animation_profile")
        if not isinstance(profile, dict):
            continue
        profile_count += 1
        available = set(profile.get("clips", {}))
        set_id = profile.get("set")
        if set_id:
            if set_id not in sets:
                errors.append(f"{path}: missing animation set {set_id}")
            else:
                set_doc = sets[set_id]
                available.update(set_doc.get("clips", {}))
                for include in set_doc.get("include", []):
                    available.update(load_include_clips(include, errors, f"set {set_id}"))
        for include in profile.get("include", []):
            available.update(load_include_clips(include, errors, str(path)))
        for clip_id in state_clip_ids(profile.get("states", {})):
            if clip_id and clip_id not in available:
                errors.append(f"{path}: state references missing clip {clip_id}")

    if SETTINGS.is_file():
        settings = read(SETTINGS)
        if settings.get("rootTranslationMode") not in (1, 2):
            errors.append(f"{SETTINGS}: rootTranslationMode must be 1 or 2")
        if not isinstance(settings.get("renderPoseInterpolation", True), bool):
            errors.append(f"{SETTINGS}: renderPoseInterpolation must be boolean")
        priorities = settings.get("statePriorities", {})
        if not isinstance(priorities, dict) or not all(
                isinstance(v, int) and not isinstance(v, bool) for v in priorities.values()):
            errors.append(f"{SETTINGS}: statePriorities must be integer values")

    for path in sorted((ROOT / "config" / "weapons").glob("*/config.json")):
        definition = read(path)
        components = definition.get("components") or {}
        melee = components.get("melee") or {}
        for action_id, action in melee.get("actions", {}).items():
            clip_id = action.get("animation_clip_id")
            if clip_id and len(ids[clip_id]) != 1:
                errors.append(f"{path}: {action_id} clip {clip_id} resolves {len(ids[clip_id])} times")

    for manifest_path in sorted(IMPORTS.glob("*.json")):
        manifest = read(manifest_path)
        output_paths = set()
        clip_ids = set()
        for entry in manifest.get("entries", []):
            if entry.get("status") != "ready":
                continue
            output = ROOT / entry["output"]
            clip_id = entry["clip_id"]
            if output in output_paths or clip_id in clip_ids:
                errors.append(f"{manifest_path}: duplicate ready output or clip ID {clip_id}")
            output_paths.add(output)
            clip_ids.add(clip_id)
            if not output.is_file():
                errors.append(f"{manifest_path}: missing exported clip {entry['output']}")
                continue
            exported = read(output)
            if list(exported.get("clips", {})) != [clip_id]:
                errors.append(f"{output}: does not contain the selected clip {clip_id}")
                continue
            clip = exported["clips"][clip_id]
            if clip.get("loop") != entry.get("loop"):
                errors.append(f"{output}: loop differs from import manifest")
            if entry.get("clip_type") == "melee" and "phase.active" not in {
                    marker.get("name") for marker in clip.get("markers", [])}:
                errors.append(f"{output}: selected Melee clip has no active marker")

    print(f"clips={checked} avatar_profiles_checked={profile_count} errors={len(errors)}")
    for error in errors:
        print(error)
    return not errors


if __name__ == "__main__":
    raise SystemExit(0 if main() else 1)
