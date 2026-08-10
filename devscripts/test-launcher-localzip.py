# 08 10 2026, 17 00
# purpose
# Integration test for MimitaLauncher's local-ZIP path.
# Proves the ZIP's own version.txt wins, that a valid game executable is
# extracted into a version folder and launched, and that an invalid game
# executable is rejected without any blocking Windows dialog.
# The launcher now stays alive in the system tray, so the test terminates it
# after detecting its effects.
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


def make_zip(zip_path, version, exe_bytes=None):
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("version.txt", version)
        if exe_bytes is not None:
            zf.writestr("mimita.exe", exe_bytes)


def build_fixture(out_exe):
    os.makedirs(os.path.dirname(out_exe), exist_ok=True)
    r = subprocess.run([GCC, "-Os", "-s", "-mwindows", FIXTURE_SRC, "-o", out_exe])
    if r.returncode != 0:
        fail(f"could not build test-game.exe fixture (gcc exit {r.returncode})")


def setup_scenario(root, install_dir, zip_version, exe_bytes):
    launcher_dir = os.path.join(root, "launcher")
    os.makedirs(launcher_dir, exist_ok=True)
    os.makedirs(install_dir, exist_ok=True)
    shutil.copy(LAUNCHER, os.path.join(launcher_dir, "MimitaLauncher.exe"))
    make_zip(os.path.join(launcher_dir, "mimita-game.zip"), zip_version, exe_bytes)
    write(os.path.join(launcher_dir, "install-config.json"),
          '{"install_dir":"' + install_dir.replace("\\", "\\\\") + '"}')
    return launcher_dir


def launch_launcher(launcher_dir):
    exe = os.path.join(launcher_dir, "MimitaLauncher.exe")
    # --release-json <missing> keeps fetchReleaseInfo instant and offline.
    p = subprocess.Popen([exe, "--no-error-dialogs",
                          "--release-json", "missing.json"], cwd=launcher_dir)
    return p


def wait_for(path, seconds=TIMEOUT_S):
    deadline = time.time() + seconds
    while time.time() < deadline:
        if os.path.exists(path):
            return True
        time.sleep(0.2)
    return False


def test_zip_version_wins_and_launches(root):
    install = os.path.join(root, "install")
    fixture_exe = os.path.join(root, "fixture", "test-game.exe")
    build_fixture(fixture_exe)
    with open(fixture_exe, "rb") as f:
        exe_bytes = f.read()
    launcher_dir = setup_scenario(root, install, zip_version="9.9.9", exe_bytes=exe_bytes)

    p = launch_launcher(launcher_dir)
    try:
        version_dir = os.path.join(install, "versions", "v9.9.9")
        marker = os.path.join(version_dir, "test-game-launched.txt")
        if not wait_for(marker):
            fail("Test 1: test game was not launched (test-game-launched.txt missing)")

        active = os.path.join(install, "active-version.txt")
        active_ver = open(active).read().strip() if os.path.exists(active) else "(missing)"
        if active_ver != "9.9.9":
            fail(f"Test 1: active-version.txt = '{active_ver}', expected '9.9.9'")

        ver_file = os.path.join(version_dir, "version.txt")
        installed = open(ver_file).read().strip() if os.path.exists(ver_file) else "(missing)"
        if installed != "9.9.9":
            fail(f"Test 1: versions\\v9.9.9\\version.txt = '{installed}', expected '9.9.9'")

        if not os.path.isfile(os.path.join(version_dir, "mimita.exe")):
            fail("Test 1: versions\\v9.9.9\\mimita.exe missing (zip exe not extracted)")

        # Launcher stays alive in the tray after launching the game.
        time.sleep(0.5)
        if p.poll() is not None:
            fail(f"Test 1: launcher exited (code {p.returncode}); expected to stay in tray")
        print("[PASS] Test 1: ZIP version 9.9.9 stamped; game launched from version folder; launcher in tray")
    finally:
        if p.poll() is None:
            p.kill()
            p.wait()


def test_invalid_exe_noninteractive(root):
    install = os.path.join(root, "install-bad")
    exe_bytes = b"this is not a windows executable, it is plain text"
    launcher_dir = setup_scenario(root, install, zip_version="9.9.9", exe_bytes=exe_bytes)

    p = launch_launcher(launcher_dir)
    try:
        time.sleep(3)
        if p.poll() is not None:
            fail(f"Test 2: launcher exited (code {p.returncode}); expected to stay in tray")
        version_dir = os.path.join(install, "versions", "v9.9.9")
        if os.path.exists(version_dir):
            fail("Test 2: invalid executable was not rejected (version folder exists)")
        marker = os.path.join(install, "test-game-launched.txt")
        if os.path.exists(marker):
            fail("Test 2: game was launched for an invalid executable")
        print("[PASS] Test 2: invalid executable rejected without hang; no version folder created")
    finally:
        if p.poll() is None:
            p.kill()
            p.wait()


def main():
    if not os.path.isfile(LAUNCHER):
        fail(f"MimitaLauncher.exe not found at {LAUNCHER}. Build it first: launcher\\build.bat")
    if not os.path.isfile(GCC):
        fail(f"MinGW gcc not found at {GCC}")

    tmp = tempfile.mkdtemp(prefix="launcher-localzip-test-")
    try:
        test_zip_version_wins_and_launches(tmp)
        test_invalid_exe_noninteractive(tmp)
    finally:
        time.sleep(0.3)
        shutil.rmtree(tmp, ignore_errors=True)

    print("\n[LAUNCHER-LOCALZIP-TEST] PASS")


if __name__ == "__main__":
    main()
