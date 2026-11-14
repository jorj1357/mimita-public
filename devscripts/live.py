#!/usr/bin/env python3
# 2026-11-12
# purpose
# One-command live coordinator for the whole-game hot-reload loop.
#
#   python devscripts/live.py
#
# It watches the canonical source tree, debounces completed saves, pins one
# coherent source revision through devscripts/source_manifest.py, and spawns
# devscripts/game_build_worker.py in the background. A successful worker writes
# build/game/current.json and build/game/ready.json; the probe host now polls
# ready.json and activates the generation at a safe boundary.
#
# It never blocks the simulation thread, never relaunches the game, and never
# requests an EXE build. Unknown ownership stops with a diagnostic.
#
# The probe host polls ready.json. The production whole-game cutover is still
# pending.

from __future__ import annotations

import argparse
import atexit
import fnmatch
import json
import os
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from devscripts.source_manifest import build_manifest, diff_manifests, validate  # noqa: E402
from devscripts.process_identity import pid_alive  # noqa: E402

LIVE_LOCK_PATH = ROOT / ".dev" / "live.lock"
PLANS_PATH = ROOT / "docs" / "handoffs" / "hot-reload-work-plans.json"
SHARED_PREFIXES = (
    "src/host/abi/",
    "docs/specs/hotreload/",
    "docs/handoffs/",
    "config/source-owners.json",
    "AGENTS.md",
)

WATCH_ROOTS = (
    "src",
    "include",
    "external/recastnavigation",
    "external/libjuice",
    "external/manifold-prebuilt/include",
)
WATCH_FILES = (
    "build.py",
    "build_game_dll.py",
    "CMakeLists.txt",
    "config/build/source-owners.json",
    "config/modules/modules.json",
    "config/hot-reload/state-schema.json",
    "config/hot-reload/packet-schema.json",
    "src/network/packets.h",
)
WATCH_SUFFIXES = (".cpp", ".h", ".hpp", ".hh", ".hxx", ".inl", ".ipp", ".json", ".rc")
EXCLUDE_DIRS = {".git", ".dev", "build", "dist", "node_modules", ".venv", "__pycache__"}


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def read_json(path: Path) -> dict | None:
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, json.JSONDecodeError):
        return None


def write_json_atomic(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def watch_files() -> list[Path]:
    files: list[Path] = []
    for name in WATCH_FILES:
        path = ROOT / name
        if path.is_file():
            files.append(path)
    for directory in WATCH_ROOTS:
        base = ROOT / directory
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in WATCH_SUFFIXES:
                continue
            if set(path.relative_to(ROOT).parts) & EXCLUDE_DIRS:
                continue
            files.append(path)
    return files


def snapshot_signature() -> tuple:
    signature = []
    for path in watch_files():
        try:
            stat = path.stat()
        except OSError:
            continue
        signature.append((str(path), stat.st_mtime_ns, stat.st_size))
    return tuple(sorted(signature))


def snapshot_path(out_root: Path, revision: str) -> Path:
    return out_root / "snapshots" / revision / "manifest.json"


def spawn_build(revision: str, out_root: Path,
                change_report: Path | None = None) -> subprocess.Popen:
    command = [
        sys.executable,
        str(ROOT / "devscripts" / "game_build_worker.py"),
        "--manifest", str(snapshot_path(out_root, revision)),
        "--out", str(out_root),
        "--revision", revision,
    ]
    if change_report is not None:
        command.extend(["--change-report", str(change_report)])
    process = subprocess.Popen(command, cwd=str(ROOT))
    print(f"[LIVE] build start revision={revision} pid={process.pid}")
    return process


def publish_ready(out_root: Path, revision: str) -> bool:
    current = read_json(out_root / "current.json")
    if not current or current.get("revision") != revision or current.get("status") != "ok":
        return False
    ready = {
        "status": "ready",
        "revision": revision,
        "generation": current.get("generation"),
        "dll": current.get("dll"),
        "code_hash": current.get("code_hash"),
        "published_utc": utc_now(),
    }
    if current.get("change_report") is not None:
        ready["change_report"] = current["change_report"]
    write_json_atomic(out_root / "ready.json", ready)
    print(f"[LIVE] ready revision={revision} generation={current.get('generation')} "
          f"dll={current.get('dll')}")
    return True


CODEGEN_SCRIPTS = (
    "resident_state_codegen.py",
    "packet_registry_codegen.py",
)


def sync_state_codegen(dry_run: bool) -> bool:
    for script in CODEGEN_SCRIPTS:
        command = [sys.executable, str(ROOT / "devscripts" / script)]
        if dry_run:
            command.append("--check")
        result = subprocess.run(command, cwd=str(ROOT), capture_output=True, text=True)
        output = (result.stdout or "") + (result.stderr or "")
        for line in output.splitlines():
            print(f"[LIVE] {line}")
        if result.returncode != 0:
            print(f"[LIVE] codegen rejected by {script}; no build requested",
                  file=sys.stderr)
            return False
    return True


def tail_journal(event_filter: str, lines: int) -> int:
    """Read the newest events.jsonl. Multiple agents may run this at once."""
    logs = ROOT / "logs"
    candidates = list(logs.rglob("events.jsonl")) if logs.is_dir() else []
    if not candidates:
        print("[LIVE] no events.jsonl found under logs/", file=sys.stderr)
        return 1
    path = max(candidates, key=lambda item: item.stat().st_mtime)
    from collections import deque

    with path.open(encoding="utf-8", errors="replace") as handle:
        recent = deque(handle, maxlen=max(lines * 8, 200))
    shown = 0
    for line in reversed(recent):
        if event_filter and event_filter not in line:
            continue
        print(line.rstrip())
        shown += 1
        if shown >= lines:
            break
    print(f"[LIVE] journal {path} shown={shown} filter={event_filter or '*'}")
    return 0


def _release_live_lock() -> None:
    try:
        if LIVE_LOCK_PATH.read_text(encoding="utf-8").strip() == str(os.getpid()):
            LIVE_LOCK_PATH.unlink()
    except OSError:
        pass


def acquire_live_lock() -> bool:
    """One live loop per checkout. A stale lock from a dead PID is replaced."""
    LIVE_LOCK_PATH.parent.mkdir(parents=True, exist_ok=True)
    for _ in range(2):
        try:
            fd = os.open(LIVE_LOCK_PATH, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            os.write(fd, str(os.getpid()).encode("ascii"))
            os.close(fd)
            atexit.register(_release_live_lock)
            return True
        except FileExistsError:
            try:
                other = int(LIVE_LOCK_PATH.read_text(encoding="utf-8").strip())
            except (OSError, ValueError):
                other = 0
            if other and pid_alive(other):
                print(f"[LIVE] another live loop is running pid={other}; refusing",
                      file=sys.stderr)
                return False
            try:
                LIVE_LOCK_PATH.unlink()
            except OSError:
                pass
    print("[LIVE] could not acquire the live loop lock", file=sys.stderr)
    return False


def plan_claims() -> list[tuple[str, str, str, str]]:
    """Return (pattern, owner, status, plan_id) for active/ready plans."""
    if not PLANS_PATH.is_file():
        return []
    try:
        data = json.loads(PLANS_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return []
    claims: list[tuple[str, str, str, str]] = []
    for plan in data.get("plans", []):
        status = str(plan.get("status", ""))
        if status not in ("active", "ready", "ready-for-build"):
            continue
        owner = str(plan.get("owner", ""))
        plan_id = str(plan.get("plan_id", ""))
        for pattern in plan.get("paths", []) or []:
            claims.append((str(pattern).replace("\\", "/").lstrip("./"),
                           owner, status, plan_id))
    return claims


def claim_for(path: str, claims: list[tuple[str, str, str, str]]):
    path = path.replace("\\", "/")
    for pattern, owner, status, plan_id in claims:
        if pattern.endswith("/**"):
            if path.startswith(pattern[:-3] + "/"):
                return owner, status, plan_id
        elif any(ch in pattern for ch in "*?["):
            if fnmatch.fnmatchcase(path, pattern):
                return owner, status, plan_id
        elif path == pattern:
            return owner, status, plan_id
    return None


def preflight_owners(report: dict, agent: str, allow_other_owners: bool) -> bool:
    """Refuse a build when a changed file belongs to another active owner."""
    claims = plan_claims()
    problems: list[str] = []
    for entry in report.get("touched_sources", []):
        path = str(entry.get("path", ""))
        claim = claim_for(path, claims)
        if claim is None:
            continue
        owner, status, plan_id = claim
        if any(path.startswith(prefix) for prefix in SHARED_PREFIXES) and agent != "integrator":
            problems.append(f"{path} is a shared path ({plan_id}); integrator only")
        elif owner != agent and status == "active" and not allow_other_owners:
            problems.append(f"{path} belongs to {owner} ({plan_id}, active)")
        else:
            print(f"[LIVE] owner {path} -> {owner} ({plan_id}, {status})")
    if problems:
        for problem in problems:
            print(f"[LIVE] OWNER REFUSED {problem}", file=sys.stderr)
        print("[LIVE] no build requested; mark the owning plan ready or pass "
              "--allow-other-owners", file=sys.stderr)
        return False
    return True


def run_cycle(out_root: Path, last_manifest_path: Path, dry_run: bool,
              agent: str = "agent-a",
              allow_other_owners: bool = False) -> dict | None:
    if not sync_state_codegen(dry_run):
        return None
    manifest = build_manifest(ROOT)
    errors, warnings = validate(manifest)
    for warning in warnings:
        print(f"[LIVE] WARN {warning}")
    if errors:
        for error in errors:
            print(f"[LIVE] ERROR {error}", file=sys.stderr)
        print("[LIVE] unknown or duplicate ownership; no build requested", file=sys.stderr)
        return None

    previous = read_json(last_manifest_path) if last_manifest_path.is_file() else None
    if previous and previous.get("revision") == manifest["revision"]:
        return manifest

    if previous:
        report = diff_manifests(previous, manifest)
        print(f"[LIVE] change {report['old_revision']} -> {report['new_revision']} "
              f"added={len(report['added'])} removed={len(report['removed'])} "
              f"changed={len(report['changed'])} headers={len(report['changed_headers'])}")
        for path in report["added"]:
            print(f"[LIVE]   added {path}")
        for path in report["removed"]:
            print(f"[LIVE]   removed {path}")
        for entry in report["renamed"]:
            print(f"[LIVE]   renamed {entry['from']} -> {entry['to']}")
    else:
        report = {
            "old_revision": None,
            "new_revision": manifest["revision"],
            "initial": True,
            "added": [], "removed": [], "renamed": [], "changed": [],
            "added_headers": [], "removed_headers": [], "changed_headers": [],
            "changed_owners": [],
        }
        print(f"[LIVE] initial snapshot revision={manifest['revision']}")
    old_owners = {item["path"]: item.get("owner", "")
                  for item in (previous or {}).get("sources", [])}
    new_owners = {item["path"]: item.get("owner", "")
                  for item in manifest.get("sources", [])}
    touched = ([(path, new_owners.get(path, "")) for path in report["added"]]
               + [(path, old_owners.get(path, "")) for path in report["removed"]]
               + [(path, new_owners.get(path, "")) for path in report["changed"]]
               + [(path, new_owners.get(path, ""))
                  for path in report["changed_headers"]]
               + [(entry["from"], old_owners.get(entry["from"], ""))
                  for entry in report["renamed"]]
               + [(entry["to"], new_owners.get(entry["to"], ""))
                  for entry in report["renamed"]]
               + [(entry["path"], entry.get("to", ""))
                  for entry in report.get("changed_owners", [])])
    report["touched_sources"] = [
        {"path": path, "owner": owner} for path, owner in touched]
    if not preflight_owners(report, agent, allow_other_owners):
        return None
    report["game_only"] = bool(touched) and all(
        owner == "game" for _, owner in touched) and not report["changed_headers"]
    report["exe_rebuild_required"] = (
        not report.get("initial", False) and
        (not report["game_only"] or bool(report["changed_headers"])))
    report["generated_utc"] = utc_now()
    if report.get("game_only"):
        print("[LIVE] change is game-only; EXE rebuild is not required")

    destination = snapshot_path(out_root, manifest["revision"])
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    report_path = destination.parent / "change-report.json"
    write_json_atomic(report_path, report)

    if dry_run:
        print(f"[LIVE] dry-run: would build revision={manifest['revision']}")
        return manifest

    process = spawn_build(manifest["revision"], out_root, report_path)
    return {"manifest": manifest, "process": process}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="MiMITA live whole-game coordinator")
    parser.add_argument("--once", action="store_true", help="scan once and exit")
    parser.add_argument("--dry-run", action="store_true", help="detect changes but do not build")
    parser.add_argument("--out", default="build/game")
    parser.add_argument("--poll", type=float, default=0.4, help="watch poll seconds")
    parser.add_argument("--settle", type=float, default=0.6, help="save debounce seconds")
    parser.add_argument("--status", action="store_true")
    parser.add_argument("--journal", nargs="?", const="", default=None,
                        help="print recent events.jsonl records (optional event filter)")
    parser.add_argument("--journal-lines", type=int, default=40)
    parser.add_argument("--agent", default="integrator",
                        help="owner identity used for the plan preflight")
    parser.add_argument("--allow-other-owners", action="store_true",
                        help="build files claimed by another active plan")
    parser.add_argument("--verify-server", action="store_true",
                        help="after --once publishes, run the hot server probe")
    parser.add_argument("--verify-client", action="store_true",
                        help="run the probe with a real client join")
    parser.add_argument("--verify-chat", default="",
                        help="send this chat message in the client probe")
    parser.add_argument("--verify-swap", action="store_true",
                        help="require a distinct-module generation swap")
    parser.add_argument("--verify-swap-dll", default="",
                        help="specific DLL to swap to")
    parser.add_argument("--verify-runtime-revision", type=int, default=0,
                        help="require this runtime_revision after the swap")
    args = parser.parse_args(argv)

    out_root = (ROOT / args.out).resolve() if not Path(args.out).is_absolute() else Path(args.out)
    last_manifest_path = out_root / "last-manifest.json"

    if args.journal is not None:
        return tail_journal(args.journal, args.journal_lines)

    if args.status:
        current = read_json(out_root / "current.json") or {}
        ready = read_json(out_root / "ready.json") or {}
        print(f"[LIVE] current generation={current.get('generation')} "
              f"revision={current.get('revision')} dll={current.get('dll')}")
        print(f"[LIVE] ready generation={ready.get('generation')} "
              f"revision={ready.get('revision')}")
        report = ready.get("change_report") or current.get("change_report") or {}
        if report:
            print(f"[LIVE] change {report.get('old_revision')} -> "
                  f"{report.get('new_revision')} "
                  f"added={len(report.get('added', []))} "
                  f"removed={len(report.get('removed', []))} "
                  f"changed={len(report.get('changed', []))} "
                  f"headers={len(report.get('changed_headers', []))}")
        return 0

    if not acquire_live_lock():
        return 2

    if args.once:
        result = run_cycle(out_root, last_manifest_path, args.dry_run,
                           args.agent, args.allow_other_owners)
        if args.dry_run:
            return 0
        if isinstance(result, dict) and result.get("process"):
            result["process"].wait()
            if result["process"].returncode != 0:
                return result["process"].returncode
            publish_ready(out_root, result["manifest"]["revision"])
            write_json_atomic(last_manifest_path, result["manifest"])
        wants_probe = (args.verify_server or args.verify_client
                       or bool(args.verify_chat) or args.verify_swap)
        if wants_probe:
            command = [
                sys.executable,
                str(ROOT / "devscripts" / "server_hotness_probe.py"),
                "--json", str(out_root / "server-probe.json"),
            ]
            if args.verify_client or args.verify_chat or args.verify_swap:
                command.append("--client")
            if args.verify_chat:
                command.extend(["--chat", args.verify_chat])
            if args.verify_swap:
                command.append("--swap")
            if args.verify_swap_dll:
                command.extend(["--swap-dll", args.verify_swap_dll])
            if args.verify_runtime_revision:
                command.extend(["--expect-runtime-revision",
                                str(args.verify_runtime_revision)])
            probe = subprocess.run(command, cwd=str(ROOT))
            return probe.returncode
        return 0

    print(f"[LIVE] watching {ROOT}; poll={args.poll}s settle={args.settle}s out={out_root}")
    signature = snapshot_signature()
    processed_signature = None
    stable_since = time.monotonic()
    build_process: subprocess.Popen | None = None
    build_revision: str | None = None

    while True:
        time.sleep(args.poll)
        current_signature = snapshot_signature()

        if build_process is not None and build_process.poll() is not None:
            success = build_process.returncode == 0
            if success and build_revision:
                publish_ready(out_root, build_revision)
                manifest = read_json(snapshot_path(out_root, build_revision))
                if manifest:
                    write_json_atomic(last_manifest_path, manifest)
            else:
                print(f"[LIVE] build failed revision={build_revision} "
                      f"exit={build_process.returncode}; active generation unchanged",
                      file=sys.stderr)
            build_process = None
            build_revision = None

        if current_signature != signature:
            signature = current_signature
            stable_since = time.monotonic()
            continue

        if time.monotonic() - stable_since < args.settle:
            continue

        if current_signature == processed_signature:
            continue

        if build_process is not None:
            # Coalesce: the newest stable revision is built after the current one.
            continue

        processed_signature = current_signature
        result = run_cycle(out_root, last_manifest_path, args.dry_run,
                           args.agent, args.allow_other_owners)
        if isinstance(result, dict) and result.get("process"):
            build_process = result["process"]
            build_revision = result["manifest"]["revision"]


if __name__ == "__main__":
    raise SystemExit(main())
