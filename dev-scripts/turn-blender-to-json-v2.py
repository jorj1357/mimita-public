import os
import sys
import math
import json
import subprocess

# ============================================================
# CONFIG
# ============================================================

BLENDER_EXE = r"C:\Program Files\Blender Foundation\Blender 5.0\blender.exe"

OUT_DIR = r"C:\important\quiet\n\mimita-public\mimita-public\assets\maps\json-converts"

POS_STEP = 0.1
ROT_STEP = 15.0

# ============================================================
# HELPERS (shared logic)
# ============================================================

def snap(v, step, decimals):
    return round(round(v / step) * step, decimals)

def normalize_deg(a):
    a = a % 360.0
    if a >= 180.0:
        a -= 360.0
    return a

# ============================================================
# BLENDER MODE (runs INSIDE Blender)
# ============================================================

def run_inside_blender():
    import bpy  # ONLY valid inside Blender

    def snap_position(loc):
        return [
            snap(loc.x, POS_STEP, 1),
            snap(loc.y, POS_STEP, 1),
            snap(loc.z, POS_STEP, 1),
        ]

    def snap_rotation(rot):
        deg = [math.degrees(r) for r in rot]
        norm = [normalize_deg(r) for r in deg]
        return [int(snap(r, ROT_STEP, 0)) for r in norm]
    
    os.makedirs(OUT_DIR, exist_ok=True)

    export = {}

    for obj in bpy.context.scene.objects:
        if obj.type not in {"MESH", "EMPTY"}:
            continue

        export[obj.name] = {
            "position": snap_position(obj.location),
            "rotation": snap_rotation(obj.rotation_euler),
        }

    blend_name = os.path.splitext(os.path.basename(bpy.data.filepath))[0]
    out_path = os.path.join(OUT_DIR, blend_name + "-converted.json")

    with open(out_path, "w") as f:
        json.dump(export, f, indent=2)

    print("Exported JSON to:", out_path)

# ============================================================
# LAUNCHER MODE (double-clicked)
# ============================================================

def run_as_launcher():
    print("--------------------------------------------------")
    print(" Blender → JSON Map Converter")
    print("--------------------------------------------------")
    print("Drag a .blend file here and press Enter")
    print()

    blend_path = input("> ").strip().strip('"')

    if not blend_path.lower().endswith(".blend"):
        print("ERROR: Not a .blend file")
        input("Press Enter to exit...")
        return

    if not os.path.exists(blend_path):
        print("ERROR: File does not exist")
        input("Press Enter to exit...")
        return

    cmd = [
        BLENDER_EXE,
        blend_path,
        "--background",
        "--python",
        os.path.abspath(__file__),
    ]

    print()
    print("Running Blender headless...")
    print()

    subprocess.run(cmd)

    print()
    print("DONE.")
    input("Press Enter to close...")

# ============================================================
# ENTRY POINT
# ============================================================

try:
    import bpy
    run_inside_blender()
except ImportError:
    run_as_launcher()
