#!/usr/bin/env python3
# 08 10 2026, 17 00
# purpose
# One-command release: build the launcher, bundle the game zip, verify hashes,
# write launcher_info.json, and publish a GitHub release (zip + launcher + info).
# Prints the exact download link to put on the website.
# Does NOT touch the VPS, run the game, or require npm install.
# Does NOT commit code unless --commit-push is passed explicitly.

import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = "jorj1357/mimita-public"
LAUNCHER = os.path.join(ROOT, "MimitaLauncher.exe")
ZIP = os.path.join(ROOT, "mimita-game.zip")
INFO = os.path.join(ROOT, "launcher_info.json")
BUILD_BAT = os.path.join(ROOT, "launcher", "build.bat")

DEBUG_EXE_SIZE_BYTES = 60 * 1024 * 1024


def fail(msg):
    print(f"[FAIL] {msg}")
    sys.exit(1)


def run(args, **kw):
    return subprocess.run(args, **kw)


def sha256_of(path):
    import hashlib
    sha = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            sha.update(chunk)
    return sha.hexdigest()


def game_version():
    vt = os.path.join(ROOT, "version.txt")
    if not os.path.isfile(vt):
        fail("version.txt not found. Set the game version there first.")
    with open(vt, "r", encoding="utf-8", errors="replace") as f:
        v = f.read().strip()
    if not v:
        fail("version.txt is empty. Set the game version there first.")
    return v


def launcher_version():
    import re
    src = os.path.join(ROOT, "launcher", "main.cpp")
    with open(src, "r", encoding="utf-8", errors="replace") as f:
        m = re.search(r'#define\s+LAUNCHER_VERSION\s+"([^"]+)"', f.read())
    return m.group(1) if m else "0.0.0"


def check_release_build():
    exe = os.path.join(ROOT, "mimita.exe")
    if not os.path.isfile(exe):
        fail("mimita.exe not found. Build it first: python build_agent.py release")
    size = os.path.getsize(exe)
    if size > DEBUG_EXE_SIZE_BYTES:
        fail(f"mimita.exe is {size / 1e6:.1f} MB — this is a DEBUG build. "
             "End users must get the release build (~15-20 MB). Run: python build_agent.py release")


def build_launcher():
    print("== Building MimitaLauncher ==")
    r = run(["cmd", "/c", "launcher\\build.bat"], cwd=ROOT)
    if r.returncode != 0:
        fail("launcher build failed")
    if not os.path.isfile(LAUNCHER):
        fail("MimitaLauncher.exe not produced by build.bat")


def bundle_zip():
    print("== Bundling mimita-game.zip ==")
    r = run([sys.executable, os.path.join("devscripts", "bundle-game.py")], cwd=ROOT)
    if r.returncode != 0:
        fail("bundle-game.py failed")
    if not os.path.isfile(ZIP):
        fail("mimita-game.zip not produced")


def write_launcher_info(ver):
    zip_sha = sha256_of(ZIP)
    launcher_sha = sha256_of(LAUNCHER)
    base = f"https://github.com/{REPO}/releases/download/v{ver}"
    info = {
        "launcher_version": launcher_version(),
        "game_version": ver,
        "game_zip_url": f"{base}/mimita-game.zip",
        "game_zip_sha256": zip_sha,
        "launcher_url": f"{base}/MimitaLauncher.exe",
        "launcher_sha256": launcher_sha,
        "changelog": "",
    }
    with open(INFO, "w", encoding="utf-8") as f:
        json.dump(info, f, indent=2)
    print(f"[OK]   {INFO}")
    return info


def publish(ver, notes):
    if os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN"):
        pass
    else:
        r = run(["gh", "auth", "status"])
        if r.returncode != 0:
            fail("GitHub CLI is not authenticated. Run `gh auth login` or set GH_TOKEN.")

    tag = f"v{ver}"
    title = f"MiMITA v{ver}"
    assets = [ZIP, LAUNCHER, INFO]

    print(f"== Publishing release {tag} to {REPO} ==")
    r = run(["gh", "release", "view", tag, "--repo", REPO],
            capture_output=True, text=True)
    exists = r.returncode == 0
    if exists:
        print(f"   release {tag} exists — uploading/clobbering assets")
        r = run(["gh", "release", "upload", tag, *assets,
                 "--repo", REPO, "--clobber"])
        if r.returncode != 0:
            fail("gh release upload failed")
        r = run(["gh", "release", "edit", tag, "--repo", REPO,
                 "--title", title, "--notes", notes])
        if r.returncode != 0:
            fail("gh release edit failed")
    else:
        r = run(["gh", "release", "create", tag, *assets,
                 "--repo", REPO, "--title", title, "--notes", notes])
        if r.returncode != 0:
            fail("gh release create failed")

    print(f"\n[OK] Published v{ver}")
    print(f"     Game ZIP SHA-256: {sha256_of(ZIP)}")
    print(f"     Launcher SHA-256: {sha256_of(LAUNCHER)}")
    print(f"     Website download link (paste into the site's download button):")
    print(f"     https://github.com/{REPO}/releases/latest/download/MimitaLauncher.exe")


def commit_push(ver):
    print("== Committing version bump ==")
    files = ["version.txt", "config/version.json", "launcher/main.cpp"]
    r = run(["git", "add", "--"] + files, cwd=ROOT)
    if r.returncode != 0:
        fail("git add failed")
    r = run(["git", "commit", "-m", f"release: v{ver}", "--"] + files, cwd=ROOT)
    if r.returncode != 0:
        print("   (nothing to commit or commit failed — continuing)")
    r = run(["git", "push"], cwd=ROOT)
    if r.returncode != 0:
        fail("git push failed")


def main():
    notes = "MiMITA v" + game_version() + " release."
    if "--commit-push" in sys.argv:
        pass
    ver = game_version()
    check_release_build()
    build_launcher()
    bundle_zip()
    info = write_launcher_info(ver)
    if "--notes" in sys.argv:
        i = sys.argv.index("--notes")
        if i + 1 < len(sys.argv):
            notes = sys.argv[i + 1]
    if info["changelog"]:
        notes = f"{notes}\n\n{info['changelog']}"
    publish(ver, notes)
    if "--commit-push" in sys.argv:
        commit_push(ver)
    print("\n[PUBLISH-RELEASE] DONE")


if __name__ == "__main__":
    main()
