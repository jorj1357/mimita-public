# 08 11 2026, 17 14
# purpose
# One interactive console menu for MiMITA builds and release prep.
# Builds debug/release mimita.exe via build_agent.py, MimitaLauncher.exe via
# launcher/build.bat, and mimita-game.zip via devscripts/bundle-game.py, then
# stages the release artifacts into release/<version>/ with hashes, an AV scan
# report, and a launcher_info.json ready for a GitHub release.
# Prints every command before running it, verifies every expected output, and
# stops immediately on the first failed command.
# Does NOT upload, publish, git tag, commit, push, sign, or change Defender
# settings. Does NOT modify build scripts or launcher behavior.

import json
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Reuse the existing sha256 helper from devscripts/bundle-game.py instead of
# duplicating it (AGENTS.md: one source of truth). The module has a hyphen in
# its filename, so it cannot be imported normally — load it by path. It has no
# import-time side effects (its main work is guarded by __main__).
import importlib.util

_bundle_game_spec = importlib.util.spec_from_file_location(
    "bundle_game", os.path.join(os.path.dirname(os.path.abspath(__file__)), "bundle-game.py")
)
_bundle_game = importlib.util.module_from_spec(_bundle_game_spec)
_bundle_game_spec.loader.exec_module(_bundle_game)
sha256_of = _bundle_game.sha256_of

GITHUB_REPO = "jorj1357/mimita-public"
DEFENDER = r"C:\Program Files\Windows Defender\MpCmdRun.exe"

# Strings that must never appear in a release exe (dev-path leaks). Mirrors
# docs/build-release-flow.md and the AV-hardening report.
LEAK_STRINGS = (
    "mimita-priv-v8",
    "C:\\Users",
    "OneDrive",
    "build\\obj",
    "obj-debug",
    "obj-release",
    "devscripts",
    "installer\\",
)


def print_sep():
    print("=" * 60)


def fail(msg):
    print(f"\n[FAIL] {msg}")
    sys.exit(1)


def run(cmd, cwd=ROOT):
    print(f"\n> {cmd if isinstance(cmd, str) else ' '.join(cmd)}")
    print_sep()
    result = subprocess.run(cmd, cwd=cwd)
    if result.returncode != 0:
        fail(f"command exited with code {result.returncode}: "
             f"{cmd if isinstance(cmd, str) else ' '.join(cmd)}")
    return result


def verify_exists(rel_path, min_bytes=0, label=None):
    full = os.path.join(ROOT, rel_path)
    if not os.path.isfile(full):
        fail(f"expected output missing: {rel_path}")
    size = os.path.getsize(full)
    if min_bytes and size < min_bytes:
        fail(f"expected output too small ({size} bytes): {rel_path}")
    print(f"[OK]   {rel_path} ({size / 1e6:.1f} MB)")
    return full


def read_version():
    vt = os.path.join(ROOT, "version.txt")
    if not os.path.isfile(vt):
        fail("version.txt not found. Run: python devscripts/generate-version.py")
    with open(vt, "r", encoding="utf-8", errors="replace") as f:
        ver = f.read().strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", ver):
        fail(f"version.txt does not look like a release version ('{ver}'). "
             "Fix version.txt or run: python devscripts/generate-version.py")
    return ver


def release_dir(ver):
    return os.path.join(ROOT, "release", ver)


def build_lock_check():
    lock = os.path.join(ROOT, "build", "build-agent.lock")
    if not os.path.isfile(lock):
        return
    try:
        with open(lock, "r", encoding="utf-8") as f:
            info = json.load(f)
        pid = int(info.get("pid", 0) or 0)
    except Exception:
        return
    if pid <= 0:
        return
    probe = subprocess.run(
        ["tasklist", "/FI", f"PID eq {pid}", "/FO", "CSV", "/NH"],
        capture_output=True, text=True, timeout=5,
    )
    if f'"{pid}"' in probe.stdout or f",{pid}," in probe.stdout:
        print(f"[LOCK] another build is running (pid={pid}, host={info.get('host', '?')})")
        print("       wait for it to finish, then re-run.")
        sys.exit(1)


def confirm(prompt):
    print(f"{prompt} [y/N] ", end="")
    sys.stdout.flush()
    ans = input().strip().lower()
    return ans in ("y", "yes")


def clean_objects(mode):
    obj = os.path.join(ROOT, "build", f"obj-{mode}")
    if os.path.isdir(obj):
        print(f"[CLEAN] removing {os.path.relpath(obj, ROOT)}")
        shutil.rmtree(obj)


def clean_pch():
    pch = os.path.join(ROOT, "src", "pch.h.gch")
    if os.path.isfile(pch):
        print(f"[CLEAN] removing {os.path.relpath(pch, ROOT)}")
        os.remove(pch)


def build_release(clean_policy):
    if clean_policy == "full":
        clean_objects("release")
        clean_pch()
        stale = os.path.join(ROOT, "mimita-game.zip")
        if os.path.isfile(stale):
            print(f"[CLEAN] removing stale {os.path.relpath(stale, ROOT)}")
            os.remove(stale)
    elif clean_policy == "quick":
        clean_objects("release")
    run([sys.executable, "build_agent.py", "release"])
    verify_exists("mimita.exe")


def build_launcher():
    run(["cmd", "/c", "launcher\\build.bat"])
    verify_exists("MimitaLauncher.exe")


def bundle_zip():
    run([sys.executable, os.path.join("devscripts", "bundle-game.py")])
    verify_exists("mimita-game.zip")


def stage_artifacts(ver):
    rdir = release_dir(ver)
    os.makedirs(rdir, exist_ok=True)
    pairs = [
        ("mimita.exe", "mimita.exe"),
        ("MimitaLauncher.exe", "MimitaLauncher.exe"),
        ("mimita-game.zip", "mimita-game.zip"),
    ]
    for src_rel, dst_rel in pairs:
        src = os.path.join(ROOT, src_rel)
        dst = os.path.join(rdir, dst_rel)
        print(f"[COPY] {src_rel} -> release/{ver}/{dst_rel}")
        shutil.copy2(src, dst)
        verify_exists(os.path.join("release", ver, dst_rel))
    return rdir


def string_leak_scan(exe_path):
    print(f"\n[SCAN] dev-path string scan of {os.path.basename(exe_path)}")
    with open(exe_path, "rb") as f:
        data = f.read()
    leaks = []
    for s in LEAK_STRINGS:
        b = s.encode("ascii", errors="replace")
        if b in data:
            leaks.append(s)
    if leaks:
        print(f"[FAIL] release exe contains leaked strings: {leaks}")
        return False
    print("[OK]   no dev-path leaks found")
    return True


def defender_scan(path):
    if not os.path.isfile(DEFENDER):
        print(f"[WARN] MpCmdRun not found at {DEFENDER} — skipping Defender scan")
        return ("skipped", "MpCmdRun.exe not found")
    print(f"\n> \"{DEFENDER}\" -Scan -ScanType 3 -File \"{path}\"")
    print_sep()
    result = subprocess.run(
        [DEFENDER, "-Scan", "-ScanType", "3", "-File", path],
        capture_output=True, text=True, timeout=300,
    )
    out = (result.stdout or "") + (result.stderr or "")
    threats = re.findall(r"Threats\s*Found:\s*(\d+)", out, re.IGNORECASE)
    found = int(threats[0]) if threats else (0 if result.returncode == 0 else None)
    status = "CLEAN" if found == 0 else f"THREATS FOUND ({found})"
    print(f"[SCAN] {os.path.basename(path)} -> {status} (exit {result.returncode})")
    return (status, out)


def write_release_hashes(rdir, ver):
    lines = []
    for name in ("mimita.exe", "MimitaLauncher.exe", "mimita-game.zip"):
        full = os.path.join(rdir, name)
        size = os.path.getsize(full)
        lines.append(f"{name}  SHA256={sha256_of(full)}  size={size} bytes ({size / 1e6:.1f} MB)")
    out = os.path.join(rdir, "release-hashes.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write(f"MiMITA release hashes — v{ver}\n")
        f.write("\n".join(lines) + "\n")
    print(f"[OK]   {os.path.join('release', ver, 'release-hashes.txt')}")
    return out


def write_av_report(rdir, ver, scan_results):
    out = os.path.join(rdir, "av-scan-report.txt")
    lines = [
        "============================================================",
        f" MiMITA AV SCAN REPORT — v{ver}",
        "============================================================",
    ]
    for name, (status, detail) in scan_results.items():
        lines.append(f"{name}: {status}")
        if status != "CLEAN" and detail and detail != "MpCmdRun.exe not found":
            lines.append(detail.strip())
    lines.append("")
    lines.append("NOTE: read-only Defender custom scan (MpCmdRun -ScanType 3).")
    lines.append("No exclusions or Defender settings were modified.")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print(f"[OK]   {os.path.join('release', ver, 'av-scan-report.txt')}")
    return out


def write_launcher_info(rdir, ver):
    base = f"https://github.com/{GITHUB_REPO}/releases/download/v{ver}"
    zip_sha = sha256_of(os.path.join(rdir, "mimita-game.zip"))
    launcher_sha = sha256_of(os.path.join(rdir, "MimitaLauncher.exe"))
    launcher_ver = "0.0.0"
    src = os.path.join(ROOT, "launcher", "main.cpp")
    with open(src, "r", encoding="utf-8", errors="replace") as f:
        m = re.search(r'#define\s+LAUNCHER_VERSION\s+"([^"]+)"', f.read())
    if m:
        launcher_ver = m.group(1)
    info = {
        "launcher_version": launcher_ver,
        "game_version": ver,
        "game_zip_url": f"{base}/mimita-game.zip",
        "game_zip_sha256": zip_sha,
        "launcher_url": f"{base}/MimitaLauncher.exe",
        "launcher_sha256": launcher_sha,
        "changelog": "",
    }
    out = os.path.join(rdir, "launcher_info.json")
    with open(out, "w", encoding="utf-8") as f:
        json.dump(info, f, indent=2)
    print(f"[OK]   {os.path.join('release', ver, 'launcher_info.json')}")
    return out


def scan_release_artifacts(rdir, ver, write_reports):
    targets = ["mimita.exe", "MimitaLauncher.exe", "mimita-game.zip"]
    exe_ok = True
    for name in targets:
        full = os.path.join(rdir, name)
        if not os.path.isfile(full):
            fail(f"{name} not staged in release/{ver}/ — run a release option first")
    print("\n-- String leak scan (release exe) --")
    exe_ok = string_leak_scan(os.path.join(rdir, "mimita.exe"))
    print("\n-- Defender scan --")
    results = {}
    for name in targets:
        status, detail = defender_scan(os.path.join(rdir, name))
        results[name] = (status, detail)
    if write_reports:
        write_release_hashes(rdir, ver)
        write_av_report(rdir, ver, results)
    if not exe_ok:
        fail("release exe failed the dev-path string scan")
    for name, (status, _) in results.items():
        if status != "CLEAN" and status != "skipped":
            fail(f"Defender scan of {name} did not pass: {status}")


def print_upload_info(ver, rdir):
    print()
    print_sep()
    print(" RELEASE STAGED — READY TO UPLOAD")
    print_sep()
    for name in ("mimita.exe", "MimitaLauncher.exe", "mimita-game.zip",
                 "release-hashes.txt", "av-scan-report.txt", "launcher_info.json"):
        full = os.path.join(rdir, name)
        if os.path.isfile(full):
            print(f"  {full}")
            print(f"    SHA256 {sha256_of(full)}")
    print()
    print("Next step (manual upload — this menu does NOT upload):")
    print(f"  1. python devscripts/publish-release.py")
    print("     (rebuilds from the repo root and publishes the GitHub release)")
    print("  or upload the staged files directly:")
    print(f"     gh release create v{ver} \\")
    print(f"       {os.path.join('release', ver, 'mimita-game.zip')} \\")
    print(f"       {os.path.join('release', ver, 'MimitaLauncher.exe')} \\")
    print(f"       {os.path.join('release', ver, 'launcher_info.json')} \\")
    print(f"       --repo {GITHUB_REPO} --title \"MiMITA v{ver}\" \\")
    print(f"       --notes \"MiMITA v{ver} release.\"")
    print(f"  Website download button already points at:")
    print(f"     https://github.com/{GITHUB_REPO}/releases/latest/download/MimitaLauncher.exe")


def option_debug():
    print_sep()
    print(" OPTION 1 — DEBUG BUILD + LAUNCH")
    print_sep()
    build_lock_check()
    run([sys.executable, "build_agent.py"])
    verify_exists("mimita.exe")
    if "--no-launch" in sys.argv:
        print("\n[OK] debug build complete (--no-launch). Not launching.")
        return
    exe = os.path.join(ROOT, "mimita.exe")
    print("\nLaunching debug mimita.exe...")
    print("(this opens a full graphics window and stays open until you close it)")
    print_sep()
    subprocess.run([exe], cwd=ROOT)


def option_release(clean_policy):
    print_sep()
    print(f" OPTION {3 if clean_policy == 'full' else 2} — RELEASE BUILD ({clean_policy} clean)")
    print_sep()
    build_lock_check()
    ver = read_version()
    build_release(clean_policy)
    build_launcher()
    bundle_zip()
    stage_artifacts(ver)


def option_scan():
    print_sep()
    print(" OPTION 4 — SCAN RELEASE")
    print_sep()
    ver = read_version()
    rdir = release_dir(ver)
    if not os.path.isdir(rdir):
        fail(f"release/{ver}/ does not exist — run option 2, 3, or 5 first")
    scan_release_artifacts(rdir, ver, write_reports=False)


def option_full_prep():
    print_sep()
    print(" OPTION 5 — FULL RELEASE PREP")
    print_sep()
    build_lock_check()
    ver = read_version()
    build_release("full")
    build_launcher()
    bundle_zip()
    rdir = stage_artifacts(ver)
    scan_release_artifacts(rdir, ver, write_reports=True)
    write_launcher_info(rdir, ver)
    print_upload_info(ver, rdir)


def main():
    argv = [a for a in sys.argv[1:] if not a.startswith("--")]
    if argv:
        choice = argv[0]
    else:
        print_sep()
        print(" MiMITA BUILD MENU")
        print_sep()
        print(" 1 = Debug build + launch")
        print(" 2 = Release (quick clean)")
        print(" 3 = Full clean release (AV hardening, slow)")
        print(" 4 = Scan release (Defender + hashes)")
        print(" 5 = Full release prep")
        print(" q = quit")
        print_sep()
        choice = input("Choose: ").strip()

    if choice == "1":
        option_debug()
    elif choice == "2":
        option_release("quick")
    elif choice == "3":
        if "--yes" not in sys.argv:
            if not confirm("Full clean release recompiles ~430 files from scratch. Continue?"):
                print("Cancelled.")
                return
        option_release("full")
    elif choice == "4":
        option_scan()
    elif choice == "5":
        if "--yes" not in sys.argv:
            if not confirm("Full release prep does a full clean build, zip, AV scan, and hashes. Continue?"):
                print("Cancelled.")
                return
        option_full_prep()
    elif choice in ("q", "quit", "exit"):
        print("Goodbye.")
        return
    else:
        fail(f"unknown option: {choice}")


if __name__ == "__main__":
    main()
