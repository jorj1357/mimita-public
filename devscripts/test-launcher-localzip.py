# 07 31 2026, 14 00
# purpose
# Integration test for MimitaLauncher's local-ZIP path.
# Proves the ZIP's own version.txt wins over the launcher folder's version.txt,
# that a valid game executable is launched and exits cleanly, and that an
# invalid game executable is rejected without any blocking Windows dialog.
# Does NOT download from GitHub, touch the real install dir, or open a game.
# Does NOT modify production source; builds a throwaway test-game.exe fixture.

import os
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LAUNCHER = os.path.join(ROOT, "MimitaLauncher.exe")
FIXTURE_SRC = os.path.join(ROOT, "tests", "fixtures", "launcher-test-game.c")
GCC = r"C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\gcc.exe"

TIMEOUT_S = 30
PASS = 0
FAIL = 1


def fail(msg):
    print(f"[FAIL] {msg}")
    sys.exit(FAIL)


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="") as f:
        f.write(data)


def make_zip(zip_path, version):
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("version.txt", version)


def build_fixture(out_exe):
    os.makedirs(os.path.dirname(out_exe), exist_ok=True)
    r = subprocess.run([GCC, "-Os", "-s", "-mwindows", FIXTURE_SRC, "-o", out_exe])
    if r.returncode != 0:
        fail(f"could not build test-game.exe fixture (gcc exit {r.returncode})")


def setup_scenario(root, install_dir, mimita_exe_bytes, zip_version, launcher_version):
    launcher_dir = os.path.join(root, "launcher")
    os.makedirs(launcher_dir, exist_ok=True)
    os.makedirs(install_dir, exist_ok=True)
    shutil.copy(LAUNCHER, os.path.join(launcher_dir, "MimitaLauncher.exe"))
    write(os.path.join(launcher_dir, "version.txt"), launcher_version)
    make_zip(os.path.join(launcher_dir, "mimita-game.zip"), zip_version)
    write(os.path.join(launcher_dir, "install-config.json"),
          '{"install_dir":"' + install_dir.replace("\\", "\\\\") + '"}')
    with open(os.path.join(install_dir, "mimita.exe"), "wb") as f:
        f.write(mimita_exe_bytes)
    return launcher_dir


def run_launcher(launcher_dir, proc_holder):
    exe = os.path.join(launcher_dir, "MimitaLauncher.exe")
    p = subprocess.Popen([exe, "--no-error-dialogs"], cwd=launcher_dir)
    proc_holder.append(p)
    try:
        return p.wait(timeout=TIMEOUT_S)
    except subprocess.TimeoutExpired:
        p.kill()
        p.wait()
        return None


def test_zip_version_wins_and_launches(root):
    install = os.path.join(root, "install")
    fixture_exe = os.path.join(root, "fixture", "test-game.exe")
    build_fixture(fixture_exe)
    with open(fixture_exe, "rb") as f:
        exe_bytes = f.read()
    launcher_dir = setup_scenario(root, install, exe_bytes,
                                  zip_version="9.9.9", launcher_version="1.1.1")

    procs = []
    code = run_launcher(launcher_dir, procs)
    if code is None:
        fail("Test 1: launcher did not exit within %ds" % TIMEOUT_S)
    if code != 0:
        fail(f"Test 1: launcher exit code {code}, expected 0")

    ver_file = os.path.join(install, "version.txt")
    installed = open(ver_file).read().strip() if os.path.exists(ver_file) else "(missing)"
    if installed != "9.9.9":
        fail(f"Test 1: install\\version.txt = '{installed}', expected '9.9.9' (from ZIP, not launcher folder 1.1.1)")

    marker = os.path.join(install, "test-game-launched.txt")
    if not os.path.exists(marker):
        fail("Test 1: test game was not launched (test-game-launched.txt missing)")
    print("[PASS] Test 1: ZIP version 9.9.9 stamped; launcher exited 0; test game launched")
    return procs


def test_invalid_exe_noninteractive(root):
    install = os.path.join(root, "install-bad")
    exe_bytes = b"this is not a windows executable, it is plain text"
    launcher_dir = setup_scenario(root, install, exe_bytes,
                                  zip_version="9.9.9", launcher_version="1.1.1")

    procs = []
    code = run_launcher(launcher_dir, procs)
    if code is None:
        fail("Test 2: launcher did not exit within %ds (blocked on an OS error dialog?)" % TIMEOUT_S)

    logs = sorted([
        os.path.join(install, "launcher-data", "logs", f)
        for f in os.listdir(os.path.join(install, "launcher-data", "logs"))
        if f.startswith("launch-error-") and f.endswith(".txt")
    ]) if os.path.isdir(os.path.join(install, "launcher-data", "logs")) else []

    if not logs:
        fail("Test 2: no launch-error log written for invalid executable")
    content = open(logs[0]).read()
    if "mimita.exe" not in content:
        fail("Test 2: launch-error log missing executable path")
    if "not a valid Windows executable" not in content:
        fail("Test 2: launch-error log missing invalid-executable reason")
    if "win32_error=193" not in content:
        fail("Test 2: launch-error log missing win32_error=193")
    print("[PASS] Test 2: invalid executable rejected without hang; error logged with path + win32_error=193")
    return procs


def main():
    if not os.path.isfile(LAUNCHER):
        fail(f"MimitaLauncher.exe not found at {LAUNCHER}. Build it first: launcher\\build.bat")
    if not os.path.isfile(GCC):
        fail(f"MinGW gcc not found at {GCC}")

    tmp = tempfile.mkdtemp(prefix="launcher-localzip-test-")
    procs = []
    try:
        procs += test_zip_version_wins_and_launches(tmp)
        procs += test_invalid_exe_noninteractive(tmp)
    finally:
        for p in procs:
            if p.poll() is None:
                p.kill()
                p.wait()
        time.sleep(0.3)
        shutil.rmtree(tmp, ignore_errors=True)

    print("\n[LAUNCHER-LOCALZIP-TEST] PASS")


if __name__ == "__main__":
    main()
