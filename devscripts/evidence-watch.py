#!/usr/bin/env python3
"""Read-only live evidence observer for any running MiMITA process.

It discovers MiMITA executables under this checkout, maps each PID to its
canonical journal through run manifests or logger.started records, and exposes
list/show/tail/capture commands. It never adopts, stops, attaches to, or edits a
process, and it never creates a run or build.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable

ROOT = Path(__file__).resolve().parent.parent
LOGS = ROOT / "logs"
REGISTRY = ROOT / ".dev" / "live-evidence" / "registry.json"

if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

try:
    from devscripts.process_identity import windows_process_inventory
except Exception:  # pragma: no cover - non-Windows or import failure
    windows_process_inventory = None


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(
        timespec="milliseconds").replace("+00:00", "Z")


def parse_utc(value: str) -> float:
    if not value:
        return 0.0
    try:
        return datetime.fromisoformat(value.replace("Z", "+00:00")).timestamp()
    except ValueError:
        return 0.0


def read_json(path: Path) -> dict[str, Any] | None:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    return value if isinstance(value, dict) else None


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1 << 20), b""):
                digest.update(chunk)
    except OSError:
        return ""
    return digest.hexdigest()


def iter_events(path: Path, *, limit: int = 0) -> Iterable[dict[str, Any]]:
    try:
        with path.open("r", encoding="utf-8", errors="replace") as stream:
            for index, line in enumerate(stream):
                if limit and index >= limit:
                    break
                line = line.strip()
                if not line:
                    continue
                try:
                    value = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if isinstance(value, dict):
                    yield value
    except OSError:
        return


def journal_segments(directory: Path) -> list[Path]:
    if not directory.is_dir():
        return []
    def key(path: Path) -> tuple[int, int]:
        name = path.name
        if name == "events.jsonl":
            return (1, 0)
        try:
            return (0, int(name.split("-")[1].split(".")[0]))
        except (IndexError, ValueError):
            return (2, 0)
    return sorted(directory.glob("events*.jsonl"), key=key)


def run_manifests() -> list[dict[str, Any]]:
    manifests: list[dict[str, Any]] = []
    if not LOGS.is_dir():
        return manifests
    for path in LOGS.rglob("run.json"):
        value = read_json(path)
        if value is None:
            continue
        value["_path"] = str(path)
        manifests.append(value)
    return manifests


def manifest_index(manifests: list[dict[str, Any]]) -> dict[int, dict[str, Any]]:
    index: dict[int, dict[str, Any]] = {}
    for manifest in manifests:
        for instance in manifest.get("instances", []) or []:
            try:
                pid = int(instance.get("pid") or 0)
            except (TypeError, ValueError):
                continue
            if pid <= 0:
                continue
            index[pid] = {
                "run_id": manifest.get("run_id", ""),
                "manifest": manifest,
                "instance": instance,
            }
    return index


def logger_started_for_pid(directory: Path, pid: int) -> dict[str, Any] | None:
    for segment in journal_segments(directory):
        for event in iter_events(segment, limit=600):
            if event.get("event") != "logger.started":
                continue
            fields = event.get("fields", {})
            try:
                event_pid = int(fields.get("pid") or event.get("pid") or 0)
            except (TypeError, ValueError):
                continue
            if event_pid == pid:
                return event
    return None


def discover_processes() -> list[dict[str, Any]]:
    if windows_process_inventory is None or os.name != "nt":
        return []
    try:
        rows = windows_process_inventory()
    except OSError:
        return []
    checkout = ROOT.resolve()
    processes: list[dict[str, Any]] = []
    for row in rows:
        executable = str(row.get("executable") or "")
        try:
            Path(executable).resolve().relative_to(checkout)
        except (OSError, ValueError):
            continue
        processes.append(row)
    return processes


def build_entry(process: dict[str, Any], manifests_index: dict[int, dict[str, Any]],
                diagnostics: list[str]) -> dict[str, Any]:
    pid = int(process.get("pid") or 0)
    started = str(process.get("process_started_utc") or "")
    command_line = str(process.get("command_line") or "")
    entry: dict[str, Any] = {
        "pid": pid,
        "executable": str(process.get("executable") or ""),
        "command_line": command_line,
        "process_started_utc": started,
        "role": "",
        "run_id": "",
        "build_id": "",
        "mode": "",
        "map": "",
        "journal": "",
        "journal_segments": [],
        "journal_bytes": 0,
        "last_event_utc": "",
        "last_event": "",
        "event_age_ms": None,
        "source": "unverified",
        "diagnostics": [],
    }
    manifest = manifests_index.get(pid)
    if manifest:
        instance = manifest["instance"]
        run = manifest["manifest"]
        entry.update({
            "role": str(instance.get("role") or ""),
            "run_id": str(run.get("run_id") or manifest.get("run_id") or ""),
            "build_id": str(instance.get("build_id") or run.get("build_id") or ""),
            "mode": str(instance.get("mode") or run.get("mode") or ""),
            "map": str(instance.get("map") or run.get("map") or ""),
            "journal": str(instance.get("journal_path") or ""),
            "source": "run_manifest",
        })
    if not entry["journal"]:
        started_seconds = parse_utc(started)
        cutoff = started_seconds - 120.0 if started_seconds else 0.0
        candidates: list[Path] = []
        if LOGS.is_dir():
            for path in LOGS.rglob("events.jsonl"):
                try:
                    stat = path.stat()
                except OSError:
                    continue
                if cutoff and stat.st_mtime < cutoff:
                    continue
                candidates.append(path)
        candidates.sort(key=lambda path: path.stat().st_mtime if path.exists() else 0.0,
                        reverse=True)
        for journal in candidates[:80]:
            event = logger_started_for_pid(journal.parent, pid)
            if event is None:
                continue
            fields = event.get("fields", {})
            entry.update({
                "role": str(fields.get("process_role") or fields.get("process") or ""),
                "build_id": str(fields.get("build_id") or entry["build_id"]),
                "journal": str(fields.get("path") or journal),
                "source": "logger_started",
            })
            break
    if not entry["role"]:
        if "--server" in command_line:
            entry["role"] = "server"
        elif "--connect" in command_line:
            entry["role"] = "client"
    if entry["journal"]:
        journal_path = Path(entry["journal"])
        directory = journal_path.parent
        segments = journal_segments(directory)
        entry["journal_segments"] = [str(path) for path in segments]
        entry["journal_bytes"] = sum(path.stat().st_size for path in segments
                                     if path.exists())
        if segments:
            last = None
            for event in iter_events(segments[-1]):
                last = event
            if last:
                entry["last_event_utc"] = str(last.get("wall_time") or "")
                entry["last_event"] = str(last.get("event") or "")
                wall = parse_utc(entry["last_event_utc"])
                if wall:
                    entry["event_age_ms"] = int((time.time() - wall) * 1000)
        if not entry["event_age_ms"]:
            try:
                age_ms = int((time.time() - journal_path.stat().st_mtime) * 1000)
                entry["event_age_ms"] = age_ms
            except OSError:
                pass
    if not entry["journal"]:
        entry["diagnostics"].append("journal not found for this PID")
    if entry["role"] not in ("server", "client"):
        entry["diagnostics"].append("role unverified")
    return entry


def discover(diagnostics: list[str]) -> list[dict[str, Any]]:
    manifests = run_manifests()
    index = manifest_index(manifests)
    entries = [build_entry(process, index, diagnostics)
               for process in discover_processes()]
    return sorted(entries, key=lambda row: (row["role"] or "", row["pid"]))


def find_entry(selector: str) -> dict[str, Any] | None:
    diagnostics: list[str] = []
    entries = discover(diagnostics)
    for entry in entries:
        if str(entry["pid"]) == selector or entry["run_id"] == selector:
            return entry
    return None


def print_table(entries: list[dict[str, Any]]) -> None:
    print(f"{'pid':>7}  {'role':<7}  {'run':<22}  {'mode':<22}  {'map':<18}  "
          f"{'age_ms':>8}  journal")
    for entry in entries:
        age = entry.get("event_age_ms")
        print(f"{entry['pid']:>7}  {entry.get('role') or '?':<7}  "
              f"{entry.get('run_id') or '-':<22}  "
              f"{entry.get('mode') or '-':<22}  "
              f"{entry.get('map') or '-':<18}  "
              f"{str(age) if age is not None else '-':>8}  "
              f"{entry.get('journal') or '(no journal)'}")
        for note in entry.get("diagnostics", []):
            print(f"         ! {note}")


def print_show(entry: dict[str, Any]) -> None:
    print(json.dumps(entry, indent=2, sort_keys=True))


def match_event(event: dict[str, Any], args: argparse.Namespace) -> bool:
    if args.event and event.get("event") != args.event:
        return False
    if args.actor:
        fields = event.get("fields", {})
        value = (fields.get("actor") or fields.get("actor_id") or
                 event.get("correlation_id"))
        if str(value) != str(args.actor):
            return False
    if args.since:
        value = parse_utc(str(event.get("wall_time") or ""))
        if value and value < parse_utc(args.since):
            return False
    if args.until:
        value = parse_utc(str(event.get("wall_time") or ""))
        if value and value > parse_utc(args.until):
            return False
    return True


def tail_entry(entry: dict[str, Any], args: argparse.Namespace) -> int:
    journal = entry.get("journal") or ""
    if not journal:
        print("No journal found for this process.", file=sys.stderr)
        return 2
    directory = Path(journal).parent
    printed = 0
    while True:
        for segment in journal_segments(directory):
            for event in iter_events(segment):
                if not match_event(event, args):
                    continue
                print(json.dumps(event, separators=(",", ":")))
                printed += 1
                if args.lines and printed >= args.lines:
                    return 0
        if not args.follow:
            return 0
        time.sleep(max(0.2, args.interval))
        if args.lines and printed >= args.lines:
            return 0


def capture_entry(entry: dict[str, Any], out_dir: Path) -> int:
    if not entry.get("journal"):
        print("No journal found for this process.", file=sys.stderr)
        return 2
    out_dir.mkdir(parents=True, exist_ok=True)
    manifest = {
        "captured_utc": utc_now(),
        "checkout_root": str(ROOT),
        "process": entry,
        "files": [],
    }
    for path_text in entry.get("journal_segments", []):
        path = Path(path_text)
        if not path.is_file():
            continue
        target = out_dir / "journals" / path.name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        manifest["files"].append({
            "source": str(path),
            "copy": str(target),
            "bytes": target.stat().st_size,
            "sha256": sha256_file(target),
        })
    manifest_path = out_dir / "evidence.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n",
                             encoding="utf-8")
    print(f"Evidence bundle: {out_dir}")
    print(f"Manifest: {manifest_path}")
    return 0


def write_registry(entries: list[dict[str, Any]], diagnostics: list[str]) -> None:
    REGISTRY.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "schema_version": 1,
        "generated_utc": utc_now(),
        "checkout_root": str(ROOT),
        "diagnostics": diagnostics,
        "processes": entries,
    }
    temporary = REGISTRY.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n",
                         encoding="utf-8")
    temporary.replace(REGISTRY)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Read-only evidence observer for any running MiMITA process.")
    sub = parser.add_subparsers(dest="command", required=True)

    list_parser = sub.add_parser("list", help="list running MiMITA processes")
    list_parser.add_argument("--json", action="store_true")
    list_parser.add_argument("--write-registry", action="store_true")

    show_parser = sub.add_parser("show", help="show one process")
    show_parser.add_argument("selector", help="PID or run ID")
    show_parser.add_argument("--json", action="store_true")

    tail_parser = sub.add_parser("tail", help="read/tail one process journal")
    tail_parser.add_argument("selector", help="PID or run ID")
    tail_parser.add_argument("--event", default="")
    tail_parser.add_argument("--actor", default="")
    tail_parser.add_argument("--since", default="")
    tail_parser.add_argument("--until", default="")
    tail_parser.add_argument("--lines", type=int, default=0)
    tail_parser.add_argument("--follow", action="store_true")
    tail_parser.add_argument("--interval", type=float, default=1.0)

    capture_parser = sub.add_parser("capture", help="copy a bounded evidence bundle")
    capture_parser.add_argument("selector", help="PID or run ID")
    capture_parser.add_argument("--out", required=True)

    watch_parser = sub.add_parser("watch", help="periodically write the registry index")
    watch_parser.add_argument("--interval", type=float, default=5.0)
    watch_parser.add_argument("--once", action="store_true")

    args = parser.parse_args(argv)
    diagnostics: list[str] = []
    entries = discover(diagnostics)

    if args.command == "list":
        if args.json:
            print(json.dumps({"diagnostics": diagnostics, "processes": entries},
                             indent=2, sort_keys=True))
        else:
            if diagnostics:
                for note in diagnostics:
                    print(f"[EVIDENCE] ! {note}", file=sys.stderr)
            print_table(entries)
        if args.write_registry:
            write_registry(entries, diagnostics)
        return 0

    if args.command == "show":
        entry = find_entry(args.selector)
        if not entry:
            print(f"No running MiMITA process matched {args.selector!r}.",
                  file=sys.stderr)
            return 2
        print_show(entry)
        return 0

    if args.command == "tail":
        entry = find_entry(args.selector)
        if not entry:
            print(f"No running MiMITA process matched {args.selector!r}.",
                  file=sys.stderr)
            return 2
        return tail_entry(entry, args)

    if args.command == "capture":
        entry = find_entry(args.selector)
        if not entry:
            print(f"No running MiMITA process matched {args.selector!r}.",
                  file=sys.stderr)
            return 2
        return capture_entry(entry, Path(args.out))

    if args.command == "watch":
        while True:
            diagnostics = []
            entries = discover(diagnostics)
            write_registry(entries, diagnostics)
            print(f"[EVIDENCE] {len(entries)} process(es); {REGISTRY}")
            if args.once:
                return 0
            time.sleep(max(0.5, args.interval))

    return 2


if __name__ == "__main__":
    raise SystemExit(main())
