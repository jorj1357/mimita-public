# 10 17 2026, 15 30
"""Platform terminal helper for the MiMITA dev-loop dashboard.

Owns the small amount of platform-specific work required to give the dashboard a
readable, opaque terminal window: enabling Windows virtual-terminal processing,
setting the window title, and (on Windows) preferring to relaunch inside Windows
Terminal with a standard Windows console fallback.

This module is presentation/launch plumbing only. It does not own build, game,
run, or control state, and it never starts or stops the watchdog or daemon.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Set in the relaunched dashboard child so it never relaunches again.
DASHBOARD_TERMINAL_ENV = "MIMITA_DEV_DASHBOARD_TERMINAL"
# Optional Windows Terminal profile name to prefer (should be opaque).
WT_PROFILE_ENV = "MIMITA_WT_PROFILE"
STANDARD_OUTPUT_HANDLE = -11
ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004


def _stdout_isatty() -> bool:
    try:
        return bool(sys.stdout.isatty())
    except (ValueError, OSError):
        return False


def enable_virtual_terminal() -> bool:
    """Enable ANSI/VT processing for the current console when possible.

    Returns True when ANSI escape sequences should be honored. On non-Windows
    hosts this is just the stdout TTY check.
    """
    if os.name != "nt":
        return _stdout_isatty()
    try:
        import ctypes

        kernel32 = ctypes.windll.kernel32
        handle = kernel32.GetStdHandle(STANDARD_OUTPUT_HANDLE)
        mode = ctypes.c_ulong()
        if not kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
            return _stdout_isatty()
        if not kernel32.SetConsoleMode(
            handle, ctypes.c_ulong(mode.value | ENABLE_VIRTUAL_TERMINAL_PROCESSING)
        ):
            return _stdout_isatty()
        return True
    except (AttributeError, OSError, ValueError):
        return _stdout_isatty()


def supports_ansi() -> bool:
    # MIMITA_DASHBOARD_ANSI can force the capability on/off so the rainbow and
    # plain fallback paths are both reachable when stdout is not a real TTY
    # (automated validation, redirected logging).
    forced = os.environ.get("MIMITA_DASHBOARD_ANSI", "").strip().lower()
    if forced in ("1", "true", "yes"):
        return True
    if forced in ("0", "false", "no"):
        return False
    return _stdout_isatty()


def set_title(title: str) -> None:
    """Set the console window title via Win32 and the ANSI OSC sequence."""
    if os.name == "nt":
        try:
            import ctypes

            ctypes.windll.kernel32.SetConsoleTitleW(str(title))
        except (AttributeError, OSError, ValueError):
            pass
    try:
        sys.stdout.write(f"\x1b]0;{title}\x07")
        sys.stdout.flush()
    except (ValueError, OSError):
        pass
    if os.name == "nt":
        # Some hosts need an extra flush of the title through the VT encoder.
        try:
            sys.stdout.flush()
        except (ValueError, OSError):
            pass


def in_windows_terminal() -> bool:
    return bool(os.environ.get("WT_SESSION"))


def windows_terminal_executable() -> str | None:
    if os.name != "nt":
        return None
    return shutil.which("wt.exe") or shutil.which("wt")


def _dashboard_command() -> list[str]:
    return [
        sys.executable,
        str(ROOT / "devscripts" / "dev-loop.py"),
        "--dashboard",
        "--dev-loop-role=dashboard",
    ]


def relaunch_in_windows_terminal(title: str) -> bool:
    """Launch the dashboard in an opaque Windows Terminal window.

    Returns True only when a new Windows Terminal window was started; the caller
    should then exit so the shortcut's own console closes and exactly one
    dashboard remains. Returns False when the caller should render in place
    (not Windows, already inside Windows Terminal, `wt.exe` unavailable, or this
    process is already the relaunched child).
    """
    if os.name != "nt":
        return False
    if os.environ.get(DASHBOARD_TERMINAL_ENV):
        return False
    if in_windows_terminal():
        return False
    wt = windows_terminal_executable()
    if not wt:
        return False

    args = [wt, "-w", "new", "new-tab", "--suppressApplicationTitle", "--title", title]
    profile = os.environ.get(WT_PROFILE_ENV)
    if profile:
        args += ["-p", profile]
    args += ["-d", str(ROOT), "--", *_dashboard_command()]

    environment = os.environ.copy()
    environment[DASHBOARD_TERMINAL_ENV] = "1"
    creationflags = 0
    if os.name == "nt":
        creationflags = (
            getattr(subprocess, "DETACHED_PROCESS", 0)
            | getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
        )
    try:
        subprocess.Popen(
            args,
            cwd=str(ROOT),
            env=environment,
            close_fds=True,
            creationflags=creationflags,
        )
    except OSError:
        return False
    return True
