"""Generate version files from config/version.json (single source of truth)."""

import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def load_version():
    with open(os.path.join(ROOT, "config", "version.json")) as f:
        return json.load(f)

def ver_str(v):
    return f"{v['major']}.{v['minor']}.{v['patch']}"

def write_version_h(v):
    s = ver_str(v)
    path = os.path.join(ROOT, "src", "game", "version.h")
    with open(path, "w") as f:
        f.write(f"""#pragma once

#define MIMITA_VERSION_MAJOR {v['major']}
#define MIMITA_VERSION_MINOR {v['minor']}
#define MIMITA_VERSION_PATCH {v['patch']}

#define MIMITA_VERSION_STRING "{s}"
""")
    print(f"[GEN] {os.path.relpath(path, ROOT)}")

def write_version_txt(v):
    path = os.path.join(ROOT, "version.txt")
    with open(path, "w") as f:
        f.write(ver_str(v) + "\n")
    print(f"[GEN] {os.path.relpath(path, ROOT)}")

def write_server_config(v):
    path = os.path.join(ROOT, "website", "server", "version.json")
    with open(path, "w") as f:
        json.dump({
            "version": ver_str(v),
            "release_date": v["release_date"],
            "file_size_mb": 107,
            "platform": "windows-64"
        }, f, indent=2)
        f.write("\n")
    print(f"[GEN] {os.path.relpath(path, ROOT)}")

def update_iss(v):
    s = ver_str(v)
    iss_path = os.path.join(ROOT, "installer", "setup.iss")
    with open(iss_path, "r") as f:
        content = f.read()
    old_line = None
    for line in content.splitlines():
        if line.startswith("#define MyAppVersion "):
            old_line = line.strip()
            break
    if old_line:
        new_line = f'#define MyAppVersion "{s}"'
        content = content.replace(old_line, new_line)
        with open(iss_path, "w") as f:
            f.write(content)
        print(f"[GEN] {os.path.relpath(iss_path, ROOT)}")

if __name__ == "__main__":
    v = load_version()
    write_version_h(v)
    write_version_txt(v)
    write_server_config(v)
    update_iss(v)
    print(f"[OK] Version {ver_str(v)} generated")
