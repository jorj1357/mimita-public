#!/usr/bin/env python3
# 09 17 2026
# purpose
# Offline Blender -> MiMITA exact-pose animation exporter.
#
# Two modes:
#   * Blender mode (`--blender`): samples an armature/empties at exactly 60
#     samples/second, bakes each body-part transform actor-relative to the
#     `plrOrigin` empty, converts Blender coordinates to MiMITA coordinates, and
#     writes a spec. Requires `bpy` (run with `blender --background --python`).
#   * Standalone mode (`--spec`): expands a sparse key spec into one pose per
#     fixed simulation tick and emits BOTH the canonical JSON animation asset
#     and the generated hot C++ header the runtime registers.
#
# The runtime never parses JSON: the generated header is compiled into the hot
# DLL, so editing a clip and rebuilding hot-reloads it in the same EXE/world.
# The exporter is an authoring tool; it does not own gameplay or damage.
#
# Coordinate contract: MiMITA is Z-up (vertical axis index 2) like Blender.
# `blender_to_mimita` is the single conversion owner; it is identity until the
# rest-pose check proves a forward-axis difference.
#
# Does NOT link into the EXE and does not modify gameplay source.

import argparse
import json
import math
import os
import re
import sys

PART_ORDER = ["torso", "head", "leftArm", "rightArm", "leftLeg", "rightLeg"]
SAMPLE_RATE = 60


def blender_to_mimita(v):
    """Single coordinate-conversion owner (Blender -> MiMITA)."""
    return [float(v[0]), float(v[1]), float(v[2])]


def symbol_name(animation_id):
    safe = re.sub(r"[^0-9A-Za-z]+", "_", animation_id).strip("_")
    return safe


def lerp(a, b, w):
    return a + (b - a) * w


def expand_keys(spec):
    """Expand sparse keys into exactly one 6-float pose per tick.

    Returns (frames, mask) where frames[t] is a dict part -> [tx,ty,tz,rx,ry,rz].
    """
    duration = int(spec["duration_ticks"])
    keys = sorted(spec.get("keys", []), key=lambda k: int(k["tick"]))
    if not keys:
        raise ValueError("spec has no keys")
    for k in keys:
        if int(k["tick"]) < 0 or int(k["tick"]) >= duration:
            raise ValueError("key tick out of range: %s" % k["tick"])

    mask = 0
    for part in PART_ORDER:
        for k in keys:
            if part in k.get("pose", {}):
                mask |= 1 << PART_ORDER.index(part)
                break

    frames = []
    for tick in range(duration):
        if tick <= int(keys[0]["tick"]):
            frame = keys[0].get("pose", {})
        elif tick >= int(keys[-1]["tick"]):
            frame = keys[-1].get("pose", {})
        else:
            lo = keys[0]
            hi = keys[-1]
            for i in range(1, len(keys)):
                if int(keys[i]["tick"]) >= tick:
                    lo, hi = keys[i - 1], keys[i]
                    break
            span = int(hi["tick"]) - int(lo["tick"])
            w = 0.0 if span <= 0 else (tick - int(lo["tick"])) / float(span)
            frame = {}
            for part in PART_ORDER:
                a = lo.get("pose", {}).get(part)
                b = hi.get("pose", {}).get(part)
                if a is None and b is None:
                    continue
                if a is None:
                    a = [0.0] * 6
                if b is None:
                    b = [0.0] * 6
                frame[part] = [lerp(a[i], b[i], w) for i in range(6)]
        full = {}
        for part in PART_ORDER:
            full[part] = [float(x) for x in frame.get(part, [0.0] * 6)]
        frames.append(full)
    return frames, mask


def frames_to_cpp(frames):
    rows = []
    for tick, frame in enumerate(frames):
        parts = []
        for part in PART_ORDER:
            v = frame[part]
            parts.append("{" + ", ".join("%.4ff" % x for x in v) + "}")
        # The comment goes on its own line so the join comma stays valid C++.
        rows.append("    // tick %d\n    {%.1ff, {%s}}" % (tick, float(tick),
                                                          ", ".join(parts)))
    return ",\n".join(rows)


def markers_to_cpp(markers):
    if not markers:
        return None
    rows = []
    for m in markers:
        off = blender_to_mimita(m.get("offset", [0.0, 0.0, 0.0]))
        flags = 1 if m.get("weapon") else 0
        rows.append(
            '    {gameHash("%s"), gameHash("%s"), {%.4ff, %.4ff, %.4ff}, %.4ff, %d, 0}'
            % (m["part"], m["name"], off[0], off[1], off[2],
               float(m.get("radius", 0.1)), flags))
    return ",\n".join(rows)


def emit_header(spec, frames, mask, header_path):
    animation_id = spec["animation_id"]
    name = symbol_name(animation_id)
    duration = len(frames)
    loop = 1 if int(spec.get("loop", 0)) else 0
    markers_cpp = markers_to_cpp(spec.get("markers", []))
    marker_count = len(spec.get("markers", []))
    marker_array = ""
    marker_ptr = "nullptr"
    if markers_cpp is not None:
        marker_array = (
            "inline constexpr HotAnim::BlenderMarker k%sMarkers[] = {\n%s\n};\n"
            % (name, markers_cpp))
        marker_ptr = "k%sMarkers" % name

    text = """// GENERATED FILE - do not edit by hand.
// Source: tools/blender_export_animation.py --spec <spec> for %s
// One exact pose per fixed simulation tick; actor-relative; degrees.
#pragma once

#include "hot-reload/hot-animation-blender.h"

namespace HotAnimGenerated {

inline constexpr HotAnim::Keyframe k%sFrames[] = {
%s
};

%s
inline const HotAnim::BlenderClip k%sClip{
    gameHash("%s"),
    %d,   // sample rate
    %d,   // duration ticks
    %du,  // mask
    %du,  // frame count
    k%sFrames,
    %s,
    %du,
    %du};

inline const bool k%sRegistered = HotAnim::registerBlenderClip(k%sClip);

} // namespace HotAnimGenerated
""" % (animation_id, name, frames_to_cpp(frames), marker_array, name, animation_id,
       SAMPLE_RATE, duration, mask, duration, name, marker_ptr, marker_count, loop,
       name, name)

    os.makedirs(os.path.dirname(os.path.abspath(header_path)), exist_ok=True)
    with open(header_path, "w", encoding="utf-8") as handle:
        handle.write(text)


def emit_json_asset(spec, frames, mask, json_path):
    asset = {
        "version": 1,
        "animation_id": spec["animation_id"],
        "sample_rate": SAMPLE_RATE,
        "duration_ticks": len(frames),
        "loop": 1 if int(spec.get("loop", 0)) else 0,
        "origin": spec.get("origin", "plrOrigin"),
        "mask": mask,
        "parts": {part: [] for part in PART_ORDER},
        "markers": spec.get("markers", []),
    }
    for tick, frame in enumerate(frames):
        for part in PART_ORDER:
            asset["parts"][part].append(
                {"tick": tick, "pose": [round(float(x), 6) for x in frame[part]]})

    os.makedirs(os.path.dirname(os.path.abspath(json_path)), exist_ok=True)
    with open(json_path, "w", encoding="utf-8") as handle:
        json.dump(asset, handle, indent=2)
        handle.write("\n")


def run_standalone(spec_path, header_path, json_path):
    with open(spec_path, "r", encoding="utf-8") as handle:
        spec = json.load(handle)
    frames, mask = expand_keys(spec)
    emit_header(spec, frames, mask, header_path)
    emit_json_asset(spec, frames, mask, json_path)
    print("[EXPORT] %s -> %s (%d ticks)" % (spec["animation_id"], header_path, len(frames)))
    print("[EXPORT] %s -> %s" % (spec["animation_id"], json_path))


# ── Blender mode ────────────────────────────────────────────────────
def sample_blender(animation_id, action_name, origin_name, part_map, loop,
                   header_path, json_path):
    """Sample the active armature at 60 Hz and emit a spec, then expand it.

    `part_map` maps a MiMITA part name to a Blender bone/object name. Transform
    of each part is made relative to the `origin_name` object (plrOrigin), so all
    exported transforms are actor-relative. Run inside Blender:
      blender --background file.blend --python tools/blender_export_animation.py -- \
          --blender --action slash --animation-id animation.katana_slash \
          --out-header src/hot-reload/generated/hot-anim-katana-slash.h \
          --out-json assets/animations/animation.katana_slash.json
    """
    import bpy  # noqa: E402  (only available inside Blender)

    scene = bpy.context.scene
    fps = int(round(scene.render.fps / max(scene.render.fps_base, 1e-6)))
    if fps != SAMPLE_RATE:
        raise SystemExit(
            "[EXPORT] scene fps is %d; set the scene to 60 fps for exact ticks" % fps)

    origin = bpy.data.objects[origin_name]
    armature = None
    for obj in bpy.data.objects:
        if obj.type == "ARMATURE":
            armature = obj
            break
    if armature is None:
        raise SystemExit("[EXPORT] no armature found")

    action = bpy.data.actions[action_name]
    frame_start = int(action.frame_range[0])
    frame_end = int(action.frame_range[1])
    duration = max(1, frame_end - frame_start + 1)

    origin_inv = origin.matrix_world.inverted()
    keys = []
    for tick in range(duration):
        frame = frame_start + tick
        scene.frame_set(frame)
        pose = {}
        for part, bone_name in part_map.items():
            bone = armature.pose.bones[bone_name]
            local = origin_inv @ armature.matrix_world @ bone.matrix
            loc = blender_to_mimita(local.to_translation())
            rot = [math.degrees(a) for a in local.to_euler("XYZ")]
            pose[part] = [loc[0], loc[1], loc[2], rot[0], rot[1], rot[2]]
        keys.append({"tick": tick, "pose": pose})

    spec = {
        "animation_id": animation_id,
        "sample_rate": SAMPLE_RATE,
        "duration_ticks": duration,
        "loop": loop,
        "origin": origin_name,
        "keys": keys,
        "markers": [],
    }
    frames, mask = expand_keys(spec)
    emit_header(spec, frames, mask, header_path)
    emit_json_asset(spec, frames, mask, json_path)
    print("[EXPORT] sampled %d ticks from action %s" % (duration, action_name))


def parse_args(argv):
    parser = argparse.ArgumentParser(description="MiMITA exact-pose animation exporter")
    parser.add_argument("--spec")
    parser.add_argument("--out-header", required=False)
    parser.add_argument("--out-json", required=False)
    parser.add_argument("--blender", action="store_true")
    parser.add_argument("--animation-id", default="animation.custom")
    parser.add_argument("--action", default=None)
    parser.add_argument("--origin", default="plrOrigin")
    parser.add_argument("--loop", type=int, default=0)
    parser.add_argument("--part-map", default=None,
                        help="comma list part=bone, e.g. torso=spine,head=head")
    return parser.parse_args(argv)


def main():
    args = parse_args(sys.argv[1:])
    if args.blender:
        if not args.action or not args.out_header or not args.out_json:
            raise SystemExit("--blender requires --action --out-header --out-json")
        part_map = {part: part for part in PART_ORDER}
        if args.part_map:
            part_map = dict(kv.split("=", 1) for kv in args.part_map.split(","))
        sample_blender(args.animation_id, args.action, args.origin, part_map,
                       args.loop, args.out_header, args.out_json)
        return 0
    if not args.spec or not args.out_header or not args.out_json:
        raise SystemExit("standalone mode requires --spec --out-header --out-json")
    run_standalone(args.spec, args.out_header, args.out_json)
    return 0


if __name__ == "__main__":
    sys.exit(main())
