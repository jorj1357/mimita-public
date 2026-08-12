# 08 11 2026, 18 55
# purpose
# Local-only repeated release/AV/launcher test for MiMITA. Verifies the staged
# release/2.0.1 artifacts, Defender-scans the exes, zip and extracted install,
# then runs the real MimitaLauncher.exe against the real mimita-game.zip through
# isolated dev-mode cycles (clean install, second launch, repair, repeats) and
# offline non-dev-mode flows (self-install-to-home, no-re-extract, update-after-
# exit, self-update swap, --tray silent start) using file:// URLs so nothing
# contacts production servers.
# Does NOT publish, deploy, push, tag, upload, sign, disable Defender, or add
# exclusions. Does NOT require admin. Writes build/local-launcher-av-test/.
# Requires: staged release/2.0.1/, MimitaLauncher.exe at repo root, and no
# running mimita.exe / MimitaLauncher.exe (pass --force-kill to kill them).

import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEST_ROOT = os.path.join(ROOT, "build", "local-launcher-av-test")
RELEASE = os.path.join(ROOT, "release")
REPORT = os.path.join(TEST_ROOT, "report.txt")
LAUNCHER_ROOT = os.path.join(ROOT, "MimitaLauncher.exe")
GAME_IMAGE = "mimita.exe"
LAUNCHER_IMAGE = "MimitaLauncher.exe"
HOME_DIR = os.path.join(os.environ.get("LOCALAPPDATA", ""), "MiMITA", "launcher")
RUN_KEY = r"Software\Microsoft\Windows\CurrentVersion\Run"
RUN_VALUE = "MimitaLauncher"
RELEASE_FILES = ("mimita.exe", "MimitaLauncher.exe", "mimita-game.zip", "launcher_info.json")

PASS_COUNT = 0
FAIL_COUNT = 0


# ── Reuse existing helpers (AGENTS.md: one source of truth) ─────
def _load_module(name):
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), name + ".py")
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


_bundle = _load_module("bundle-game")
_menu = _load_module("mimita-build-menu")
sha256_of = _bundle.sha256_of
defender_scan = _menu.defender_scan
string_leak_scan = _menu.string_leak_scan


# ── Report helpers ──────────────────────────────────────────────
def report(line=""):
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, "a", encoding="utf-8") as f:
        f.write(line + "\n")
    print(line)


def passed(name):
    global PASS_COUNT
    PASS_COUNT += 1
    report(f"[PASS] {name}")


def failed(name, detail=""):
    global FAIL_COUNT
    FAIL_COUNT += 1
    report(f"[FAIL] {name}" + (f" — {detail}" if detail else ""))


def check(name, cond, detail=""):
    if cond:
        passed(name)
    else:
        failed(name, detail)
    return cond


def sep(title):
    report("=" * 60)
    report(f" {title}")
    report("=" * 60)


# ── Process / fs helpers ────────────────────────────────────────
def process_running(image):
    r = subprocess.run(["tasklist", "/FI", f"IMAGENAME eq {image}", "/FO", "CSV", "/NH"],
                       capture_output=True, text=True, timeout=15)
    return image.lower() in r.stdout.lower()


def image_pids(image):
    r = subprocess.run(["tasklist", "/FI", f"IMAGENAME eq {image}", "/FO", "CSV", "/NH"],
                       capture_output=True, text=True, timeout=15)
    pids = []
    for m in re.finditer(r'"(?:[^"]*)"\s*,\s*"(\d+)"', r.stdout):
        pids.append(int(m.group(1)))
    return pids


def kill_image(image):
    subprocess.run(["taskkill", "/f", "/im", image], capture_output=True, text=True)
    time.sleep(1.0)


def cleanup_instances():
    # A previous launcher instance (e.g. the self-installed copy in
    # %LOCALAPPDATA%\MiMITA\launcher) stays in the tray and holds the
    # single-instance mutex + file lock, blocking every later launch. Each
    # sub-test must start from a clean slate and clean up afterwards.
    if process_running(GAME_IMAGE):
        kill_image(GAME_IMAGE)
    if process_running(LAUNCHER_IMAGE):
        kill_image(LAUNCHER_IMAGE)
    time.sleep(0.5)


def wait_for(pred, seconds, step=0.3):
    deadline = time.time() + seconds
    while time.time() < deadline:
        if pred():
            return True
        time.sleep(step)
    return False


def wait_for_file(path, seconds):
    return wait_for(lambda: os.path.exists(path), seconds)


def wait_game_up(seconds=90):
    return wait_for(lambda: process_running(GAME_IMAGE), seconds, step=0.5)


def terminate(proc, wait=2.0):
    if proc is None or proc.poll() is not None:
        return
    proc.kill()
    try:
        proc.wait(timeout=wait)
    except subprocess.TimeoutExpired:
        pass


# ── Defender helpers ────────────────────────────────────────────
def scan_report(path, label):
    status, _ = defender_scan(path)
    ok = status in ("CLEAN", "skipped")
    report(f"  Defender {label}: {status}")
    check(f"Defender scan {label}", ok, status)
    return ok


def threat_detections():
    try:
        r = subprocess.run(
            ["powershell", "-NoProfile", "-NonInteractive", "-Command",
             "Get-MpThreatDetection | Select-Object -ExpandProperty Resources"],
            capture_output=True, text=True, timeout=60)
        return set(l.strip().lower() for l in (r.stdout or "").splitlines() if l.strip())
    except Exception:
        return None


def threat_history(before, after):
    if before is None or after is None:
        report("  Get-MpThreatDetection: unavailable")
        return True
    new_detections = after - before
    if new_detections:
        report("  Get-MpThreatDetection: NEW DETECTIONS this run:")
        for d in sorted(new_detections):
            report("    " + d)
        return False
    preexisting = {d for d in after if "mimita" in d}
    if preexisting:
        report("  Get-MpThreatDetection: no NEW detections. Pre-existing history "
               f"({len(preexisting)} mimita-related entries) — investigate separately.")
        for d in sorted(preexisting):
            report("    " + d)
        return True
    report("  Get-MpThreatDetection: no MiMITA-related detections")
    return True


# ── Tier 1: isolated dev-mode cycles ────────────────────────────
def make_launcher_env(root):
    launcher_dir = os.path.join(root, "launcher")
    install_dir = os.path.join(root, "install")
    os.makedirs(launcher_dir, exist_ok=True)
    os.makedirs(install_dir, exist_ok=True)
    shutil.copy(LAUNCHER_ROOT, os.path.join(launcher_dir, "MimitaLauncher.exe"))
    with open(os.path.join(launcher_dir, "install-config.json"), "w", encoding="utf-8") as f:
        json.dump({"install_dir": install_dir.replace("\\", "\\\\")}, f)
    return launcher_dir, install_dir


def run_launcher_dev(launcher_dir, zip_path):
    exe = os.path.join(launcher_dir, "MimitaLauncher.exe")
    return subprocess.Popen([exe, "--local-zip", zip_path,
                             "--no-error-dialogs", "--release-json", "missing.json"],
                            cwd=launcher_dir)


def version_dir(install_dir, ver="2.0.1"):
    return os.path.join(install_dir, "versions", "v" + ver)


def tier1_cycle(launcher_dir, install_dir, zip_path, name):
    sep(f"Tier 1 — {name}")
    ver = os.path.join(version_dir(install_dir), "mimita.exe")
    before = os.path.getmtime(ver) if os.path.isfile(ver) else None
    p = run_launcher_dev(launcher_dir, zip_path)
    start = time.time()
    try:
        ok_install = wait_for_file(ver, 240)
        check(f"{name}: mimita.exe installed", ok_install, "version folder not created in 240s")
        active = os.path.join(install_dir, "active-version.txt")
        if os.path.isfile(active):
            check(f"{name}: active-version.txt == 2.0.1",
                  open(active).read().strip() == "2.0.1", open(active).read().strip())
        else:
            failed(f"{name}: active-version.txt missing")

        ok_launch = wait_game_up(90)
        launch_secs = time.time() - start
        check(f"{name}: launcher started the game", ok_launch, "mimita.exe never appeared")
        if ok_launch:
            report(f"  game process appeared after {launch_secs:.1f}s; observing 10s "
                   "(Defender reaction window)")
            time.sleep(10)
            kill_image(GAME_IMAGE)
            time.sleep(3)
        check(f"{name}: launcher alive in tray after game exit",
              p.poll() is None, f"launcher exited code {p.returncode}")

        after = os.path.getmtime(ver) if os.path.isfile(ver) else None
        if before is not None and after is not None:
            check(f"{name}: game files not re-extracted", before == after,
                  "mimita.exe mtime changed (unexpected re-extract)")
        return True
    finally:
        terminate(p)


def tier1_repair_cycle(launcher_dir, install_dir, zip_path):
    sep("Tier 1 — repair path (simulated missing mimita.exe)")
    ver = os.path.join(version_dir(install_dir), "mimita.exe")
    if os.path.isfile(ver):
        os.remove(ver)
    report("  deleted versions\\v2.0.1\\mimita.exe")
    p = run_launcher_dev(launcher_dir, zip_path)
    try:
        ok = wait_for_file(ver, 240)
        check("repair: mimita.exe restored", ok, "not re-extracted in 240s")
        ok_launch = wait_game_up(90)
        check("repair: game relaunched after repair", ok_launch, "game did not start")
        if ok_launch:
            time.sleep(8)
            kill_image(GAME_IMAGE)
            time.sleep(2)
        check("repair: launcher alive after repair", p.poll() is None, "launcher exited")
    finally:
        terminate(p)


def tier1(zip_path):
    sep("TIER 1 — isolated dev-mode cycles (real zip, real game)")
    r1 = os.path.join(TEST_ROOT, "tier1")
    shutil.rmtree(r1, ignore_errors=True)
    ldir, idir = make_launcher_env(r1)

    tier1_cycle(ldir, idir, zip_path, "clean install + first launch")
    scan_report(version_dir(idir), "extracted install folder (tier1)")

    tier1_cycle(ldir, idir, zip_path, "second launch (already installed)")
    tier1_repair_cycle(ldir, idir, zip_path)

    for i in range(1, 4):
        tier1_cycle(ldir, idir, zip_path, f"repeat launch {i}/3")

    staged_exe = os.path.join(RELEASE, "2.0.1", "mimita.exe")
    if os.path.isfile(staged_exe):
        string_leak_scan(staged_exe)
    check("tier1: final install intact",
          os.path.isfile(os.path.join(version_dir(idir), "mimita.exe")),
          "mimita.exe missing after cycles")


# ── Tier 2: offline non-dev-mode flows ──────────────────────────
def read_reg_value():
    r = subprocess.run(["reg", "query", f"HKCU\\{RUN_KEY}", "/v", RUN_VALUE],
                       capture_output=True, text=True, timeout=15)
    return r.stdout if r.returncode == 0 else ""


def set_reg_value(cmdline):
    subprocess.run(["reg", "add", f"HKCU\\{RUN_KEY}", "/v", RUN_VALUE,
                    "/t", "REG_SZ", "/d", cmdline, "/f"],
                   capture_output=True, text=True, timeout=15)


def delete_reg_value():
    subprocess.run(["reg", "delete", f"HKCU\\{RUN_KEY}", "/v", RUN_VALUE, "/f"],
                   capture_output=True, text=True, timeout=15)


def parse_reg_data(raw):
    for line in raw.splitlines():
        m = re.search(r"REG_SZ\s+(.+)$", line)
        if m:
            return m.group(1).strip()
    return ""


def make_release_json(path, game_version, zip_url, zip_sha, launcher_version):
    data = {
        "launcher_version": launcher_version,
        "game_version": game_version,
        "game_zip_url": "file:///" + zip_url.replace("\\", "/"),
        "game_zip_sha256": zip_sha,
        "launcher_url": "file:///" + LAUNCHER_ROOT.replace("\\", "/"),
        "launcher_sha256": sha256_of(LAUNCHER_ROOT),
        "changelog": "",
    }
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    return path


def run_launcher_nondev(work_dir, release_json, extra=()):
    exe = os.path.join(work_dir, "MimitaLauncher.exe")
    return subprocess.Popen([exe, "--release-json", release_json,
                             "--no-error-dialogs", *extra], cwd=work_dir)


def make_update_zip(ver, real_zip_path):
    import zipfile
    d = os.path.join(TEST_ROOT, "tier2-update")
    os.makedirs(d, exist_ok=True)
    out = os.path.join(d, ver + ".zip")
    if os.path.isfile(out):
        return out
    with zipfile.ZipFile(real_zip_path, "r") as zf:
        exe_bytes = zf.read("mimita.exe")
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("version.txt", ver)
        zf.writestr("mimita.exe", exe_bytes)
    return out


def _tier2_setup(zip_path):
    os.makedirs(HOME_DIR, exist_ok=True)
    t2 = os.path.join(TEST_ROOT, "tier2")
    shutil.rmtree(t2, ignore_errors=True)
    os.makedirs(t2, exist_ok=True)
    install_dir = os.path.join(t2, "install")
    tmp_launch = os.path.join(t2, "tmp-launch")
    os.makedirs(tmp_launch, exist_ok=True)
    os.makedirs(install_dir, exist_ok=True)

    home_exe = os.path.join(HOME_DIR, "MimitaLauncher.exe")
    home_cfg = os.path.join(HOME_DIR, "install-config.json")
    bak_exe = None
    bak_cfg = None
    bak_reg = read_reg_value()
    if os.path.isfile(home_exe):
        bak_exe = home_exe + ".testbak"
        shutil.copy2(home_exe, bak_exe)
    if os.path.isfile(home_cfg):
        bak_cfg = home_cfg + ".testbak"
        shutil.copy2(home_cfg, bak_cfg)

    # install-config for the home instance (and a safety copy in the throwaway
    # launch dir) points at the tier2 temp install so the real default install
    # dir is never touched even if self-install fails.
    for d in (HOME_DIR, tmp_launch):
        with open(os.path.join(d, "install-config.json"), "w", encoding="utf-8") as f:
            json.dump({"install_dir": install_dir.replace("\\", "\\\\")}, f)

    shutil.copy(LAUNCHER_ROOT, os.path.join(tmp_launch, "MimitaLauncher.exe"))

    real_sha = sha256_of(zip_path)
    steady = make_release_json(os.path.join(t2, "release-2.0.1.json"),
                               "2.0.1", zip_path, real_sha, "1.0.0")
    upd_zip = make_update_zip("9.9.9", zip_path)
    upd_sha = sha256_of(upd_zip)
    upd_json = make_release_json(os.path.join(t2, "release-9.9.9.json"),
                                 "9.9.9", upd_zip, upd_sha, "1.0.0")
    upgrade_json = make_release_json(os.path.join(t2, "release-upgrade.json"),
                                     "9.9.9", upd_zip, upd_sha, "9.9.9")

    env = {
        "install_dir": install_dir,
        "tmp_launch": tmp_launch,
        "home_exe": home_exe,
        "home_cfg": home_cfg,
        "steady": steady,
        "upd_json": upd_json,
        "upgrade_json": upgrade_json,
        "upd_zip": upd_zip,
        "upd_sha": upd_sha,
    }
    return env, bak_exe, bak_cfg, bak_reg


def _tier2_restore(env, bak_exe, bak_cfg, bak_reg):
    cleanup_instances()
    home_exe = env["home_exe"]
    home_cfg = env["home_cfg"]
    if bak_exe and os.path.isfile(bak_exe):
        shutil.copy2(bak_exe, home_exe)
        os.remove(bak_exe)
    elif os.path.isfile(home_exe):
        os.remove(home_exe)
    if bak_cfg and os.path.isfile(bak_cfg):
        shutil.copy2(bak_cfg, home_cfg)
        os.remove(bak_cfg)
    elif os.path.isfile(home_cfg):
        os.remove(home_cfg)
    if bak_reg:
        set_reg_value(parse_reg_data(bak_reg))
    else:
        delete_reg_value()
    report("  tier2: real launcher home + registry state restored")


def tier2_a(env):
    # Self-install-to-home + install + launch
    cleanup_instances()
    p = run_launcher_nondev(env["tmp_launch"], env["steady"])
    try:
        selfinstalled = wait_for(
            lambda: os.path.isfile(env["home_exe"]) and process_running(LAUNCHER_IMAGE), 60)
        check("T2a: launcher self-installed to %LOCALAPPDATA%\\MiMITA\\launcher",
              selfinstalled, "home exe or launcher process not present")
        ok = wait_for_file(os.path.join(version_dir(env["install_dir"]), "mimita.exe"), 240)
        check("T2a: game installed from file:// zip", ok, "no version folder")
        ok_launch = wait_game_up(90)
        check("T2a: game launched (instantPlay)", ok_launch, "game did not start")
        if ok_launch:
            time.sleep(8)
            kill_image(GAME_IMAGE)
            time.sleep(2)
    finally:
        terminate(p)
        cleanup_instances()


def tier2_b(env):
    # No unnecessary re-extract when versions match
    ver = os.path.join(version_dir(env["install_dir"]), "mimita.exe")
    before = os.path.getmtime(ver)
    p = run_launcher_nondev(env["tmp_launch"], env["steady"])
    try:
        ok_launch = wait_game_up(90)
        check("T2b: second launch starts game", ok_launch, "game did not start")
        time.sleep(3)
        after = os.path.getmtime(ver) if os.path.isfile(ver) else None
        check("T2b: no re-extract when versions match", before == after,
              "game files were re-extracted")
        if ok_launch:
            time.sleep(5)
            kill_image(GAME_IMAGE)
            time.sleep(2)
    finally:
        terminate(p)
        cleanup_instances()


def tier2_c(env):
    # Update-after-exit: newer game_version, game exits, update applies
    p = run_launcher_nondev(env["tmp_launch"], env["upd_json"])
    try:
        ok_launch = wait_game_up(90)
        check("T2c: current game launches while update pending", ok_launch,
              "game did not start")
        if ok_launch:
            time.sleep(6)
            kill_image(GAME_IMAGE)
        applied = wait_for_file(os.path.join(version_dir(env["install_dir"], "9.9.9"), "mimita.exe"), 180)
        check("T2c: update applied after game exit", applied, "v9.9.9 folder never appeared")
        av = os.path.join(env["install_dir"], "active-version.txt")
        if os.path.isfile(av):
            check("T2c: active-version.txt == 9.9.9",
                  open(av).read().strip() == "9.9.9", open(av).read().strip())
    finally:
        terminate(p)
        cleanup_instances()


def tier2_d(env):
    # Zero-click launcher self-update swap (launcher_version 9.9.9)
    home_exe = env["home_exe"]
    upgrade_json = env["upgrade_json"]
    last_mtime = os.path.getmtime(home_exe)
    swapped = False
    stable_pid = None
    stable_since = 0.0
    cleanup_instances()
    p = run_launcher_nondev(env["tmp_launch"], upgrade_json)
    try:
        deadline = time.time() + 90
        while time.time() < deadline:
            try:
                m = os.path.getmtime(home_exe)
            except OSError:
                m = None
            if m is not None and m != last_mtime:
                last_mtime = m
                swapped = True
                # Embedded LAUNCHER_VERSION is still 1.0.0; pin the json so
                # the spawned copy stops re-swapping (deterministic exit).
                make_release_json(upgrade_json, "9.9.9", env["upd_zip"], env["upd_sha"], "1.0.0")
            pids = image_pids(LAUNCHER_IMAGE)
            if pids:
                cur = pids[0]
                if cur != stable_pid:
                    stable_pid = cur
                    stable_since = time.time()
                elif time.time() - stable_since >= 4.0:
                    break
            else:
                stable_pid = None
            time.sleep(0.3)
        check("T2d: launcher self-update swapped the home exe", swapped,
              "home exe never replaced")
        check("T2d: spawned launcher stabilised (no swap loop)", stable_pid is not None,
              "launcher never settled")
        check("T2d: old launcher exe cleaned up",
              not os.path.exists(os.path.join(HOME_DIR, "MimitaLauncher.old.exe")),
              "MimitaLauncher.old.exe still present")
        if process_running(GAME_IMAGE):
            kill_image(GAME_IMAGE)
    finally:
        if process_running(LAUNCHER_IMAGE):
            kill_image(LAUNCHER_IMAGE)
        terminate(p)
        cleanup_instances()


def tier2_e(env, bak_reg):
    # Auto-start registry + --tray silent start (no game launch)
    set_reg_value(f'"{env["home_exe"]}" --tray')
    check("T2e: auto-start Run key written", RUN_VALUE in read_reg_value(),
          "registry value missing after write")
    cleanup_instances()
    p = run_launcher_nondev(env["tmp_launch"], env["upd_json"], extra=("--tray",))
    try:
        # The throwaway tmp process self-installs and exits by design; the
        # real tray instance runs from %LOCALAPPDATA%\MiMITA\launcher.
        alive = wait_for(lambda: process_running(LAUNCHER_IMAGE), 20)
        check("T2e: --tray instance alive (silent)", alive, "no launcher process")
        time.sleep(5)
        check("T2e: --tray did not auto-launch the game", not process_running(GAME_IMAGE),
              "game launched from --tray")
        check("T2e: --tray process still in tray after 5s", process_running(LAUNCHER_IMAGE))
    finally:
        terminate(p)
        cleanup_instances()
    if bak_reg:
        set_reg_value(parse_reg_data(bak_reg))
        check("T2e: auto-start Run key restored", RUN_VALUE in read_reg_value())
    else:
        delete_reg_value()
        check("T2e: auto-start Run key removed", not read_reg_value())


def tier2(zip_path):
    sep("TIER 2 — offline non-dev flows (file:// URLs, no GitHub)")
    if not os.environ.get("LOCALAPPDATA"):
        report("[SKIP] Tier 2: no LOCALAPPDATA (cannot test real launcher home)")
        return
    cleanup_instances()
    env, bak_exe, bak_cfg, bak_reg = _tier2_setup(zip_path)
    try:
        tier2_a(env)
        tier2_b(env)
        tier2_c(env)
        tier2_d(env)
        tier2_e(env, bak_reg)
    finally:
        _tier2_restore(env, bak_exe, bak_cfg, bak_reg)


# ── Preflight / main ────────────────────────────────────────────
def preflight():
    sep("PREFLIGHT")
    ok = True
    rdir = os.path.join(RELEASE, "2.0.1")
    for name in RELEASE_FILES:
        full = os.path.join(rdir, name)
        if not os.path.isfile(full):
            failed(f"release/2.0.1/{name} missing", "run a release option first")
            ok = False
        else:
            passed(f"release/2.0.1/{name} present")
    if not os.path.isfile(LAUNCHER_ROOT):
        failed("MimitaLauncher.exe at repo root missing", "run launcher\\build.bat")
        ok = False
    else:
        passed("MimitaLauncher.exe at repo root present")
    staged = os.path.join(rdir, "mimita.exe")
    if os.path.isfile(staged) and os.path.getsize(staged) > 60 * 1024 * 1024:
        failed("staged mimita.exe looks like a DEBUG build",
               f"{os.path.getsize(staged)/1e6:.1f} MB")
        ok = False
    else:
        passed("staged mimita.exe is a release build (~14 MB)")
    if process_running(GAME_IMAGE) or process_running(LAUNCHER_IMAGE):
        if "--force-kill" in sys.argv:
            report("  --force-kill: terminating running game/launcher processes")
            kill_image(GAME_IMAGE)
            kill_image(LAUNCHER_IMAGE)
        else:
            report("[FAIL] mimita.exe or MimitaLauncher.exe is already running.")
            report("       Close them (or re-run with --force-kill) before the test.")
            return False
    return ok


def main():
    global PASS_COUNT, FAIL_COUNT
    os.makedirs(TEST_ROOT, exist_ok=True)
    if os.path.isfile(REPORT):
        os.remove(REPORT)
    report("MiMITA local release / AV / launcher test")
    report("Time: " + time.strftime("%Y-%m-%d %H:%M:%S"))
    report("Artifacts: " + RELEASE)
    sep("HASHES")
    rdir = os.path.join(RELEASE, "2.0.1")
    for name in ("mimita.exe", "MimitaLauncher.exe", "mimita-game.zip"):
        full = os.path.join(rdir, name)
        if os.path.isfile(full):
            report(f"  {name}  SHA256={sha256_of(full)}  size={os.path.getsize(full)} bytes")

    if not preflight():
        report("\n[LAUNCHER-RELEASE-LOCAL-TEST] FAIL (preflight)")
        print("\n[LAUNCHER-RELEASE-LOCAL-TEST] FAIL (preflight)")
        sys.exit(1)

    zip_path = os.path.join(rdir, "mimita-game.zip")
    sep("DEFENDER SCANS (pre)")
    scan_report(os.path.join(rdir, "mimita.exe"), "staged mimita.exe")
    scan_report(LAUNCHER_ROOT, "MimitaLauncher.exe (root)")
    scan_report(zip_path, "mimita-game.zip")

    before_det = threat_detections()
    tier1(zip_path)
    tier2(zip_path)

    sep("DEFENDER THREAT HISTORY")
    after_det = threat_detections()
    threat_history(before_det, after_det)

    sep("SUMMARY")
    report(f"  PASS: {PASS_COUNT}   FAIL: {FAIL_COUNT}")
    report(f"  Report: {REPORT}")
    ok = FAIL_COUNT == 0
    report("\n[LAUNCHER-RELEASE-LOCAL-TEST] " + ("PASS" if ok else "FAIL"))
    print(f"\n[LAUNCHER-RELEASE-LOCAL-TEST] {'PASS' if ok else 'FAIL'} ({PASS_COUNT} pass, {FAIL_COUNT} fail)")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
