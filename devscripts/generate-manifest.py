"""Generate update manifest with SHA-256 hashes for all game files.
Run after building the game. Output: manifests/{version}.json"""

import hashlib
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST_DIR = os.path.join(ROOT, "manifests")

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()

def load_version():
    path = os.path.join(ROOT, "config", "version.json")
    with open(path) as f:
        return json.load(f)

def build_manifest():
    v = load_version()
    ver_str = f"{v['major']}.{v['minor']}.{v['patch']}"

    # Files to include in manifest (relative to repo root)
    include_patterns = [
        "mimita.exe",
        "MimitaLauncher.exe",
        "version.txt",
        "glfw3.dll",
        "libgcc_s_seh-1.dll",
        "libstdc++-6.dll",
        "libwinpthread-1.dll",
    ]

    # Include all files in assets/
    assets_dir = os.path.join(ROOT, "assets")
    for dirpath, dirnames, filenames in os.walk(assets_dir):
        for fn in filenames:
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, ROOT)
            include_patterns.append(rel)

    # Include default config
    include_patterns.append("config/accounts/default.json")
    include_patterns.append("config/current-profile.json")

    files = []
    for pattern in include_patterns:
        full = os.path.join(ROOT, pattern)
        if not os.path.isfile(full):
            print(f"[WARN] Missing file: {pattern}")
            continue
        size = os.path.getsize(full)
        sha = sha256_file(full)
        files.append({
            "path": pattern.replace("\\", "/"),
            "size": size,
            "sha256": sha
        })
        print(f"  {pattern} ({size} bytes)")

    manifest = {
        "version": ver_str,
        "files": files
    }

    os.makedirs(MANIFEST_DIR, exist_ok=True)
    out_path = os.path.join(MANIFEST_DIR, f"{ver_str}.json")
    with open(out_path, "w") as f:
        json.dump(manifest, f, indent=2)
    print(f"\n[OK] Manifest written: {out_path}")
    print(f"     {len(files)} files, {(sum(f['size'] for f in files) / 1e6):.1f} MB")

if __name__ == "__main__":
    build_manifest()
