#!/usr/bin/env python3
"""Generate the Rocket League reference geometry record and arena2 from the
measured collision mesh.

Input  : assets/maps/rl/pitch.obj  (standard Soccar collision mesh, UU, z up)
Output : config/reference/arena2-geometry.json  (measured values + provenance)
         config/maps/arena2.json                (authored MiMITA map)

Every number is labelled DOCUMENTED, MEASURED, DERIVED or ESTIMATED. The map is
derived, so it can be regenerated; the mesh itself is never shipped or loaded by
the game.
"""

from __future__ import annotations

import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MESH = ROOT / "assets" / "maps" / "rl" / "pitch.obj"
REFERENCE = ROOT / "config" / "reference" / "arena2-geometry.json"
MAP = ROOT / "config" / "maps" / "arena2.json"
GLB = ROOT / "assets" / "maps" / "arena2.glb"

UU = 100.0  # 100 unreal units = 1 m

# Documented values (RLBot / RocketSim / RLGym tables). Kept for cross-checks.
DOCUMENTED = {
    "width_m": 81.92,
    "length_m": 102.40,
    "ceiling_m": 20.48,
    "goal_width_m": 17.8551,
    "goal_height_m": 6.42775,
    "goal_depth_m": 8.80,
    "corner_plane_m": 80.64,
    "goal_back_m": 60.00,
}


def load_mesh(path: Path):
    verts = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("v "):
            _, x, y, z = line.split()[:4]
            verts.append((float(x), float(y), float(z)))
    return verts


def profile(verts, pick, to_uv):
    """Unique sorted cross-section points from a vertex filter."""
    seen = {}
    for v in verts:
        if not pick(v):
            continue
        u, w = to_uv(v)
        # 1 mm buckets: the mesh repeats each cross-section vertex per segment.
        seen.setdefault((round(u / UU, 3), round(w / UU, 3)), None)
    return sorted(seen)


def write_glb(half_length: float, half_width: float, half_goal_back: float) -> None:
    """Minimal valid base GLB. The client refuses a map with no base file; all
    real arena2 geometry is authored in the map JSON, so this is one hidden
    floor quad below the play surface plus the two world spawn anchors."""
    import struct

    xs, ys, zs = half_goal_back, half_width, -0.5
    positions = [
        (-xs, -ys, zs), (xs, -ys, zs), (xs, ys, zs), (-xs, ys, zs),
    ]
    normals = [(0.0, 0.0, 1.0)] * 4
    indices = [0, 1, 2, 0, 2, 3]
    pos_bytes = b"".join(struct.pack("<3f", *p) for p in positions)
    nrm_bytes = b"".join(struct.pack("<3f", *n) for n in normals)
    idx_bytes = b"".join(struct.pack("<H", i) for i in indices)
    binary = pos_bytes + nrm_bytes + idx_bytes
    doc = {
        "asset": {"version": "2.0", "generator": "MiMITA arena2 builder"},
        "scene": 0,
        "scenes": [{"nodes": [0, 1, 2]}],
        "nodes": [
            {"mesh": 0, "name": "arena2_base_floor"},
            {"name": "spawn_blue", "translation": [-20.0, 0.0, 1.0]},
            {"name": "spawn_orange", "translation": [20.0, 0.0, 1.0]},
        ],
        "meshes": [{
            "name": "arena2_base_floor",
            "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1},
                            "indices": 2, "mode": 4}],
        }],
        "buffers": [{"byteLength": len(binary)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(pos_bytes),
             "target": 34962},
            {"buffer": 0, "byteOffset": len(pos_bytes),
             "byteLength": len(nrm_bytes), "target": 34962},
            {"buffer": 0, "byteOffset": len(pos_bytes) + len(nrm_bytes),
             "byteLength": len(idx_bytes), "target": 34963},
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 4,
             "type": "VEC3", "min": [-xs, -ys, zs], "max": [xs, ys, zs]},
            {"bufferView": 1, "componentType": 5126, "count": 4,
             "type": "VEC3"},
            {"bufferView": 2, "componentType": 5123, "count": 6,
             "type": "SCALAR"},
        ],
    }
    payload = json.dumps(doc, separators=(",", ":")).encode("utf-8")
    payload += b" " * ((4 - len(payload) % 4) % 4)
    bin_chunk = binary + b"\x00" * ((4 - len(binary) % 4) % 4)
    total = 12 + 8 + len(payload) + 8 + len(bin_chunk)
    out = bytearray()
    out += b"glTF" + struct.pack("<II", 2, total)
    out += struct.pack("<I", len(payload)) + b"JSON" + payload
    out += struct.pack("<I", len(bin_chunk)) + b"BIN\x00" + bin_chunk
    GLB.write_bytes(bytes(out))


def main() -> int:
    verts = load_mesh(MESH)

    def bounds(pick):
        s = [v for v in verts if pick(v)]
        return (
            min(v[0] for v in s) / UU,
            max(v[0] for v in s) / UU,
            min(v[1] for v in s) / UU,
            max(v[1] for v in s) / UU,
            min(v[2] for v in s) / UU,
            max(v[2] for v in s) / UU,
        )

    floor_bounds = bounds(lambda v: abs(v[2]) < 1.0)
    ceiling_bounds = bounds(lambda v: v[2] > 2040.0)

    # Straight-section cross-sections, measured away from corners and goals.
    side_lower = profile(
        verts,
        lambda v: -2500.0 < v[0] < 2500.0 and v[1] > 3500.0 and 0.0 <= v[2] < 400.0,
        lambda v: (v[1], v[2]),
    )
    side_upper = profile(
        verts,
        lambda v: -2500.0 < v[0] < 2500.0 and v[1] > 3000.0 and v[2] > 1500.0,
        lambda v: (v[1], v[2]),
    )
    back_lower_raw = profile(
        verts,
        lambda v: abs(v[1]) > 3000.0 and v[0] > 4500.0 and 0.0 <= v[2] < 400.0,
        lambda v: (v[0], v[2]),
    )

    # Wall faces: the vertical band between the two ramps.
    wall_face = max(
        v[1] for v in verts
        if -2000.0 < v[0] < 2000.0 and 500.0 < v[2] < 1500.0
    ) / UU
    back_face = max(
        v[0] for v in verts
        if 2000.0 < abs(v[1]) < 2900.0 and 500.0 < v[2] < 1500.0
    ) / UU
    ceiling_face = min(v[2] for v in verts if v[2] > 2040.0) / UU
    # Diagonal wall face: restrict to the wall band, away from floor plates and
    # goal/net geometry (which sit at much larger |x|+|y| sums).
    corner_plane = max(
        abs(v[0]) + abs(v[1])
        for v in verts
        if 3300.0 < v[0] < 4600.0 and 3300.0 < v[1] < 4600.0
        and 300.0 < v[2] < 1600.0
    ) / UU

    # Plan placement uses the DOCUMENTED field dimensions. The measured faces
    # are ~0.1 m outside them (mesh thickness), while the curve profiles are
    # MEASURED. Dimensions from documentation, curvature from measurement.
    half_length = DOCUMENTED["length_m"] / 2.0
    half_width = DOCUMENTED["width_m"] / 2.0
    corner_cut = round(half_length - (DOCUMENTED["corner_plane_m"] - half_width), 4)
    diagonal = round(math.sqrt(2.0) * corner_cut, 4)

    # Normalise both ramps onto the wall plane: u = inward offset (<= 0).
    def to_wall(points, face):
        out = []
        for u, v in points:
            out.append((min(0.0, round(u - face, 4)), v))
        return out

    lower = to_wall(side_lower, half_width)
    upper = to_wall(side_upper, half_width)
    # Ceiling plate bottom is the playable surface.
    upper = [(u, min(v, round(ceiling_face, 4))) for u, v in upper]

    reference = {
        "id": "arena2-geometry",
        "description": "Standard Soccar collision geometry used to generate arena2.",
        "units": "metres",
        "provenance": {
            "DOCUMENTED": "published tables (RLBot / RocketSim / RLGym)",
            "MEASURED": "read directly from assets/maps/rl/pitch.obj",
            "DERIVED": "computed from measured or documented values",
            "ESTIMATED": "authored approximation, not verified against the mesh",
        },
        "source": {
            "mesh": "assets/maps/rl/pitch.obj",
            "triangles": 8028,
            "vertices": len(verts),
            "note": "reference only; not shipped and not loaded by the game",
        },
        "documented": DOCUMENTED,
        "measured": {
            "floor_bounds_m": [round(x, 4) for x in floor_bounds],
            "ceiling_bounds_m": [round(x, 4) for x in ceiling_bounds],
            "wall_face_m": round(wall_face, 4),
            "back_face_m": round(back_face, 4),
            "ceiling_face_m": round(ceiling_face, 4),
            "corner_plane_m": round(corner_plane, 4),
            "side_lower_profile_m": lower,
            "side_upper_profile_m": upper,
            "back_lower_profile_m": back_lower_raw,
        },
        "derived": {
            "half_length_m": round(half_length, 4),
            "half_width_m": round(half_width, 4),
            "corner_cut_m": corner_cut,
            "diagonal_length_m": diagonal,
            "goal_opening_width_m": DOCUMENTED["goal_width_m"],
            "goal_depth_m": DOCUMENTED["goal_depth_m"],
        },
        "estimated": {
            "goal_back_curve_radius_m": 2.0,
            "note": "goal interior curvature not measured yet",
        },
    }
    REFERENCE.parent.mkdir(parents=True, exist_ok=True)
    REFERENCE.write_text(json.dumps(reference, indent=2) + "\n", encoding="utf-8")

    # ── arena2 map ─────────────────────────────────────────────────────────
    goal_half = DOCUMENTED["goal_width_m"] / 2.0
    goal_height = DOCUMENTED["goal_height_m"]
    goal_back = DOCUMENTED["goal_back_m"]
    lower_top = lower[-1][1] if lower else 3.55
    upper_bottom = upper[-1][1] if upper else 16.39
    ceiling_z = ceiling_face
    # A 1 m wall let a 23 m/s car tunnel through at the wall/upper-profile
    # seam; the solver then pushed it out whichever face was nearest and it
    # ended behind the wall at (-52.8, 33). Two metres plus a 1 m profile
    # overlap closes the seam without moving the playable inner face.
    wall_thickness = 2.0
    wall_overlap = 1.0
    # The vertical wall band must start where the lower ramp profile ends. The
    # old band started at lower_top (3.55) and overlapped the ramp volume; a car
    # on the ramp at z~3.86 was fully inside the band box, so the solver pushed
    # it down/out and the car snapped at y=41.9. Flush at 4.55 removes the pocket.
    wall_bottom = lower_top + wall_overlap
    lower = [[lower[0][0], -1.0]] + list(lower) + [[0.0, lower_top + wall_overlap]]
    upper = [[upper[-1][0], upper[-1][1] - wall_overlap]] + list(upper)

    blocks = []

    def add(block_id, shape, position, size, material, color,
            rotation=(0.0, 0.0, 0.0), **extra):
        block = {
            "id": block_id,
            "shape": shape,
            "position": [round(v, 4) for v in position],
            "size": [round(v, 4) for v in size],
            "rotationEuler": [round(v, 4) for v in rotation],
            "material": material,
            "color": color,
            "enabled": True,
            "collision": True,
            "anchored": True,
            "tags": ["arena"],
        }
        block.update(extra)
        blocks.append(block)

    # Tints above 1 deliberately brighten: the arena has no dedicated light
    # rig, and an unlit ceiling read as black at ambient 0.5.
    floor_col = [1.10, 1.15, 1.10, 1.0]
    wall_col = [1.35, 1.35, 1.45, 1.0]
    ceil_col = [2.10, 2.15, 2.30, 1.0]
    goal_col = [1.60, 1.60, 1.80, 1.0]

    # One solid box per surface, not a tiled field. A single 120 x 82 m quad pair
    # dropped the ball straight through (run 20261126_230039: ball rested at
    # z = -1.31), but arena1's single 102 x 82 m box floor works and keeps the
    # ball. Tiling added a collision seam every 10 m; the ball's rounded contact
    # shell caught each seam corner and launched itself (run 20261127_051141:
    # 18 m/s and 23 m/s velocity changes at x = 9.75 on flat ground). A box has
    # thickness, so the swept sphere cannot pass it. Top stays at -0.01 m like
    # arena1.
    add("floor", "box", (0.0, 0.0, -1.51),
        (2 * goal_back + 8.0, 2 * half_width + 8.0, 3.0), "grass", floor_col,
        tags=["arena", "floor"])
    add("ceiling", "box", (0.0, 0.0, ceiling_z + 0.5),
        (2 * goal_back, 2 * half_width, 1.0), "wallv4", ceil_col,
        tags=["arena", "ceiling"])

    # Wall faces and their outward yaw. Local +x is the outward normal.
    walls = [
        ("long_ne", (0.0, half_width, 0.0), 90.0),
        ("long_nw", (0.0, half_width, 0.0), 90.0),
        ("long_se", (0.0, -half_width, 0.0), -90.0),
        ("long_sw", (0.0, -half_width, 0.0), -90.0),
        ("back_e", (half_length, 0.0, 0.0), 0.0),
        ("back_w", (-half_length, 0.0, 0.0), 180.0),
    ]
    # Simple straight walls: one lower ramp, one band, one upper ramp each.
    # The span is the playfield corner cut, NOT the goal-back length: using
    # 2*corner_plane - 2*half_length (58.88 m) left a ~10 m open gap between the
    # long wall end (x=29.44) and the corner wall start (x=39.99), which is where
    # the ball escaped at x~30, y~-40.9.
    long_span = 2 * (corner_plane - half_width)
    straight = [
        ("long_pos", (0.0, half_width, 0.0), 90.0, long_span),
        ("long_neg", (0.0, -half_width, 0.0), -90.0, long_span),
    ]
    for name, pos, yaw, span in straight:
        add(f"{name}_ramp_lower", "profile", pos, (1.0, span, 1.0), "wallv2",
            wall_col, rotation=(0.0, 0.0, yaw), profile=lower, profile_axis=1)
        # Rotated 90 degrees, so the span must live on the local y axis: a
        # (span, 1, h) box became a 79 m wall through the middle of the field
        # and the ball fell through the floor (run 20261126_231000).
        add(f"{name}_wall", "box", (pos[0], pos[1] + (wall_thickness / 2.0 if pos[1] > 0 else -wall_thickness / 2.0), (wall_bottom + upper_bottom) / 2.0),
            (wall_thickness, span, upper_bottom - wall_bottom), "wallv2", wall_col,
            rotation=(0.0, 0.0, yaw))
        add(f"{name}_ramp_upper", "profile", (pos[0], pos[1], 0.0), (1.0, span, 1.0),
            "wallv2", wall_col, rotation=(0.0, 0.0, yaw), profile=upper,
            profile_axis=1)

    # Back walls: split around the goal opening.
    back_span = 2 * (corner_plane - half_width)
    for name, sign in (("back_e", 1.0), ("back_w", -1.0)):
        face_x = sign * half_length
        yaw = 0.0 if sign > 0 else 180.0
        side_span = back_span - 2 * goal_half
        side_centre = goal_half + side_span / 2.0
        for suffix, sy in (("l", -1.0), ("r", 1.0)):
            add(f"{name}_{suffix}_ramp_lower", "profile",
                (face_x, sy * side_centre, 0.0), (1.0, side_span, 1.0),
                "wallv2", wall_col, rotation=(0.0, 0.0, yaw), profile=lower,
                profile_axis=1)
            add(f"{name}_{suffix}_wall", "box",
                (face_x + sign * (wall_thickness / 2.0), sy * side_centre, (wall_bottom + upper_bottom) / 2.0),
                (wall_thickness, side_span, upper_bottom - wall_bottom), "wallv2", wall_col,
                rotation=(0.0, 0.0, yaw))
            add(f"{name}_{suffix}_ramp_upper", "profile",
                (face_x, sy * side_centre, 0.0), (1.0, side_span, 1.0),
                "wallv2", wall_col, rotation=(0.0, 0.0, yaw), profile=upper,
                profile_axis=1)
        # Above the goal opening.
        add(f"{name}_header_wall", "box",
            (face_x + sign * (wall_thickness / 2.0), 0.0, (goal_height + upper_bottom) / 2.0),
            (wall_thickness, 2 * goal_half, upper_bottom - goal_height), "wallv4", goal_col,
            rotation=(0.0, 0.0, yaw))
        add(f"{name}_header_ramp_upper", "profile", (face_x, 0.0, 0.0),
            (1.0, 2 * goal_half, 1.0), "wallv4", goal_col,
            rotation=(0.0, 0.0, yaw), profile=upper, profile_axis=1)
        # Goal recess: side walls, ceiling, back.
        for suffix, sy in (("l", -1.0), ("r", 1.0)):
            add(f"{name}_goal_side_{suffix}", "box",
                (sign * (half_length + (goal_back - half_length) / 2.0),
                 sy * goal_half, goal_height / 2.0),
                (goal_back - half_length, 0.5, goal_height), "wallv4", goal_col)
        add(f"{name}_goal_ceiling", "box",
            (sign * (half_length + (goal_back - half_length) / 2.0), 0.0,
             goal_height + 0.25),
            (goal_back - half_length, 2 * goal_half, 0.5), "wallv4", goal_col)
        add(f"{name}_goal_back", "box",
            (sign * (goal_back - 0.5), 0.0, goal_height / 2.0),
            (1.0, 2 * goal_half, goal_height), "wallv4", goal_col)

    # Corner walls: 45 degree boxes with the same measured profiles.
    for name, sx, sy in (("corner_ne", 1.0, 1.0), ("corner_nw", -1.0, 1.0),
                         ("corner_se", 1.0, -1.0), ("corner_sw", -1.0, -1.0)):
        mid_x = sx * (half_length - corner_cut / 2.0)
        mid_y = sy * (half_width - corner_cut / 2.0)
        yaw = math.degrees(math.atan2(sy, sx))
        add(f"{name}_ramp_lower", "profile", (mid_x, mid_y, 0.0),
            (1.0, diagonal, 1.0), "wallv2", wall_col,
            rotation=(0.0, 0.0, yaw), profile=lower, profile_axis=1)
        add(f"{name}_wall", "box",
            (mid_x + sx * (wall_thickness / 2.0), mid_y + sy * (wall_thickness / 2.0), (wall_bottom + upper_bottom) / 2.0),
            (wall_thickness, diagonal, upper_bottom - wall_bottom), "wallv2", wall_col,
            rotation=(0.0, 0.0, yaw))
        add(f"{name}_ramp_upper", "profile", (mid_x, mid_y, 0.0),
            (1.0, diagonal, 1.0), "wallv2", wall_col,
            rotation=(0.0, 0.0, yaw), profile=upper, profile_axis=1)

    # Invisible collision backstops behind the visible walls/upper profiles.
    # The ramp/wall seams are curved ribbons; a fast car can slip behind one.
    # These boxes stop it before it leaves the arena; opacity 0 keeps them out
    # of the visual. They overlap the visible wall (inner face 42.4 on the long
    # walls, 52.4 on the back walls) so no pocket remains.
    backstop_col = [0.0, 0.0, 0.0, 0.0]
    add("long_pos_backstop", "box", (0.0, 43.4, 10.2),
        (108.0, 2.0, 20.4), "wallv2", backstop_col, opacity=0.0)
    add("long_neg_backstop", "box", (0.0, -43.4, 10.2),
        (108.0, 2.0, 20.4), "wallv2", backstop_col, opacity=0.0)
    for name, sign in (("back_e", 1.0), ("back_w", -1.0)):
        for suffix, sy in (("l", -1.0), ("r", 1.0)):
            add(f"{name}_backstop_{suffix}", "box",
                (sign * 53.4, sy * 26.5, 10.2),
                (2.0, 35.0, 20.4), "wallv2", backstop_col, opacity=0.0)
        add(f"{name}_backstop_header", "box",
            (sign * 53.4, 0.0, 13.45),
            (2.0, 18.0, 13.9), "wallv2", backstop_col, opacity=0.0)

    arena1 = json.loads((ROOT / "config" / "maps" / "arena1.json").read_text(encoding="utf-8"))
    goals = []
    for goal in arena1["arena"]["goals"]:
        goals.append({
            "id": goal["id"],
            "team": goal["team"],
            "enabled": True,
            "center": [goal["center"][0], 0.0, goal["center"][2]],
            "facing": goal["facing"],
            "halfExtents": [goal["halfExtents"][0], goal["halfExtents"][1],
                            goal["halfExtents"][2]],
        })

    document = {
        "mapId": "arena2",
        "name": "Arena 2 (measured Rocket League geometry)",
        "description": (
            "Standard Soccar arena generated from the measured collision mesh: "
            "45 degree corner cuts, measured lower and upper wall ramps, 8.8 m "
            "goal recess. See config/reference/arena2-geometry.json."
        ),
        "worldMode": "static",
        "arena": {
            "ball": dict(arena1["arena"]["ball"],
                         spawn=[0.0, 0.0, 0.9525]),
            "goals": goals,
            "kickoffs": arena1["arena"]["kickoffs"],
        },
        "blocks": blocks,
        "boost_pads": arena1.get("boost_pads", []),
        "entities": [],
        "glb_objects": [],
        "vehicles": arena1.get("vehicles", []),
        "chunks": [],
        "attachments": [],
        "generation_zones": [],
        "npc_presets": [],
        "weapon_overrides": [],
        "interactions": [],
    }
    MAP.write_text(json.dumps(document, indent=1) + "\n", encoding="utf-8")

    write_glb(half_length, half_width, goal_back)
    print(f"reference -> {REFERENCE.relative_to(ROOT)}")
    print(f"map       -> {MAP.relative_to(ROOT)} blocks={len(blocks)}")
    print("wall_face", round(wall_face, 3), "back_face", round(back_face, 3),
          "ceiling", round(ceiling_face, 3), "corner", round(corner_plane, 3))
    print("lower", lower)
    print("upper", upper)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
