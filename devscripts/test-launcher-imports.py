#!/usr/bin/env python3
# 07 31 2026, 00 00
# purpose
# Regression check for the MimitaLauncher TaskDialogIndirect startup bug.
# Verifies the launcher EXE does NOT statically import TaskDialogIndirect and
# that its import table names COMCTL32 (not the launcher itself) for comctl32.
# Also verifies the launcher process starts and stays alive (no 0xC0000139).
# Does NOT launch the game, touch the VPS, or download anything.

import os
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OBJDUMP = r"C:\important\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\objdump.exe"
LAUNCHER = ROOT / "MimitaLauncher.exe"


def imports_table():
    result = subprocess.run(
        [OBJDUMP, "-p", str(LAUNCHER)],
        capture_output=True, text=True, errors="replace")
    return result.stdout


def main():
    if not LAUNCHER.exists():
        print(f"FAIL: launcher missing: {LAUNCHER}")
        return 1
    if LAUNCHER.stat().st_size == 0:
        print("FAIL: launcher is zero bytes")
        return 1

    text = imports_table()
    ok = True

    # 1. TaskDialogIndirect must NOT be a static import at all.
    if re.search(r"TaskDialogIndirect", text):
        print("FAIL: TaskDialogIndirect is statically imported in the PE")
        ok = False
    else:
        print("PASS: TaskDialogIndirect is not a static import")

    # 2. COMCTL32 must be imported from COMCTL32.dll, never from the launcher.
    #    The bug symptom was the loader searching the EXE itself.
    if "DLL Name: COMCTL32.dll" not in text:
        print("FAIL: COMCTL32.dll is not in the import table")
        ok = False
    else:
        print("PASS: COMCTL32.dll present in import table")
    if re.search(r"DLL Name: [^\n]*MimitaLauncher\.exe", text, re.IGNORECASE):
        print("FAIL: import table references the launcher itself as a DLL")
        ok = False
    else:
        print("PASS: import table does not reference MimitaLauncher.exe as a DLL")

    # 3. Embedded v6 manifest present (comctl32 v6 -> TaskDialogIndirect exists).
    #    The resource must be numeric RT_MANIFEST (type 0x18 = 24); windres
    #    writes the bare token as a string name unless given "24".
    has_manifest_res = ("RT_MANIFEST" in text
                        or "ID: 0x000018" in text
                        or "0x000018" in text)
    if has_manifest_res:
        print("PASS: common-controls v6 manifest embedded (RT_MANIFEST/24)")
    else:
        print("FAIL: manifest resource not embedded as RT_MANIFEST type 24")
        ok = False

    # 3b. Background image + GUI config must be numeric RCDATA resources. The
    #     same windres quirk as the manifest: a bare token becomes a string
    #     name, so FindResource(MAKEINTRESOURCE(101/102)) would fail and the
    #     gradient would show instead of the PNG. The string "IDB_LOADING_IMAGE"
    #     must NOT appear in the binary, and the embedded config must.
    raw = LAUNCHER.read_bytes()
    if b"IDB_LOADING_IMAGE" in raw:
        print("FAIL: IDB_LOADING_IMAGE is a string resource name, not numeric 101")
        ok = False
    else:
        print("PASS: IDB_LOADING_IMAGE is a numeric resource (no string name)")
    if b'"window.title"' in raw:
        print("PASS: launcher-gui.json embedded as GUI_CONFIG RCDATA")
    else:
        print("FAIL: launcher-gui.json not embedded (GUI_CONFIG resource missing)")
        ok = False

    # 3c. The launcher must embed an application icon (taskbar + system tray).
    import ctypes
    from ctypes import wintypes
    shell32 = ctypes.WinDLL("shell32.dll")
    shell32.ExtractIconExW.restype = ctypes.c_uint
    shell32.ExtractIconExW.argtypes = [
        wintypes.LPCWSTR, ctypes.c_int,
        ctypes.POINTER(wintypes.HICON), ctypes.POINTER(wintypes.HICON),
        ctypes.c_uint]
    large = (wintypes.HICON * 1)()
    small = (wintypes.HICON * 1)()
    icon_count = shell32.ExtractIconExW(str(LAUNCHER), 0, large, small, 1)
    user32 = ctypes.WinDLL("user32.dll")
    user32.DestroyIcon.argtypes = [wintypes.HICON]
    for ic in (large[0], small[0]):
        if ic:
            user32.DestroyIcon(ic)
    if icon_count > 0:
        print("PASS: launcher embeds an application icon (ExtractIconEx found it)")
    else:
        print("FAIL: launcher has no embedded application icon")
        ok = False

    # 4. Launcher process must start and stay alive (no STATUS_ENTRYPOINT_NOT_FOUND).
    #    Run from an isolated temp dir: a dummy mimita-game.zip makes it dev-mode
    #    (no self-install/self-update), and a missing --release-json keeps the
    #    version fetch instant and offline. Without an install-config.json the
    #    launcher just shows the first-run wizard and stays in the tray.
    import tempfile
    import shutil
    tmp = tempfile.mkdtemp(prefix="launcher-import-test-")
    try:
        isolated = os.path.join(tmp, "MimitaLauncher.exe")
        shutil.copy(str(LAUNCHER), isolated)
        dummy_zip = os.path.join(tmp, "mimita-game.zip")
        open(dummy_zip, "wb").close()
        proc = subprocess.Popen(
            [isolated, "--no-error-dialogs", "--release-json", "missing.json"],
            cwd=tmp)
        time.sleep(3)
        if proc.poll() is None:
            print(f"PASS: launcher started and is running (pid={proc.pid})")
            proc.terminate()
            time.sleep(1)
            if proc.poll() is None:
                proc.kill()
        else:
            code = proc.returncode
            print(f"FAIL: launcher exited immediately with code={code} (0x{code & 0xFFFFFFFF:08X})")
            if code == -1073741511 or (code & 0xFFFFFFFF) == 0xC0000139:
                print("      -> STATUS_ENTRYPOINT_NOT_FOUND (TaskDialogIndirect bug)")
            ok = False
    finally:
        time.sleep(0.2)
        shutil.rmtree(tmp, ignore_errors=True)

    print()
    print("[LAUNCHER-IMPORT-TEST]", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
