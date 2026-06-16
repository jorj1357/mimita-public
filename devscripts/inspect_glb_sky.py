import json, struct, sys, os, glob

glb_dir = r"C:\important\mimita-priv-v8\assets\maps"
glb_files = glob.glob(os.path.join(glb_dir, "*.glb"))
if not glb_files:
    print("No GLB files found in " + glb_dir)
    sys.exit(1)
glb_files = [os.path.basename(f) for f in glb_files]

for glb_file in glb_files:
    path = os.path.join(glb_dir, glb_file)
    print(f"\n{'='*60}")
    print(f"FILE: {glb_file}")
    print(f"{'='*60}")
    
    with open(path, "rb") as f:
        magic, version, length = struct.unpack("<III", f.read(12))
        chunk_len, chunk_type = struct.unpack("<II", f.read(8))
        json_data = f.read(chunk_len).decode("utf-8")
        data = json.loads(json_data)
    
    # Node names
    node_names = {}
    if "nodes" in data:
        for i, node in enumerate(data["nodes"]):
            name = node.get("name", f"unnamed_{i}")
            node_names[i] = name
            mesh_info = f" mesh={node['mesh']}" if "mesh" in node else ""
            print(f"  Node [{i}]: {name}{mesh_info}")
    
    # Sky-related node names
    sky_terms = ["sky", "environment", "dome", "atmosphere", "world", "cloud", "hdr", "hdri", "sphere"]
    print(f"\n  Sky-related nodes:")
    found_sky = False
    for i, name in node_names.items():
        lower = name.lower()
        for term in sky_terms:
            if term.lower() in lower:
                print(f"    [{i}] \"{name}\" (matches \"{term}\")")
                found_sky = True
                break
    if not found_sky:
        print("    (none)")
    
    # Images
    print(f"\n  Images ({len(data.get('images', []))}):")
    for i, img in enumerate(data.get("images", [])):
        uri = img.get("uri", "embedded")
        name = img.get("name", f"unnamed_{i}")
        print(f"    [{i}] name={name} uri={uri}")
    
    # Materials
    print(f"\n  Materials ({len(data.get('materials', []))}):")
    for i, mat in enumerate(data.get("materials", [])):
        name = mat.get("name", f"unnamed_{i}")
        emissive = mat.get("emissiveFactor", [0,0,0])
        pbr = mat.get("pbrMetallicRoughness", {})
        base_color = pbr.get("baseColorFactor", [1,1,1,1]) if pbr else [1,1,1,1]
        print(f"    [{i}] {name} emissive={emissive} baseColor={base_color}")
    
    # Extensions
    print(f"\n  Extensions used: {data.get('extensionsUsed', [])}")
    
    # Scene
    scene_idx = data.get("scene", 0)
    scenes = data.get("scenes", [])
    if scenes:
        s = scenes[scene_idx] if scene_idx < len(scenes) else scenes[0]
        print(f"  Default scene: \"{s.get('name', 'unnamed')}\" nodes={s.get('nodes', [])}")
