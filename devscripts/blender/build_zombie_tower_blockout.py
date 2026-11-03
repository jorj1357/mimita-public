"""Build the first procedural Zombie Tower blockout in Blender.

Run this inside Blender's Python Console or via the MiMITA Blender MCP:
    build_zombie_tower_blockout(seed=20261102)

The script owns only the collection named by ROOT_COLLECTION. Re-running it
replaces that generated collection and preserves all other scene content.
"""

import math
import random
import os

import bpy
from mathutils import Quaternion, Vector

ROOT_COLLECTION = "Zombie Tower | Procedural Blockout"
FLOOR_COLLECTION = "Floors 001-100"
BASE_COLLECTION = "Foundation and Rooftop"
LABEL_COLLECTION = "Floor Labels and Landmarks"

FOUNDATION_BOTTOM_Z = 20.0
FOUNDATION_HEIGHT = 10.0
FOUNDATION_TOP_Z = FOUNDATION_BOTTOM_Z + FOUNDATION_HEIGHT
FLOOR_COUNT = 100
FLOOR_RISE = 50.0
FLOOR_SIZE = 100.0
RAMP_CENTERLINE_RADIUS = 32.0
RAMP_WIDTH = 32.0
TRANSITION_CLEAR_HEADROOM = 3.8
RAMP_THICKNESS = 2.0
RAMP_SIDES_PER_FLOOR = 4
RAMP_SIDE_LENGTH = 2.0 * RAMP_CENTERLINE_RADIUS
RAMP_RISE_PER_SIDE = FLOOR_RISE / RAMP_SIDES_PER_FLOOR
TOWER_TOP_Z = FOUNDATION_TOP_Z + FLOOR_COUNT * FLOOR_RISE

FEATURE_LAYOUTS = (
    "curved_route",
    "tunnel_gallery",
    "long_corridor",
    "lava_gallery",
    "narrow_bridge",
    "obstacle_gallery",
    "vertical_chamber",
    "broken_architecture",
    "floating_platforms",
    "alternate_route",
)


def _remove_generated_collection():
    root = bpy.data.collections.get(ROOT_COLLECTION)
    if root is None:
        return
    for obj in list(root.all_objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for child in list(root.children):
        bpy.data.collections.remove(child, do_unlink=True)
    bpy.data.collections.remove(root, do_unlink=True)


def _collection(name, parent):
    collection = bpy.data.collections.new(name)
    parent.children.link(collection)
    return collection


def _material(name, color, metallic=0.0, roughness=0.7):
    material = bpy.data.materials.get(name)
    if material is None:
        material = bpy.data.materials.new(name)
    material.diffuse_color = (*color, 1.0)
    material.use_nodes = True
    bsdf = next(node for node in material.node_tree.nodes
                if node.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    return material


def _add_mesh(name, vertices, faces, collection, material, properties=None):
    mesh = bpy.data.meshes.new(name + " Mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.validate(clean_customdata=False)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    if material is not None:
        obj.data.materials.append(material)
    if properties:
        for key, value in properties.items():
            obj[key] = value
    return obj


def _append_box(vertices, faces, center, size):
    cx, cy, cz = center
    sx, sy, sz = (component * 0.5 for component in size)
    start = len(vertices)
    vertices.extend([
        (cx - sx, cy - sy, cz - sz), (cx + sx, cy - sy, cz - sz),
        (cx + sx, cy + sy, cz - sz), (cx - sx, cy + sy, cz - sz),
        (cx - sx, cy - sy, cz + sz), (cx + sx, cy - sy, cz + sz),
        (cx + sx, cy + sy, cz + sz), (cx - sx, cy + sy, cz + sz),
    ])


def _floor_deck_with_ramp_passage(top_z):
    """Return a solid deck whose only opening follows the wide ramp turn."""
    bottom_z = top_z - 2.0
    xs = (-50.0, -48.0, -32.0, -16.0, 32.0, 50.0)
    ys = (-50.0, -48.0, -32.0, -16.0, 32.0, 50.0)
    vertices, faces, vertex_ids = [], [], {}

    def vertex_id(x, y, z):
        key = (float(x), float(y), float(z))
        if key not in vertex_ids:
            vertex_ids[key] = len(vertices)
            vertices.append(key)
        return vertex_ids[key]

    def passage_cell(x, y):
        incoming = -48.0 <= x <= -16.0 and -32.0 <= y <= 32.0
        turn = -48.0 <= x <= -16.0 and -48.0 <= y <= -16.0
        outgoing = -32.0 <= x <= 32.0 and -48.0 <= y <= -16.0
        return incoming or turn or outgoing

    boundary_edges = {}
    for ix in range(len(xs) - 1):
        for iy in range(len(ys) - 1):
            x1, x2 = xs[ix], xs[ix + 1]
            y1, y2 = ys[iy], ys[iy + 1]
            if passage_cell((x1 + x2) * 0.5, (y1 + y2) * 0.5):
                continue
            top = (vertex_id(x1, y1, top_z), vertex_id(x2, y1, top_z),
                   vertex_id(x2, y2, top_z), vertex_id(x1, y2, top_z))
            bottom = (vertex_id(x1, y1, bottom_z), vertex_id(x2, y1, bottom_z),
                      vertex_id(x2, y2, bottom_z), vertex_id(x1, y2, bottom_z))
            faces.extend((top, tuple(reversed(bottom))))
            for a, b in zip(top, top[1:] + top[:1]):
                edge = tuple(sorted((a, b)))
                boundary_edges[edge] = boundary_edges.get(edge, 0) + 1

    for edge, count in boundary_edges.items():
        if count != 1:
            continue
        a, b = edge
        pa, pb = vertices[a], vertices[b]
        bottom_a = vertex_id(pa[0], pa[1], bottom_z)
        bottom_b = vertex_id(pb[0], pb[1], bottom_z)
        faces.append((a, b, bottom_b, bottom_a))
    return vertices, faces
    faces.extend([
        (start + 0, start + 3, start + 2, start + 1),
        (start + 4, start + 5, start + 6, start + 7),
        (start + 0, start + 1, start + 5, start + 4),
        (start + 1, start + 2, start + 6, start + 5),
        (start + 2, start + 3, start + 7, start + 6),
        (start + 3, start + 0, start + 4, start + 7),
    ])


def _append_ramp_panel(vertices, faces, start_xy, end_xy, start_z, end_z,
                       width, thickness=RAMP_THICKNESS):
    dx = end_xy[0] - start_xy[0]
    dy = end_xy[1] - start_xy[1]
    length = math.hypot(dx, dy)
    nx, ny = -dy / length, dx / length
    half = width * 0.5
    p0_minus = (start_xy[0] - nx * half, start_xy[1] - ny * half, start_z)
    p1_minus = (end_xy[0] - nx * half, end_xy[1] - ny * half, end_z)
    p1_plus = (end_xy[0] + nx * half, end_xy[1] + ny * half, end_z)
    p0_plus = (start_xy[0] + nx * half, start_xy[1] + ny * half, start_z)
    top = (p0_minus, p1_minus, p1_plus, p0_plus)
    bottom = tuple((x, y, z - thickness) for x, y, z in top)
    start = len(vertices)
    vertices.extend(top)
    vertices.extend(bottom)
    faces.extend([
        (start + 0, start + 1, start + 2, start + 3),
        (start + 4, start + 7, start + 6, start + 5),
        (start + 0, start + 4, start + 5, start + 1),
        (start + 1, start + 5, start + 6, start + 2),
        (start + 2, start + 6, start + 7, start + 3),
        (start + 3, start + 7, start + 4, start + 0),
    ])


def _append_bezier_ribbon(vertices, faces, start, control, end, width, steps=28):
    points = []
    for index in range(steps + 1):
        t = index / steps
        u = 1.0 - t
        points.append(Vector(start) * (u * u) + Vector(control) * (2.0 * u * t)
                      + Vector(end) * (t * t))
    first = len(vertices)
    half = width * 0.5
    for index, point in enumerate(points):
        if index == 0:
            tangent = (points[1] - point).normalized()
        elif index == len(points) - 1:
            tangent = (point - points[index - 1]).normalized()
        else:
            tangent = (points[index + 1] - points[index - 1]).normalized()
        normal = Vector((-tangent.y, tangent.x, 0.0)).normalized()
        for side in (-1.0, 1.0):
            p = point + normal * half * side
            vertices.append((p.x, p.y, p.z))
    for index in range(steps):
        a = first + index * 2
        faces.append((a, a + 2, a + 3, a + 1))


def _append_cylinder(vertices, faces, center, radius, depth, sides=16):
    cx, cy, cz = center
    start = len(vertices)
    for z in (cz - depth * 0.5, cz + depth * 0.5):
        for index in range(sides):
            angle = 2.0 * math.pi * index / sides
            vertices.append((cx + radius * math.cos(angle),
                             cy + radius * math.sin(angle), z))
    faces.append(tuple(start + index for index in reversed(range(sides))))
    faces.append(tuple(start + sides + index for index in range(sides)))
    for index in range(sides):
        nxt = (index + 1) % sides
        faces.append((start + index, start + nxt,
                      start + sides + nxt, start + sides + index))


def _add_text(name, body, location, rotation, size, collection, material,
              properties=None):
    data = bpy.data.curves.new(name + " Text", type="FONT")
    data.body = body
    data.size = size
    data.extrude = 0.04
    data.bevel_depth = 0.015
    data.align_x = "LEFT"
    obj = bpy.data.objects.new(name, data)
    collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler = rotation
    obj.data.materials.append(material)
    if properties:
        for key, value in properties.items():
            obj[key] = value
    return obj


def _layout_schedule(seed):
    rng = random.Random(seed)
    layouts = list(FEATURE_LAYOUTS) * 9 + ["standard"] * 10
    rng.shuffle(layouts)
    return layouts


def _feature_mesh(floor_number, layout, base_z, collection, materials, rng, seed):
    vertices, faces = [], []
    level_z = base_z + FLOOR_RISE * 0.5
    ramp_start = (-RAMP_CENTERLINE_RADIUS, -RAMP_CENTERLINE_RADIUS, base_z)
    ramp_opposite = (RAMP_CENTERLINE_RADIUS, RAMP_CENTERLINE_RADIUS, level_z)
    feature_material = materials["accent"]

    if layout == "tunnel_gallery":
        surface_z = base_z + RAMP_RISE_PER_SIDE * 0.5
        y = -RAMP_CENTERLINE_RADIUS
        for x in (-5.5, 5.5):
            _append_box(vertices, faces, (x, y, surface_z + 4.5), (1.4, 9.5, 9.0))
        _append_box(vertices, faces, (0.0, y, surface_z + 9.0), (12.5, 10.0, 1.5))
    elif layout in ("curved_route", "alternate_route"):
        control_offset = 13.0 if layout == "curved_route" else 0.0
        control = (control_offset, -control_offset, base_z + FLOOR_RISE * 0.25)
        _append_bezier_ribbon(vertices, faces, ramp_start, control, ramp_opposite,
                              5.5 if layout == "curved_route" else 8.0)
    elif layout == "lava_gallery":
        _append_cylinder(vertices, faces, (0.0, 0.0, base_z + 10.0),
                         21.0, 1.5, sides=32)
        for index in range(8):
            angle = 2.0 * math.pi * index / 8.0
            x, y = 23.5 * math.cos(angle), 23.5 * math.sin(angle)
            _append_box(vertices, faces, (x, y, base_z + 13.5), (2.0, 2.0, 7.0))
        feature_material = materials["lava"]
    elif layout == "obstacle_gallery":
        for index in range(8):
            angle = (2.0 * math.pi * index / 8.0) + rng.uniform(-0.12, 0.12)
            radius = rng.uniform(14.0, 27.0)
            x, y = radius * math.cos(angle), radius * math.sin(angle)
            height = rng.uniform(2.0, 7.0)
            _append_box(vertices, faces, (x, y, base_z + 8.0),
                        (rng.uniform(2.0, 5.0), rng.uniform(2.0, 5.0), height))
    elif layout == "vertical_chamber":
        for x in (-18.0, 18.0):
            for y in (-18.0, 18.0):
                _append_box(vertices, faces, (x, y, base_z + 21.0), (3.5, 3.5, 42.0))
        _append_box(vertices, faces, (0.0, 0.0, base_z + 17.0), (28.0, 4.0, 2.0))
        _append_box(vertices, faces, (0.0, 0.0, base_z + 33.0), (4.0, 28.0, 2.0))
    elif layout == "broken_architecture":
        _append_box(vertices, faces, (-15.0, 2.0, base_z + 9.0), (5.0, 22.0, 18.0))
        _append_box(vertices, faces, (10.0, 14.0, base_z + 15.0), (22.0, 4.0, 3.0))
        _append_box(vertices, faces, (18.0, -10.0, base_z + 6.0), (4.0, 16.0, 12.0))
        _append_box(vertices, faces, (0.0, 0.0, base_z + 26.0), (20.0, 3.0, 2.5))
    elif layout == "floating_platforms":
        for index in range(1, 10, 2):
            t = index / 10.0
            x = ramp_start[0] + (ramp_opposite[0] - ramp_start[0]) * t
            y = ramp_start[1] + (ramp_opposite[1] - ramp_start[1]) * t
            z = ramp_start[2] + (ramp_opposite[2] - ramp_start[2]) * t
            _append_box(vertices, faces, (x, y, z), (9.0, 9.0, 1.5))
        feature_material = materials["bridge"]
    elif layout == "long_corridor":
        y = -RAMP_CENTERLINE_RADIUS
        surface_z = base_z + RAMP_RISE_PER_SIDE * 0.5
        for x in (-24.0, -8.0, 8.0, 24.0):
            _append_box(vertices, faces, (x, y - 5.0, surface_z + 4.0), (0.8, 5.0, 8.0))
            _append_box(vertices, faces, (x, y + 5.0, surface_z + 4.0), (0.8, 5.0, 8.0))
    elif layout == "narrow_bridge":
        for t in (0.25, 0.5, 0.75):
            x = -RAMP_CENTERLINE_RADIUS + 2.0 * RAMP_CENTERLINE_RADIUS * t
            _append_box(vertices, faces, (x, 0.0, level_z + 2.0), (5.0, 18.0, 2.0))
        feature_material = materials["bridge"]

    if vertices:
        return _add_mesh(
            f"Floor {floor_number:03d} | {layout.replace('_', ' ').title()}",
            vertices, faces, collection, feature_material,
            {"floor_number": floor_number, "layout": layout,
             "procedural_seed": seed,
             "feature_role": "visual blockout; verify traversal in MiMITA"})
    return None


def build_zombie_tower_blockout(seed=20261102, expected_filename=None):
    """Replace only this generator's collection and create the 100-floor blockout."""
    if bpy.data.filepath and not bpy.data.filepath.lower().endswith(".blend"):
        raise RuntimeError("The active Blender document is not a saved .blend file")
    if expected_filename and os.path.basename(bpy.data.filepath).lower() != expected_filename.lower():
        raise RuntimeError("The active Blender file does not match the requested project file")

    _remove_generated_collection()
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.length_unit = "METERS"
    scene.unit_settings.scale_length = 1.0

    root = bpy.data.collections.new(ROOT_COLLECTION)
    scene.collection.children.link(root)
    base_collection = _collection(BASE_COLLECTION, root)
    floors_collection = _collection(FLOOR_COLLECTION, root)
    labels_collection = _collection(LABEL_COLLECTION, root)

    materials = {
        "foundation": _material("ZT | Foundation basalt", (0.12, 0.16, 0.21), 0.15, 0.78),
        "ramp": _material("ZT | Traversal sandstone", (0.48, 0.39, 0.27), 0.05, 0.82),
        "floor": _material("ZT | Solid floor decks", (0.34, 0.40, 0.46), 0.08, 0.8),
        "structure": _material("ZT | Tower slate", (0.23, 0.30, 0.37), 0.12, 0.76),
        "accent": _material("ZT | Architecture accent", (0.35, 0.52, 0.58), 0.12, 0.55),
        "lava": _material("ZT | Lava blockout", (0.82, 0.13, 0.035), 0.05, 0.38),
        "bridge": _material("ZT | Bridge metal", (0.78, 0.52, 0.18), 0.35, 0.45),
        "roof": _material("ZT | Rooftop arena", (0.24, 0.47, 0.44), 0.12, 0.7),
        "label": _material("ZT | Floor labels", (0.95, 0.72, 0.22), 0.15, 0.45),
    }

    base_vertices, base_faces = [], []
    _append_box(base_vertices, base_faces, (0.0, 0.0, FOUNDATION_BOTTOM_Z + FOUNDATION_HEIGHT * 0.5),
                (500.0, 500.0, FOUNDATION_HEIGHT))
    base = _add_mesh("Foundation | 500 × 500 × 10 m | bottom Z=20",
                      base_vertices, base_faces, base_collection, materials["foundation"],
                      {"bottom_z_m": FOUNDATION_BOTTOM_Z,
                       "top_z_m": FOUNDATION_TOP_Z,
                       "dimensions_m": "500 x 500 x 10"})
    bevel = base.modifiers.new("Soft foundation edges", "BEVEL")
    bevel.width = 1.5
    bevel.segments = 3
    base.modifiers.new("Weighted corner normals", "WEIGHTED_NORMAL")

    schedule = _layout_schedule(seed)
    rng = random.Random(seed ^ 0x5A17)
    corners = [(-RAMP_CENTERLINE_RADIUS, -RAMP_CENTERLINE_RADIUS),
               (RAMP_CENTERLINE_RADIUS, -RAMP_CENTERLINE_RADIUS),
               (RAMP_CENTERLINE_RADIUS, RAMP_CENTERLINE_RADIUS),
               (-RAMP_CENTERLINE_RADIUS, RAMP_CENTERLINE_RADIUS)]
    for floor_index in range(FLOOR_COUNT):
        floor_number = floor_index + 1
        base_z = FOUNDATION_TOP_Z + floor_index * FLOOR_RISE
        layout = schedule[floor_index]
        # The solid deck includes an L-shaped opening along the incoming ramp,
        # turn landing and outgoing ramp. The ramp itself bridges this passage.
        deck_vertices, deck_faces = _floor_deck_with_ramp_passage(base_z)
        _add_mesh(
            f"Floor {floor_number:03d} | Solid 100x100 Deck",
            deck_vertices, deck_faces, floors_collection, materials["floor"],
            {"floor_number": floor_number, "deck_size_m": "100 x 100",
             "deck_thickness_m": 2.0, "walkable_top_z_m": base_z,
             "solid_fall_stop_except_ramp_passage": True,
             "passage_width_m": RAMP_WIDTH,
             "passage_clear_headroom_m": TRANSITION_CLEAR_HEADROOM})

        ramp_vertices, ramp_faces = [], []
        structure_vertices, structure_faces = [], []
        for side_index in range(4):
            start_xy = corners[side_index]
            end_xy = corners[(side_index + 1) % 4]
            start_z = base_z + side_index * RAMP_RISE_PER_SIDE
            end_z = start_z + RAMP_RISE_PER_SIDE
            _append_ramp_panel(ramp_vertices, ramp_faces, start_xy, end_xy,
                               start_z, end_z, RAMP_WIDTH)
            _append_box(ramp_vertices, ramp_faces,
                        (end_xy[0], end_xy[1], end_z - RAMP_THICKNESS * 0.5),
                        (RAMP_WIDTH, RAMP_WIDTH, RAMP_THICKNESS))

        _add_mesh(
            f"Floor {floor_number:03d} | Continuous ascent ramp",
            ramp_vertices, ramp_faces, floors_collection, materials["ramp"],
            {"floor_number": floor_number, "layout": layout,
             "footprint_m": "100 x 100", "rise_m": FLOOR_RISE,
             "ramp_width_m": RAMP_WIDTH,
             "transition_clear_headroom_m": TRANSITION_CLEAR_HEADROOM,
             "main_route_continuous": True,
             "procedural_seed": seed})

        if floor_number == 1:
            # A framed ground-level doorway is the only entry into the enclosed tower.
            _append_box(structure_vertices, structure_faces,
                        (-27.0, -49.0, base_z + FLOOR_RISE * 0.5),
                        (46.0, 2.0, FLOOR_RISE))
            _append_box(structure_vertices, structure_faces,
                        (27.0, -49.0, base_z + FLOOR_RISE * 0.5),
                        (46.0, 2.0, FLOOR_RISE))
            _append_box(structure_vertices, structure_faces,
                        (-40.0, -49.0, base_z + 27.0), (8.0, 2.0, 46.0))
        else:
            _append_box(structure_vertices, structure_faces,
                        (0.0, -49.0, base_z + FLOOR_RISE * 0.5),
                        (100.0, 2.0, FLOOR_RISE))
        for center, size in [
                ((0.0, 49.0, base_z + FLOOR_RISE * 0.5),
                 (100.0, 2.0, FLOOR_RISE)),
                ((-49.0, 0.0, base_z + FLOOR_RISE * 0.5),
                 (2.0, 100.0, FLOOR_RISE)),
                ((49.0, 0.0, base_z + FLOOR_RISE * 0.5),
                 (2.0, 100.0, FLOOR_RISE)),
        ]:
            _append_box(structure_vertices, structure_faces, center, size)

        _add_mesh(
            f"Floor {floor_number:03d} | 50 m walls and corner supports",
            structure_vertices, structure_faces, floors_collection, materials["structure"],
            {"floor_number": floor_number, "wall_height_m": FLOOR_RISE,
             "layout": layout, "perimeter_enclosed": True,
             "entrance_doorway": floor_number == 1,
             "procedural_seed": seed})

        _feature_mesh(floor_number, layout, base_z, floors_collection,
                      materials, rng, seed)

        marker = bpy.data.objects.new(f"FLOOR_{floor_number:03d}__Start", None)
        floors_collection.objects.link(marker)
        marker.empty_display_type = "CIRCLE"
        marker.empty_display_size = 6.0
        marker.location = (corners[0][0], corners[0][1], base_z)
        marker["floor_number"] = floor_number
        marker["layout"] = layout
        marker["is_non_standard"] = layout != "standard"
        marker["vertical_interval_m"] = FLOOR_RISE

        label_text = f"FLOOR {floor_number:03d}  |  {layout.replace('_', ' ').upper()}"
        _add_text(f"Label | Floor {floor_number:03d}", label_text,
                  (-47.0, -51.25, base_z + FLOOR_RISE * 0.5),
                  (math.radians(90.0), 0.0, 0.0), 5.0,
                  labels_collection, materials["label"],
                  {"floor_number": floor_number, "layout": layout})

    roof_vertices, roof_faces = [], []
    _append_box(roof_vertices, roof_faces, (0.0, 0.0, TOWER_TOP_Z - 1.0),
                (FLOOR_SIZE, FLOOR_SIZE, 2.0))
    for center, size in [
        ((0.0, -49.0, TOWER_TOP_Z + 1.0), (100.0, 2.0, 4.0)),
        ((0.0, 49.0, TOWER_TOP_Z + 1.0), (100.0, 2.0, 4.0)),
        ((-49.0, 0.0, TOWER_TOP_Z + 1.0), (2.0, 100.0, 4.0)),
        ((49.0, 0.0, TOWER_TOP_Z + 1.0), (2.0, 100.0, 4.0)),
    ]:
        _append_box(roof_vertices, roof_faces, center, size)
    _add_mesh("Floor 100 | Open-air rooftop arena", roof_vertices, roof_faces,
              base_collection, materials["roof"],
              {"floor_number": 100, "open_air": True,
               "top_z_m": TOWER_TOP_Z, "final_boss_arena_blockout": True})

    _add_text("Project sign | foundation", "ZOMBIE TOWER  |  100 FLOORS  |  5,000 m ASCENT",
              (-120.0, -205.0, FOUNDATION_TOP_Z + 0.2),
              (math.radians(90.0), 0.0, 0.0), 14.0,
              labels_collection, materials["label"],
              {"purpose": "blockout title"})
    _add_text("Project sign | rooftop", "FLOOR 100  |  OPEN-AIR ROOFTOP",
              (-42.0, -49.5, TOWER_TOP_Z + 4.0),
              (math.radians(90.0), 0.0, 0.0), 5.0,
              labels_collection, materials["label"],
              {"floor_number": 100, "open_air": True})

    nonstandard_count = sum(layout != "standard" for layout in schedule)
    scene["zombie_tower_seed"] = seed
    scene["zombie_tower_floor_count"] = FLOOR_COUNT
    scene["zombie_tower_foundation_bottom_z_m"] = FOUNDATION_BOTTOM_Z
    scene["zombie_tower_foundation_top_z_m"] = FOUNDATION_TOP_Z
    scene["zombie_tower_floor_rise_m"] = FLOOR_RISE
    scene["zombie_tower_solid_deck_count"] = FLOOR_COUNT
    scene["zombie_tower_solid_deck_size_m"] = "100 x 100"
    scene["zombie_tower_solid_deck_thickness_m"] = 2.0
    scene["zombie_tower_ramp_centerline_radius_m"] = RAMP_CENTERLINE_RADIUS
    scene["zombie_tower_ramp_width_m"] = RAMP_WIDTH
    scene["zombie_tower_transition_clear_headroom_m"] = TRANSITION_CLEAR_HEADROOM
    scene["zombie_tower_ramp_landing_passage_count"] = FLOOR_COUNT
    scene["zombie_tower_enclosed_perimeter_count"] = FLOOR_COUNT
    scene["zombie_tower_tower_top_z_m"] = TOWER_TOP_Z
    scene["zombie_tower_nonstandard_floor_count"] = nonstandard_count
    scene["zombie_tower_blockout_only"] = True

    world = scene.world
    if world is None:
        world = bpy.data.worlds.new("Zombie Tower | Night Blockout")
        scene.world = world
    world.color = (0.025, 0.035, 0.06)

    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == "VIEW_3D":
                region = area.spaces.active.region_3d
                region.view_location = (0.0, 0.0, TOWER_TOP_Z * 0.48)
                region.view_distance = 6900.0
                region.view_rotation = Quaternion((0.91, 0.29, 0.13, 0.25)).normalized()
                area.spaces.active.shading.type = "SOLID"
                area.spaces.active.shading.color_type = "MATERIAL"
                area.spaces.active.overlay.show_floor = False
                area.spaces.active.overlay.show_axis_x = False
                area.spaces.active.overlay.show_axis_y = False

    return {
        "floors": FLOOR_COUNT,
        "nonstandard_floors": nonstandard_count,
        "seed": seed,
        "foundation_bottom_z_m": FOUNDATION_BOTTOM_Z,
        "foundation_top_z_m": FOUNDATION_TOP_Z,
        "floor_1_start_z_m": FOUNDATION_TOP_Z,
        "floor_100_top_z_m": TOWER_TOP_Z,
        "ramp_width_m": RAMP_WIDTH,
        "ramp_centerline_radius_m": RAMP_CENTERLINE_RADIUS,
        "transition_clear_headroom_m": TRANSITION_CLEAR_HEADROOM,
        "floor_rise_m": FLOOR_RISE,
        "layout_counts": {layout: schedule.count(layout)
                           for layout in sorted(set(schedule))},
        "collection": ROOT_COLLECTION,
    }
