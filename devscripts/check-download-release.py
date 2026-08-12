# 08 11 2026, 18 56
# purpose
# Dry-run checker for the MiMITA download/release path. Explains, without
# deploying anything, what mimita.fun/download resolves to, what GitHub
# release/tag/assets the launcher expects, whether release/2.0.1 filenames and
# launcher_info.json match that expectation, and what exact command would
# publish — without running it.
# Does NOT contact the network, upload, publish, deploy, push, tag, or sign.
# Requires: staged release/2.0.1/ artifacts.

import importlib.util
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION = "2.0.2"
RDIR = os.path.join(ROOT, "release", VERSION)
GITHUB_REPO = "jorj1357/mimita-public"
ASSETS = ("mimita.exe", "MimitaLauncher.exe", "mimita-game.zip", "launcher_info.json")

PASS = 0
FAIL = 0


def _load_module(name):
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), name + ".py")
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


sha256_of = _load_module("bundle-game").sha256_of


def check(name, cond, detail=""):
    global PASS, FAIL
    if cond:
        PASS += 1
        print(f"[PASS] {name}")
    else:
        FAIL += 1
        print(f"[FAIL] {name}" + (f" — {detail}" if detail else ""))


def print_sep(title):
    print("=" * 60)
    print(" " + title)
    print("=" * 60)


def check_sources():
    print_sep("1+2. Download button, server redirect, launcher expectations")
    try:
        with open(os.path.join(ROOT, "website", "src", "pages", "Download.jsx"),
                  encoding="utf-8", errors="replace") as f:
            check("Download.jsx points the button at /api/download/latest",
                  "/api/download/latest" in f.read())
    except OSError as e:
        check("Download.jsx readable", False, str(e))
    try:
        with open(os.path.join(ROOT, "website", "server", "server.js"),
                  encoding="utf-8", errors="replace") as f:
            server = f.read()
        redirect = re.search(r'res\.redirect\("(https://[^"]+)"\)', server)
        if redirect:
            print("  /api/download/latest redirects to: " + redirect.group(1))
            check("Redirect targets the launcher asset",
                  "MimitaLauncher.exe" in redirect.group(1))
        check("Route never serves a local installer (MimitaSetup)",
              "MimitaSetup" not in server)
    except OSError as e:
        check("server.js readable", False, str(e))
    print("  GitHub repo: " + GITHUB_REPO + "  tag: v" + VERSION)
    print("  Assets: mimita-game.zip, MimitaLauncher.exe, launcher_info.json")
    try:
        with open(os.path.join(ROOT, "launcher", "main.cpp"),
                  encoding="utf-8", errors="replace") as f:
            launcher_src = f.read()
        api_url = re.search(r'#define\s+RELEASE_API_URL\s+"([^"]+)"', launcher_src)
        repo = re.search(r'#define\s+GITHUB_REPO\s+"([^"]+)"', launcher_src)
        print("  Launcher RELEASE_API_URL:",
              api_url.group(1) if api_url else "(missing)")
        print("  Launcher GITHUB_REPO:    ",
              repo.group(1) if repo else "(missing)")
        check("Launcher repo matches", repo and repo.group(1) == GITHUB_REPO)
        check("Launcher expects release/latest API",
              api_url and "releases/latest" in api_url.group(1))
    except OSError as e:
        check("launcher/main.cpp readable", False, str(e))


def staged_hashes():
    hashes = {}
    for name in ASSETS[:3]:
        full = os.path.join(RDIR, name)
        if os.path.isfile(full):
            hashes[name] = sha256_of(full)
            print(f"  {name}  SHA256={hashes[name]}  size={os.path.getsize(full)} bytes")
    return hashes


def check_release_contents():
    print_sep("3. release/2.0.1 contents + hashes")
    for name in ASSETS:
        check(f"release/{VERSION}/{name} present",
              os.path.isfile(os.path.join(RDIR, name)))
    hashes = staged_hashes()
    rel_hashes = os.path.join(RDIR, "release-hashes.txt")
    if os.path.isfile(rel_hashes):
        with open(rel_hashes, encoding="utf-8", errors="replace") as f:
            for line in f:
                m = re.search(r"(\S+\.(?:exe|zip))\s+SHA256=([0-9a-f]{64})", line)
                if m:
                    check(f"release-hashes.txt matches {m.group(1)}",
                          hashes.get(m.group(1)) == m.group(2))
    else:
        check("release-hashes.txt present", False)
    return hashes


def check_zip(hashes):
    print_sep("4. mimita-game.zip contents")
    zip_path = os.path.join(RDIR, "mimita-game.zip")
    if not os.path.isfile(zip_path):
        check("mimita-game.zip present", False)
        return
    import zipfile
    with zipfile.ZipFile(zip_path) as zf:
        names = zf.namelist()
        check("zip contains mimita.exe at root", "mimita.exe" in names)
        check("zip contains version.txt at root", "version.txt" in names)
        for sub in ("assets/", "config/", "shaders/", "Characters/"):
            check(f"zip contains {sub}", any(n.startswith(sub) for n in names))
        check("zip does NOT contain MimitaLauncher.exe",
              not any("MimitaLauncher.exe" in n for n in names))
        bad = [n for n in names
               if n.lower().startswith("assets/sound/music/ingame/donttrack/")
               or n.lower().startswith("characters/_template/")
               or n.lower().startswith("config/accounts/")
               or n.lower() in ("config/profiles.json", "config/current-profile.json")
               or n.lower().endswith(".kra")]
        check("zip excludes dev-only content", not bad, "; ".join(bad[:5]))
        vt = zf.read("version.txt").decode("utf-8", errors="replace").strip()
        check("zip version.txt == " + VERSION, vt == VERSION, f"got '{vt}'")
        exe_size = zf.getinfo("mimita.exe").file_size if "mimita.exe" in names else 0
        check("zip mimita.exe is a release build", exe_size < 60 * 1024 * 1024,
              f"{exe_size/1e6:.1f} MB")


def check_launcher_info(hashes):
    print_sep("5. launcher_info.json consistency")
    info_path = os.path.join(RDIR, "launcher_info.json")
    if not os.path.isfile(info_path):
        check("launcher_info.json present", False)
        return
    with open(info_path, encoding="utf-8") as f:
        info = json.load(f)
    check("launcher_info.game_version == " + VERSION,
          info.get("game_version") == VERSION, info.get("game_version"))
    base = f"https://github.com/{GITHUB_REPO}/releases/download/v{VERSION}"
    check("launcher_info.game_zip_url matches tag",
          info.get("game_zip_url") == f"{base}/mimita-game.zip")
    check("launcher_info.launcher_url matches tag",
          info.get("launcher_url") == f"{base}/MimitaLauncher.exe")
    check("launcher_info.zip hash matches staged zip",
          info.get("game_zip_sha256") == hashes.get("mimita-game.zip"))
    check("launcher_info.launcher hash matches staged launcher",
          info.get("launcher_sha256") == hashes.get("MimitaLauncher.exe"))


def check_site_version():
    print_sep("6. Website version metadata")
    try:
        with open(os.path.join(ROOT, "website", "server", "version.json"),
                  encoding="utf-8") as f:
            site_ver = json.load(f)
        check("website/server/version.json version == " + VERSION,
              site_ver.get("version") == VERSION, str(site_ver.get("version")))
    except OSError as e:
        check("website/server/version.json readable", False, str(e))


def print_publish(ok):
    print_sep("RESULT")
    print(f"  PASS: {PASS}   FAIL: {FAIL}")
    print("  " + ("READY TO PUBLISH (once approved)" if ok else "FIX ISSUES BEFORE PUBLISHING"))
    print_sep("EXACT PUBLISH COMMAND (NOT RUN)")
    print("  python devscripts/publish-release.py")
    print()
    print("  This builds the launcher + zip from the repo root, writes")
    print("  launcher_info.json, and creates/uploads the GitHub release")
    print("  v%s with the three assets." % VERSION)
    print("  Website button target stays:")
    print("  https://github.com/%s/releases/latest/download/MimitaLauncher.exe" % GITHUB_REPO)
    print()
    print("[CHECK-DOWNLOAD-RELEASE] " + ("PASS" if ok else "FAIL"))


def main():
    print_sep(f"DOWNLOAD / RELEASE DRY-RUN (v{VERSION}, no deploy)")
    print("  Staged artifacts: " + RDIR)
    check_sources()
    hashes = check_release_contents()
    check_zip(hashes)
    check_launcher_info(hashes)
    check_site_version()
    ok = FAIL == 0
    print_publish(ok)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
