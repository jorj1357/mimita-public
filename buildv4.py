# 09 24 2026
"""Build MiMITA, start a local NPC test server, and join it immediately.

This keeps buildv3.py unchanged. The launched server/client use command-line
settings so the community-server GUI setup is skipped:
  - Discord announcement disabled
  - automatic map rotation disabled
  - one startup NPC enabled
  - newest dust2cyberiavN map selected

After startup, the newest session events.jsonl is opened in VS Code.
"""

import glob
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone

import build_agent


ROOT = os.path.dirname(os.path.abspath(__file__))
BUILD_RESULT = os.path.join(ROOT, "build", "build-result.json")


def newest_dust_map():
    candidates = []
    for path in glob.glob(os.path.join(ROOT, "assets", "maps", "dust2cyberiav*.glb")):
        match = re.search(r"dust2cyberiav(\d+)\.glb$", os.path.basename(path), re.IGNORECASE)
        if match:
            candidates.append((int(match.group(1)), path))
    if not candidates:
        raise FileNotFoundError("No assets/maps/dust2cyberiavN.glb map was found")
    return max(candidates, key=lambda item: item[0])[1]


def shared_events_file():
    now = datetime.now(timezone.utc)
    run_dir = os.path.join(
        ROOT, "logs", now.strftime("%Y-%m-%d"), now.strftime("%Y%m%d_%H%M%S")
    )
    os.makedirs(run_dir, exist_ok=True)
    return os.path.join(run_dir, "events.jsonl")


def open_in_vscode(path):
    if not path:
        print("[V4] no events.jsonl found yet")
        return
    try:
        subprocess.Popen(["code", "-g", path + ":1"], cwd=ROOT)
        print(f"[V4] opened JSONL in VS Code: {path}")
    except FileNotFoundError:
        # Explorer selection is a useful fallback on machines without `code`.
        subprocess.Popen(["explorer.exe", "/select,", os.path.normpath(path)])
        print(f"[V4] VS Code command not found; selected JSONL in Explorer: {path}")


def build():
    try:
        build_agent.main()
    except SystemExit as result:
        if result.code not in (0, None):
            raise
    try:
        with open(BUILD_RESULT, "r", encoding="utf-8") as stream:
            result = json.load(stream)
    except (OSError, ValueError) as error:
        raise RuntimeError(f"Could not read build result: {error}") from error
    if result.get("status") not in ("SUCCESS", "NOTHING_CHANGED"):
        raise RuntimeError(f"Build did not succeed: {result.get('status')}")
    executable = result.get("executable_path")
    if not executable or not os.path.isfile(executable):
        raise RuntimeError(f"Built executable is missing: {executable}")
    return executable


def main():
    executable = build()
    map_path = newest_dust_map()
    map_name = os.path.splitext(os.path.basename(map_path))[0]
    events_path = shared_events_file()
    process_env = os.environ.copy()
    process_env["MIMITA_EVENTS_FILE"] = events_path
    print(f"[V4] using newest map: {map_name}")
    print(f"[V4] shared client/server JSONL: {events_path}")

    room_file = tempfile.NamedTemporaryFile(
        prefix="mimita_v4_room_", suffix=".txt", delete=False
    )
    room_file_path = room_file.name
    room_file.close()

    server_args = [
        executable,
        "--server",
        "--bind", "127.0.0.1:1357",
        "--name", "MiMITA NPC JSONL Test",
        "--map", map_name,
        "--mode", "sandbox",
        "--npcs", "1",
        "--no-map-rotation",
        "--no-discord-notification",
        "--room-file", room_file_path,
    ]
    print("[V4] starting local server with NPC startup enabled")
    server = subprocess.Popen(server_args, cwd=ROOT, env=process_env)

    # The dedicated server registers its ICE room and writes the actual code
    # to --room-file. Wait for that code before joining, so this launcher uses
    # the same room-code lifecycle as the normal server UI.
    room_code = ""
    deadline = time.time() + 20.0
    while time.time() < deadline and server.poll() is None:
        try:
            with open(room_file_path, "r", encoding="utf-8") as stream:
                room_code = stream.read().strip()
        except OSError:
            pass
        if room_code:
            break
        time.sleep(0.25)
    if room_code:
        print(f"[V4] SERVER ROOM CODE: {room_code}")
    else:
        print("[V4] WARNING: server did not publish a room code before timeout")

    # Direct local UDP is intentional for a deterministic local test; the
    # server still has a real coordinator/ICE room code and displays it in its
    # own startup output. The client receives the authoritative room metadata.
    client_args = [
        executable,
        "--connect", "127.0.0.1:1357",
        "--room-code", room_code,
        "--map", map_name,
        "--name", "admin",
    ]
    print("[V4] starting client and joining local server")
    subprocess.Popen(client_args, cwd=ROOT, env=process_env)

    # The logger creates its run directory during process startup.
    time.sleep(3.0)
    open_in_vscode(events_path)
    print(f"[V4] server pid={server.pid}; leave both processes running for the test")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"[V4] FAILED: {error}", file=sys.stderr)
        raise
