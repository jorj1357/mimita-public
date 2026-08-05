#!/usr/bin/env python3
# 08 05 2026
"""purpose
* Fast local playtest launcher for mimita.exe.
* Presents an expandable console menu of launch modes.
* Mode 1 spawns a headless server on coolplace and auto-joins two game windows
  via the game's --connect flag (added for this launcher).
* Does NOT modify game files, configs, or assets.
* Does NOT auto-close windows; the user closes the game windows to end a mode.
"""

import os
import subprocess
import sys
import tempfile
import threading
import time

CREATE_NEW_CONSOLE = getattr(subprocess, "CREATE_NEW_CONSOLE", 0)

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_EXE = os.path.join(HERE, "mimita.exe")

SERVER_BIND = "0.0.0.0:1357"
CLIENT_ADDR = "127.0.0.1:1357"
ROOM_FILE_TIMEOUT = 25
GIVE_EACH_PROCESS_A_CONSOLE = True


def resolve_exe(override=None):
    exe = os.path.abspath(override) if override else DEFAULT_EXE
    if not os.path.isfile(exe):
        sys.exit("[ERROR] mimita.exe not found at: %s" % exe)
    return exe


def spawn(args):
    flags = CREATE_NEW_CONSOLE if GIVE_EACH_PROCESS_A_CONSOLE else 0
    return subprocess.Popen(args, creationflags=flags)


def wait_for_room_file(path, proc, timeout=ROOM_FILE_TIMEOUT):
    """Block until the headless server writes its room code file."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        if proc.poll() is not None:
            print("[LAUNCHER] server exited early with code %s"
                  % proc.returncode)
            return None
        if os.path.isfile(path):
            try:
                with open(path, "r", encoding="utf-8",
                          errors="replace") as f:
                    code = f.read().strip()
                if code:
                    return code
            except OSError:
                pass
        time.sleep(0.2)
    print("[LAUNCHER] timed out waiting for the server room code "
          "(is the coordinator/STUN reachable?)")
    return None


def remove_room_file(path):
    try:
        if os.path.isfile(path):
            os.remove(path)
    except OSError:
        pass


def run_playtest(exe):
    """Mode 1: headless server on coolplace + two auto-joining windows."""
    map_name = "coolplace"
    room_file = os.path.join(tempfile.gettempdir(),
                             "mimita_room_%d.txt" % os.getpid())
    try:
        open(room_file, "w").close()
    except OSError:
        room_file = "mimita_room.txt"

    server = None
    clients = []
    shutting_down = threading.Event()
    try:
        server_args = [
            exe,
            "--server",
            "--bind", SERVER_BIND,
            "--name", "Playtest Server",
            "--map", map_name,
            "--host-player", "Host",
            "--no-npcs",
            "--room-file", room_file,
        ]
        print("[LAUNCHER] starting headless server on %s (%s)"
              % (SERVER_BIND, map_name))
        server = spawn(server_args)

        code = wait_for_room_file(room_file, server)
        if code is None:
            if server.poll() is None:
                server.terminate()
            return

        print("[LAUNCHER] server up (room %s). Opening 2 game windows..."
              % code)
        for name in ("Player1", "Player2"):
            clients.append(spawn([
                exe,
                "--connect", CLIENT_ADDR,
                "--map", map_name,
                "--name", name,
            ]))

        def warn_if_server_dies():
            server.wait()
            if not shutting_down.is_set():
                print("[LAUNCHER] headless server exited (code %s)"
                      % server.returncode)

        threading.Thread(target=warn_if_server_dies, daemon=True).start()

        print("[LAUNCHER] playtest running. Close both game windows to end, "
              "or press Ctrl+C in this window.")
        for c in clients:
            c.wait()
        print("[LAUNCHER] all game windows closed; stopping the server.")
    except KeyboardInterrupt:
        print("\n[LAUNCHER] Ctrl+C received; cleaning up.")
    finally:
        shutting_down.set()
        for c in clients:
            if c.poll() is None:
                c.terminate()
        if server is not None and server.poll() is None:
            server.terminate()
            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass
        remove_room_file(room_file)


MODES = [
    {
        "key": "1",
        "name": "Playtest 2-player on coolplace (host server + 2 join)",
        "run": run_playtest,
    },
]


def main():
    override = None
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--exe" and i + 1 < len(args):
            override = args[i + 1]
            i += 2
        else:
            print("[ERROR] unknown argument: %s" % args[i])
            print("usage: python mimita-launcher.py [--exe <path-to-mimita.exe>]")
            return
    exe = resolve_exe(override)

    while True:
        print("=" * 52)
        print("MiMITA Launcher")
        print("  exe: %s" % exe)
        print("=" * 52)
        for m in MODES:
            print("  %s) %s" % (m["key"], m["name"]))
        print("  0) Quit")
        try:
            choice = input("Select mode: ").strip().lower()
        except EOFError:
            print("Bye.")
            return
        if not choice:
            continue
        if choice in ("0", "q", "quit"):
            print("Bye.")
            return
        mode = next((m for m in MODES if m["key"] == choice), None)
        if mode is None:
            print("[ERROR] unknown mode '%s'" % choice)
            continue
        mode["run"](exe)
        print()


if __name__ == "__main__":
    main()
