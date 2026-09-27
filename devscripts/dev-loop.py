#!/usr/bin/env python3
"""MiMITA's small external development loop.

This tool deliberately owns development orchestration only. The existing
build.py remains responsible for dependency tracking, PCH use, parallel
compilation, and linking. The game remains responsible for gameplay.

Usage:
    python devscripts/dev-loop.py --profile npc-navigation
    python devscripts/dev-loop.py --profile npc-navigation --auto-restart

The loop uses ccache automatically when build_toolchain.py can find it. It
never kills arbitrary mimita.exe processes; only children that it launched.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import threading
import time
import tempfile
from pathlib import Path

# Running this file as `python devscripts/dev-loop.py` puts `devscripts/`
# first on sys.path; add the repository root for the shared toolchain owner.
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from build_toolchain import compiler as resolve_compiler
from build_toolchain import glfw_lib as resolve_glfw_lib

DEV_ROOT = ROOT / ".dev"
BUILD_ROOT = DEV_ROOT / "builds"
STATE_PATH = DEV_ROOT / "state.json"
PROFILE_ROOT = ROOT / "devscripts" / "dev-profiles"

POLL_SECONDS = 0.15
DEBOUNCE_SECONDS = 0.30
KEEP_BUILDS = 5
RUNTIME_DLLS = (
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll",
    "glfw3.dll",
)

WATCH_EXTENSIONS = {".c", ".cc", ".cpp", ".h", ".hh", ".hpp", ".rc", ".py"}
WATCH_DIRECTORIES = (ROOT / "src", ROOT / "external" / "libjuice" / "src")
WATCH_FILES = (
    ROOT / "mimita.rc",
    ROOT / "build.py",
    ROOT / "build_agent.py",
    ROOT / "build_game_dll.py",
    ROOT / "build_toolchain.py",
)


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def file_signature(path: Path):
    try:
        stat = path.stat()
        return (stat.st_mtime_ns, stat.st_size)
    except OSError:
        return None


def snapshot_inputs() -> dict[str, tuple[int, int] | None]:
    result = {}
    for path in WATCH_FILES:
        result[str(path)] = file_signature(path)
    for directory in WATCH_DIRECTORIES:
        if not directory.is_dir():
            continue
        for path in directory.rglob("*"):
            if path.is_file() and path.suffix.lower() in WATCH_EXTENSIONS:
                result[str(path)] = file_signature(path)
    return result


def find_ccache() -> str | None:
    configured = os.environ.get("MIMITA_CCACHE")
    if configured and Path(configured).is_file():
        return configured
    found = shutil.which("ccache")
    return found


def load_profile(name: str) -> dict:
    path = Path(name)
    if path.suffix.lower() != ".json":
        path = PROFILE_ROOT / (name + ".json")
    if not path.is_absolute():
        path = ROOT / path
    if not path.is_file():
        raise SystemExit(f"[DEV] profile not found: {path}")
    profile = read_json(path)
    profile["_path"] = str(path)
    return profile


def next_build_number() -> int:
    numbers = []
    if BUILD_ROOT.is_dir():
        for path in BUILD_ROOT.iterdir():
            if path.is_dir() and path.name.isdigit():
                numbers.append(int(path.name))
    return max(numbers, default=0) + 1


def published_builds() -> list[Path]:
    if not BUILD_ROOT.is_dir():
        return []
    return sorted(
        (p for p in BUILD_ROOT.iterdir() if p.is_dir() and p.name.isdigit()),
        key=lambda p: int(p.name),
        reverse=True,
    )


def publish_build(number: int) -> Path:
    source_exe = ROOT / "mimita.exe"
    if not source_exe.is_file():
        raise RuntimeError("build succeeded but mimita.exe was not produced")

    destination = BUILD_ROOT / f"{number:04d}"
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source_exe, destination / "mimita.exe")

    runtime_sources = {name: ROOT / name for name in RUNTIME_DLLS}
    try:
        compiler_dir = Path(resolve_compiler()).parent
        runtime_sources.update({
            "libgcc_s_seh-1.dll": compiler_dir / "libgcc_s_seh-1.dll",
            "libstdc++-6.dll": compiler_dir / "libstdc++-6.dll",
            "libwinpthread-1.dll": compiler_dir / "libwinpthread-1.dll",
        })
        runtime_sources["glfw3.dll"] = Path(resolve_glfw_lib()) / "glfw3.dll"
    except FileNotFoundError:
        pass

    for name in RUNTIME_DLLS:
        source = runtime_sources[name]
        if source.is_file():
            shutil.copy2(source, destination / name)
        else:
            print(f"[DEV] published build missing runtime DLL: {source}")

    for old in published_builds()[KEEP_BUILDS:]:
        shutil.rmtree(old, ignore_errors=True)
    return destination


class DevLoop:
    def __init__(self, profile: dict, auto_restart: bool):
        self.profile = profile
        self.auto_restart = auto_restart
        self.stop_event = threading.Event()
        self.change_event = threading.Event()
        self.lock = threading.Lock()
        self.generation = 0
        self.last_snapshot = snapshot_inputs()
        self.processes: list[subprocess.Popen] = []
        self.running_build = None
        self.latest_build = None
        self.latest_generation = 0
        self.latest_stale = False
        self.build_pending = True
        self.last_message = "starting"
        self.last_status_line = None
        self.room_file_path: Path | None = None
        self.room_code = None

    def state(self, status: str = "idle") -> dict:
        return {
            "profile": self.profile.get("name", "unnamed"),
            "running_build": self.running_build,
            "latest_successful_build": self.latest_build,
            "source_generation": self.generation,
            "latest_successful_source_generation": self.latest_generation,
            "latest_build_stale": self.latest_stale,
            "auto_restart": self.auto_restart,
            "build_status": status,
            "message": self.last_message,
            "updated_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        }

    def save_state(self, status: str = "idle") -> None:
        write_json(STATE_PATH, self.state(status))

    def watch(self) -> None:
        while not self.stop_event.wait(POLL_SECONDS):
            current = snapshot_inputs()
            if current != self.last_snapshot:
                self.last_snapshot = current
                with self.lock:
                    self.generation += 1
                    self.latest_stale = self.latest_build is not None
                    self.last_message = f"source changed, generation {self.generation}"
                self.change_event.set()
                self.save_state("source_changed")

    def run_build(self) -> bool:
        with self.lock:
            requested_generation = self.generation
            self.running_build = requested_generation
            self.last_message = "building"
        self.save_state("building")

        command = [sys.executable, str(ROOT / "build.py"), "build-only"]
        environment = os.environ.copy()
        ccache = find_ccache()
        if ccache:
            environment["MIMITA_CCACHE"] = ccache
            print(f"[DEV] ccache: {ccache}")
        else:
            print("[DEV] ccache: not found; build will still work without it")

        print("[DEV] starting incremental build")
        output = []
        process = subprocess.Popen(
            command,
            cwd=ROOT,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        assert process.stdout is not None
        for line in process.stdout:
            print(line, end="")
            output.append(line)
        result = process.wait()
        successful = result == 0 and (
            "BUILD SUCCESS" in "".join(output)
            or "Nothing changed." in "".join(output)
        )

        with self.lock:
            self.running_build = None
            current_generation = self.generation

        if successful:
            number = next_build_number()
            try:
                destination = publish_build(number)
            except Exception as error:
                self.last_message = f"publish failed: {error}"
                self.save_state("publish_failed")
                print(f"[DEV] publish failed: {error}")
                return False
            with self.lock:
                self.latest_build = number
                self.latest_generation = requested_generation
                self.latest_stale = current_generation != requested_generation
                self.last_message = f"published build {number} at {destination}"
                self.build_pending = self.latest_stale
            print(f"[DEV] published build {number}: {destination}")
            self.save_state("success")
            # A successful publication is always the equivalent of pressing
            # [1]. The toggle remains a compatibility/status setting, but a
            # finished build must become the running build immediately.
            if not self.latest_stale:
                self.restart_latest()
            else:
                self.print_status()
            return True

        with self.lock:
            self.last_message = f"build failed with exit code {result}"
            self.build_pending = False
        self.save_state("failed")
        print(f"[DEV] build failed with exit code {result}; current game was left running")
        return False

    def launch_latest(self) -> None:
        if self.latest_build is None:
            return
        build_dir = BUILD_ROOT / f"{self.latest_build:04d}"
        exe = build_dir / "mimita.exe"
        if not exe.is_file():
            print(f"[DEV] missing published executable: {exe}")
            return

        self.stop_processes()
        server_bind = self.profile.get("server_bind", "0.0.0.0:1357")
        room_fd, room_file_name = tempfile.mkstemp(prefix="mimita-dev-room-", suffix=".txt")
        os.close(room_fd)
        self.room_file_path = Path(room_file_name)
        self.room_file_path.write_text("", encoding="utf-8")
        server_args = [
            str(exe), "--server", "--bind", server_bind,
            "--name", str(self.profile.get("server_name", "MiMITA Dev Server")),
            "--map", str(self.profile.get("map", "coolplace")),
            "--mode", str(self.profile.get("mode", "sandbox")),
            "--gamemode", str(self.profile.get("gamemode", "sandbox")),
            "--weapon-set", str(self.profile.get("weapon_set", 1)),
            "--npcs", str(self.profile.get("npc_count", 1)),
            "--no-discord-notification",
            "--room-file", str(self.room_file_path),
        ]
        # Leave --host-player unset for the automatic dev client. The server's
        # existing empty-name rule makes the first room-code joiner the host,
        # avoiding a mismatch between a profile label and the logged-in
        # AuthSystem display name used by the real client.
        configured_host = str(self.profile.get("host_player_name", "")).strip()
        if configured_host:
            server_args.extend(["--host-player", configured_host])
        if not self.profile.get("auto_map_rotation", False):
            server_args.append("--no-map-rotation")

        print(f"[DEV] launching build {self.latest_build} server")
        server = subprocess.Popen(
            server_args,
            cwd=ROOT,
            creationflags=getattr(subprocess, "CREATE_NEW_CONSOLE", 0),
        )
        self.processes = [server]

        # The dedicated server registers with the coordinator, then writes
        # the room code. This is the same handshake the GUI uses before it
        # starts the client join.
        deadline = time.time() + 20.0
        room_code = ""
        while time.time() < deadline:
            if server.poll() is not None:
                self.last_message = (
                    f"server exited before room-code handshake: {server.returncode}"
                )
                self.save_state("server_failed")
                print(f"[DEV] {self.last_message}")
                self.stop_processes()
                self.print_status()
                return
            try:
                room_code = self.room_file_path.read_text(encoding="utf-8").strip()
            except OSError:
                room_code = ""
            if room_code:
                break
            time.sleep(0.1)

        if not room_code:
            self.last_message = "server did not publish a room code within 20 seconds"
            self.save_state("server_failed")
            print(f"[DEV] {self.last_message}")
            self.stop_processes()
            self.print_status()
            return

        self.room_code = room_code
        print(f"[DEV] ROOM CODE: {room_code}")

        client_args = [
            str(exe), "--room", room_code,
            "--map", str(self.profile.get("map", "coolplace")),
            "--name", str(self.profile.get("client_name", "NPC Dev")),
        ]
        print(f"[DEV] launching build {self.latest_build} client")
        client = subprocess.Popen(
            client_args,
            cwd=ROOT,
            creationflags=getattr(subprocess, "CREATE_NEW_CONSOLE", 0),
        )
        self.processes.append(client)
        self.running_build = self.latest_build
        self.save_state("running")
        self.print_status()

    def restart_latest(self) -> None:
        self.launch_latest()

    def stop_processes(self) -> None:
        for process in self.processes:
            if process.poll() is None:
                process.terminate()
        deadline = time.time() + 3.0
        for process in self.processes:
            remaining = max(0.0, deadline - time.time())
            try:
                process.wait(timeout=remaining)
            except subprocess.TimeoutExpired:
                process.kill()
        self.processes = []
        self.running_build = None
        self.cleanup_room_file()

    def cleanup_room_file(self) -> None:
        if self.room_file_path is not None:
            try:
                self.room_file_path.unlink()
            except FileNotFoundError:
                pass
            except OSError as error:
                print(f"[DEV] could not remove room file: {error}")
            self.room_file_path = None
        self.room_code = None

    def print_status(self) -> None:
        status_line = (
            self.running_build,
            self.latest_build,
            self.auto_restart,
            self.latest_stale,
        )
        if status_line == self.last_status_line:
            return
        self.last_status_line = status_line
        print()
        print(f"RUNNING: {self.running_build or '(none)'}")
        print(f"LATEST:  {self.latest_build or '(none)'}")
        print(f"AUTO-RESTART: {'ON' if self.auto_restart else 'OFF'}")
        if self.latest_stale:
            print("LATEST BUILD IS STALE")
        print("[1] Switch to newest  [2] Stay on current  [A] Toggle auto-restart  [Q] Quit")

    def key_commands(self) -> None:
        if os.name != "nt":
            return
        import msvcrt
        if not msvcrt.kbhit():
            return
        key = msvcrt.getwch().lower()
        if key == "1":
            self.launch_latest()
            self.print_status()
        elif key == "a":
            self.auto_restart = not self.auto_restart
            print(f"[DEV] auto-restart {'ON' if self.auto_restart else 'OFF'}")
            self.save_state("idle")
        elif key == "q":
            self.stop_event.set()

    def run(self) -> None:
        watcher = threading.Thread(target=self.watch, daemon=True)
        watcher.start()
        self.save_state("starting")
        print(f"[DEV] profile={self.profile.get('name', 'unnamed')}")
        print(f"[DEV] root={ROOT}")
        print("[DEV] one build at a time; edits during a build queue another build")

        try:
            while not self.stop_event.is_set():
                self.key_commands()
                if self.build_pending:
                    self.change_event.clear()
                    time.sleep(DEBOUNCE_SECONDS)
                    self.run_build()
                    continue
                if self.change_event.wait(0.20):
                    self.build_pending = True
        finally:
            self.stop_event.set()
            self.stop_processes()
            self.save_state("stopped")
            print("[DEV] stopped; daemon-owned game processes closed")


def main() -> int:
    parser = argparse.ArgumentParser(description="MiMITA incremental development loop")
    parser.add_argument("--profile", default="npc-navigation")
    parser.add_argument("--auto-restart", action="store_true")
    args = parser.parse_args()
    profile = load_profile(args.profile)
    DevLoop(profile, args.auto_restart).run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
