"""Create mimita-game.zip of all runtime files.
The launcher downloads this from GitHub on first run.
Excludes MimitaLauncher.exe to avoid file-lock conflicts on extraction.
Refuses to bundle a debug build (huge mimita.exe) unless --allow-debug is passed.
With --launcher-info, also writes launcher_info.json (file:// URLs) so the
launcher's self-update + game-update flow can be tested offline with
--release-json launcher_info.json.
"""

import json
import os
import re
import sys
import zipfile
import hashlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Debug builds embed DWARF symbols (-Og -g) and are ~300+ MB.
# Release builds (-O2 -march=x86-64-v2 -s) are ~15-20 MB.
DEBUG_EXE_SIZE_BYTES = 60 * 1024 * 1024

def check_release_build():
    allow_debug = "--allow-debug" in sys.argv
    exe = os.path.join(ROOT, "mimita.exe")
    if not os.path.isfile(exe):
        print("[FAIL] mimita.exe not found. Build it first (python build_agent.py release).")
        sys.exit(1)
    size = os.path.getsize(exe)
    mb = size / 1e6
    if size > DEBUG_EXE_SIZE_BYTES:
        print(f"[FAIL] mimita.exe is {mb:.1f} MB — this is a DEBUG build.")
        print("       End users must get the release build (~15-20 MB).")
        print("       Build release first:  python build_agent.py release")
        if not allow_debug:
            print("       (or pass --allow-debug to bundle the debug exe anyway)")
            sys.exit(1)
        print("       Bundling debug exe anyway (--allow-debug).")
        return
    print(f"[OK]   mimita.exe is {mb:.1f} MB — release build.")

# Files players do not need, kept out of the release zip:
# - production music work files (AGENTS.md: never committed/shipped)
# - the dev character template
# - Krita autosave files (e.g. textureshq/.gross3.png-autosave.kra)
# - dev/local account + profile data (never ship the maintainer's own
#   profiles.json / accounts / current-profile to players)
EXCLUDE_PREFIXES = (
    "assets/sound/music/ingame/donttrack/",
    "characters/_template/",
    "config/accounts/",
)
EXCLUDE_SUFFIXES = (".kra",)
EXCLUDE_PATHS = ("config/profiles.json", "config/current-profile.json")

# Font source / old-font leftovers that must never ship in the release zip.
# The game only needs the bitmap atlas (noto-serif-cjk-tc-mimita-v1.fnt + pngs).
# MingLiU files were replaced; the .otf is only used to regenerate the atlas.
FONT_EXCLUDE_PREFIXES = (
    "assets/font/mingliu.ttf",
    "assets/font/mingliu-mimita-v3",
    "assets/font/oldmingliu",
    "assets/font/notoserifcjktc-regular.otf",
)
FONT_EXCLUDE_SUFFIXES = (".png~",)


def is_excluded(rel):
    low = rel.lower()
    if low.startswith(EXCLUDE_PREFIXES):
        return True
    # Any donttrack path anywhere (e.g. a stray assets/sound/sound/music/.../donttrack/).
    if "/donttrack/" in low:
        return True
    for s in EXCLUDE_SUFFIXES:
        if low.endswith(s):
            return True
    if low in EXCLUDE_PATHS:
        return True
    if low.startswith(FONT_EXCLUDE_PREFIXES):
        return True
    for s in FONT_EXCLUDE_SUFFIXES:
        if low.endswith(s):
            return True
    # Dev note files (readme-*.txt / readmedonttrack.txt / readme.txt) are not
    # runtime content.
    base = low.rsplit("/", 1)[-1]
    if base.startswith("readme"):
        return True
    return False


def collect_files():
    files = []
    skipped = 0
    for name in ["mimita.exe", "version.txt",
                  "glfw3.dll", "libgcc_s_seh-1.dll", "libstdc++-6.dll",
                  "libwinpthread-1.dll"]:
        full = os.path.join(ROOT, name)
        if os.path.isfile(full):
            files.append((name, full, os.path.getsize(full)))

    for subdir in ["assets", "config", "shaders", "Characters"]:
        d = os.path.join(ROOT, subdir)
        if os.path.isdir(d):
            for dirpath, dirnames, filenames in os.walk(d):
                for fn in sorted(filenames):
                    full = os.path.join(dirpath, fn)
                    rel = os.path.relpath(full, ROOT).replace("\\", "/")
                    if is_excluded(rel):
                        skipped += 1
                        continue
                    files.append((rel, full, os.path.getsize(full)))
    if skipped:
        print(f"[SKIP] excluded {skipped} files (donttrack, _template, .kra)")
    return files

def sha256_of(path):
    sha = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            sha.update(chunk)
    return sha.hexdigest()


def launcher_version():
    src = os.path.join(ROOT, "launcher", "main.cpp")
    try:
        with open(src, "r", encoding="utf-8", errors="replace") as f:
            m = re.search(r'#define\s+LAUNCHER_VERSION\s+"([^"]+)"', f.read())
            if m:
                return m.group(1)
    except OSError:
        pass
    return "0.0.0"


def file_url(path):
    return "file:///" + os.path.abspath(path).replace("\\", "/")


def write_launcher_info(zip_path, zip_sha):
    launcher = os.path.join(ROOT, "MimitaLauncher.exe")
    game_version = "0.0.0"
    vt = os.path.join(ROOT, "version.txt")
    if os.path.isfile(vt):
        with open(vt, "r", encoding="utf-8", errors="replace") as f:
            game_version = f.read().strip() or game_version
    if not os.path.isfile(launcher):
        print("[WARN] MimitaLauncher.exe not found; launcher_sha256 omitted.")
    info = {
        "launcher_version": launcher_version(),
        "game_version": game_version,
        "game_zip_url": file_url(zip_path),
        "game_zip_sha256": zip_sha,
        "launcher_url": file_url(launcher) if os.path.isfile(launcher) else "",
        "launcher_sha256": sha256_of(launcher) if os.path.isfile(launcher) else "",
        "changelog": "",
    }
    out = os.path.join(ROOT, "launcher_info.json")
    with open(out, "w", encoding="utf-8") as f:
        json.dump(info, f, indent=2)
    print(f"[OK]   {out}")
    print(f"       launcher_version={info['launcher_version']} game_version={info['game_version']}")
    return out


def build_zip():
    files = collect_files()
    raw = sum(sz for _, _, sz in files)
    print(f"Collecting {len(files)} files ({raw / 1e6:.1f} MB raw)...")

    zip_path = os.path.join(ROOT, "mimita-game.zip")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for i, (rel, full, _) in enumerate(files):
            zf.write(full, rel)
            if (i + 1) % 100 == 0:
                print(f"  {i + 1}/{len(files)}")

    compressed = os.path.getsize(zip_path)
    ratio = compressed / raw * 100 if raw > 0 else 0
    print(f"\n[OK] {zip_path}")
    print(f"     Raw: {raw / 1e6:.1f} MB")
    print(f"     ZIP: {compressed / 1e6:.1f} MB ({ratio:.0f}%)")

    zip_sha = sha256_of(zip_path)
    print(f"     SHA-256: {zip_sha}")

    if "--launcher-info" in sys.argv:
        write_launcher_info(zip_path, zip_sha)

if __name__ == "__main__":
    check_release_build()
    build_zip()
