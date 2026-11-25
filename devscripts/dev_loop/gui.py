"""Native, compact Tk dashboard for the MiMITA dev-loop daemon."""

from __future__ import annotations

import ctypes
import json
import os
import queue
import re
import threading
import time
from collections import deque
from datetime import datetime, timezone
from pathlib import Path
from tkinter import simpledialog
from typing import Callable


BLACK = "#000000"
RED = "#ff0000"
GREEN = "#00ff00"
ACCENT = "#00aaff"
WHITE = "#ffffff"
FONT_PATH = Path(r"C:\important\mingliu-font\mingliu.ttf")
MIN_WIDTH = 500
MIN_HEIGHT = 500
APP_USER_MODEL_ID = "MiMITA.DevLoop.Dashboard"
UI_STATE_PATH = Path(__file__).resolve().parents[2] / ".dev" / "dashboard-ui.json"
_LIVE_ACTIVATION_CACHE: dict[str, dict] = {}


def _load_ui_state() -> dict:
    try:
        value = json.loads(UI_STATE_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def _save_ui_state(value: dict) -> None:
    try:
        UI_STATE_PATH.parent.mkdir(parents=True, exist_ok=True)
        temporary = UI_STATE_PATH.with_suffix(".json.tmp")
        temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
        temporary.replace(UI_STATE_PATH)
    except OSError:
        pass


def _monitor_work_area(hwnd: int) -> tuple[int, int, int, int] | None:
    """Return the work area of the monitor containing this window."""
    if os.name != "nt":
        return None
    try:
        from ctypes import wintypes

        class MONITORINFO(ctypes.Structure):
            _fields_ = [
                ("cbSize", wintypes.DWORD),
                ("rcMonitor", wintypes.RECT),
                ("rcWork", wintypes.RECT),
                ("dwFlags", wintypes.DWORD),
            ]

        user32 = ctypes.WinDLL("user32", use_last_error=True)
        user32.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
        user32.GetAncestor.restype = wintypes.HWND
        user32.MonitorFromWindow.argtypes = [wintypes.HWND, wintypes.DWORD]
        user32.MonitorFromWindow.restype = ctypes.c_void_p
        user32.GetMonitorInfoW.argtypes = [ctypes.c_void_p,
                                           ctypes.POINTER(MONITORINFO)]
        user32.GetMonitorInfoW.restype = wintypes.BOOL

        top_level = user32.GetAncestor(hwnd, 2) or hwnd  # GA_ROOT
        monitor = user32.MonitorFromWindow(top_level, 2)  # MONITOR_DEFAULTTONEAREST
        if not monitor:
            return None
        info = MONITORINFO()
        info.cbSize = ctypes.sizeof(info)
        if not user32.GetMonitorInfoW(monitor, ctypes.byref(info)):
            return None
        work = info.rcWork
        return (work.left, work.top,
                work.right - work.left, work.bottom - work.top)
    except (AttributeError, OSError, ValueError):
        return None


def _window_bounds(hwnd: int) -> tuple[int, int, int, int] | None:
    """Return the top-level window's absolute x, y, width, and height."""
    if os.name != "nt":
        return None
    try:
        from ctypes import wintypes

        user32 = ctypes.WinDLL("user32", use_last_error=True)
        user32.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
        user32.GetAncestor.restype = wintypes.HWND
        user32.GetWindowRect.argtypes = [wintypes.HWND,
                                         ctypes.POINTER(wintypes.RECT)]
        user32.GetWindowRect.restype = wintypes.BOOL
        top_level = user32.GetAncestor(hwnd, 2) or hwnd  # GA_ROOT
        rect = wintypes.RECT()
        if not user32.GetWindowRect(top_level, ctypes.byref(rect)):
            return None
        return (rect.left, rect.top,
                rect.right - rect.left, rect.bottom - rect.top)
    except (AttributeError, OSError, ValueError):
        return None


def _tk_geometry(width: int, height: int, x: int, y: int) -> str:
    """Format absolute Tk coordinates, including negative virtual-screen positions."""
    return f"{width}x{height}+{x}+{y}"


def _configured_launch_modes() -> list[dict[str, str]]:
    path = Path(__file__).resolve().parents[2] / "devscripts" / "dev-launch-modes.json"
    try:
        config = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return []
    modes = config.get("modes") if isinstance(config, dict) else None
    if not isinstance(modes, dict):
        return []
    ordered = sorted(modes, key=lambda value: (
        not bool(modes[value].get("clients")) if isinstance(modes[value], dict) else True,
        not str(value).isdigit(),
        int(str(value)) if str(value).isdigit() else str(value),
    ))
    return [
        {"id": str(mode_id),
         "name": str(modes[mode_id].get("name", "unnamed"))
         if isinstance(modes[mode_id], dict) else "unnamed"}
        for mode_id in ordered
    ]


def _launch_timing_from_journal(events_path: str, request_id: str,
                                launch_started_epoch: float,
                                allow_unmatched: bool = False) -> dict:
    if not events_path or not request_id:
        return {}
    try:
        with open(events_path, "rb") as handle:
            handle.seek(0, os.SEEK_END)
            size = handle.tell()
            tail_size = min(size, 262144)
            handle.seek(size - tail_size)
            lines = handle.read(tail_size).decode("utf-8", errors="replace").splitlines()
    except OSError:
        return {}
    stages = {}
    for line in lines[-600:]:
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        if record.get("event") != "dev-loop.launch.stage":
            continue
        fields = record.get("fields") if isinstance(record.get("fields"), dict) else {}
        event_request_id = str(fields.get("request_id") or "")
        if event_request_id != request_id and not (allow_unmatched and not event_request_id):
            continue
        stage_name = str(fields.get("stage") or "")
        try:
            stage_elapsed = float(fields.get("elapsed_ms")) / 1000.0
        except (TypeError, ValueError):
            try:
                event_epoch = datetime.fromisoformat(
                    str(record.get("wall_time", "")).replace("Z", "+00:00")
                ).timestamp()
            except ValueError:
                event_epoch = time.time()
            stage_elapsed = max(0.0, event_epoch - launch_started_epoch)
        accept = fields.get("accept_latency_ms")
        if isinstance(accept, (int, float)):
            stages["accept_latency_ms"] = float(accept)
        if stage_name == "client_process_start" and fields.get("result") == "started":
            stages["client_start_seconds"] = stage_elapsed
        elif stage_name == "process_reuse" and fields.get("result") == "reused":
            stages["client_reused"] = True
        elif stage_name == "complete":
            stages["status"] = "succeeded" if fields.get("result") == "succeeded" else "failed"
            stages["readiness_seconds"] = stage_elapsed
            stages["message"] = str(record.get("reason") or "")
    return stages


def available() -> bool:
    try:
        import tkinter  # noqa: F401
    except Exception:
        return False
    return True


def _private_font():
    """Load MingLiU for this window process without installing it globally."""
    if os.name != "nt" or not FONT_PATH.is_file():
        return None
    try:
        gdi = ctypes.WinDLL("gdi32", use_last_error=True)
        gdi.AddFontResourceExW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint, ctypes.c_void_p]
        gdi.AddFontResourceExW.restype = ctypes.c_int
        loaded = gdi.AddFontResourceExW(str(FONT_PATH), 0x10, None)
        return gdi if loaded else None
    except (AttributeError, OSError):
        return None


def _artifact_key(value: str) -> str:
    value = value.replace("LEGACY #", "").strip()
    if value.isdecimal():
        return str(int(value))
    return value


def _utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z")


def _age_text(value: str) -> str:
    try:
        then = datetime.fromisoformat(str(value).replace("Z", "+00:00"))
        seconds = max(0, int((datetime.now(timezone.utc) - then).total_seconds()))
    except (TypeError, ValueError):
        return "unknown"
    if seconds < 60:
        return f"{seconds}s ago"
    if seconds < 3600:
        return f"{seconds // 60}m ago"
    return f"{seconds // 3600}h ago"


def _timestamp_elapsed(value: object, now: datetime | None = None) -> float | None:
    """Return live seconds since an ISO UTC timestamp, or None if unavailable."""
    try:
        then = datetime.fromisoformat(str(value).replace("Z", "+00:00"))
        if then.tzinfo is None:
            then = then.replace(tzinfo=timezone.utc)
        return max(0.0, ((now or datetime.now(timezone.utc)) - then).total_seconds())
    except (TypeError, ValueError):
        return None


def _format_duration(seconds: float) -> str:
    tenths = max(0, int(float(seconds) * 10))
    hours, remainder = divmod(tenths, 36_000)
    minutes, remainder = divmod(remainder, 600)
    whole, decimal = divmod(remainder, 10)
    return (f"{hours:02d}:{minutes:02d}:{whole:02d}.{decimal}"
            if hours else f"{minutes:02d}:{whole:02d}.{decimal}")


def _timeline_stage_start(progress: dict, phase: str) -> str | None:
    timeline = progress.get("timeline")
    if not isinstance(timeline, list):
        return None
    for item in reversed(timeline):
        if not isinstance(item, dict):
            continue
        name = str(item.get("phase") or item.get("stage") or "").lower()
        event = str(item.get("event") or "").lower()
        state = str(item.get("state") or "").lower()
        if name == phase and (state == "started" or event.endswith(".started")):
            stamp = item.get("time_utc") or item.get("started_utc")
            if stamp:
                return str(stamp)
    return None


def _short_text(value: object, limit: int = 160) -> str:
    text = " ".join(str(value or "").split())
    return text if len(text) <= limit else text[: limit - 1] + "…"


def _latest_generation_activation(root: Path, state_data: dict) -> dict:
    run_id = str(state_data.get("run_id") or "")
    if len(run_id) < 8 or not run_id[:8].isdigit():
        return {}
    try:
        run_day = datetime.strptime(run_id[:8], "%Y%m%d").strftime("%Y-%m-%d")
    except ValueError:
        return {}
    run = {}
    try:
        run = json.loads((root / "logs" / run_day / run_id / "run.json")
                         .read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    servers = [item for item in run.get("instances", [])
               if isinstance(item, dict) and item.get("role") == "server"]
    events = []
    for server in servers:
        journal = Path(str(server.get("journal_path") or ""))
        if not journal.is_file():
            continue
        try:
            key = f"{run_id}:{server.get('instance_id', '')}:{journal}"
            cached = _LIVE_ACTIVATION_CACHE.get(key, {})
            activation = cached.get("event") or {}
            found = {}
            if activation:
                size = journal.stat().st_size
                observed = int(cached.get("observed_current_size", 0))
                next_observed = size
                if size < observed:
                    observed = 0
                if size > observed:
                    with journal.open("rb") as handle:
                        handle.seek(observed)
                        data = handle.read(size - observed)
                    complete = data.rfind(b"\n")
                    if complete >= 0:
                        next_observed = observed + complete + 1
                        data = data[:complete + 1]
                        for raw in reversed(data.splitlines()):
                            try:
                                event = json.loads(raw)
                            except (json.JSONDecodeError, UnicodeDecodeError):
                                continue
                            if event.get("event") == "game.generation.activated":
                                fields = event.get("fields") if isinstance(event.get("fields"), dict) else {}
                                found = {"generation": fields.get("generation"),
                                         "dll": fields.get("dll", ""),
                                         "time_utc": event.get("wall_time", ""),
                                         "run_id": event.get("run_id", run_id),
                                         "instance_id": event.get("instance_id", "")}
                                break
                _LIVE_ACTIVATION_CACHE[key] = {
                    "event": found or activation,
                    "observed_current_size": next_observed,
                    "scan_path": str(cached.get("scan_path") or ""),
                    "scan_cursor": int(cached.get("scan_cursor") or 0),
                }
                events.append(found or activation)
                continue

            segments = [journal]
            segments.extend(sorted(journal.parent.glob("events-*.jsonl"),
                                   key=lambda path: path.stat().st_mtime_ns,
                                   reverse=True))
            scan_path = Path(str(cached.get("scan_path") or segments[0]))
            if scan_path not in segments:
                scan_path = segments[0]
            cursor = int(cached.get("scan_cursor") or scan_path.stat().st_size)
            start = max(0, cursor - 32 * 1024 * 1024)
            with scan_path.open("rb") as handle:
                handle.seek(start)
                data = handle.read(cursor - start)
            if start > 0:
                first_line_end = data.find(b"\n")
                data = data[first_line_end + 1:] if first_line_end >= 0 else b""
        except OSError:
            continue
        for raw in reversed(data.splitlines()):
            try:
                event = json.loads(raw)
            except (json.JSONDecodeError, UnicodeDecodeError):
                continue
            if event.get("event") != "game.generation.activated":
                continue
            fields = event.get("fields") if isinstance(event.get("fields"), dict) else {}
            found = {"generation": fields.get("generation"),
                     "dll": fields.get("dll", ""),
                     "time_utc": event.get("wall_time", ""),
                     "run_id": event.get("run_id", run_id),
                     "instance_id": event.get("instance_id", "")}
            break
        if not found and start == 0:
            segment_index = segments.index(scan_path) + 1
            if segment_index < len(segments):
                scan_path = segments[segment_index]
                cursor = scan_path.stat().st_size
            else:
                cursor = 0
        elif not found:
            cursor = start
        _LIVE_ACTIVATION_CACHE[key] = {
            "event": found,
            "observed_current_size": journal.stat().st_size,
            "scan_path": str(scan_path),
            "scan_cursor": cursor,
        }
        if found:
            events.append(found)
        else:
            events.append({"scan_pending": True,
                           "instance_id": server.get("instance_id", "")})
    return max(events, key=lambda item: str(item.get("time_utc") or "")) if events else {}


def _recent_game_generations(root: Path) -> list[dict]:
    summaries = root / ".dev" / "build-timings"
    try:
        paths = sorted(summaries.glob("game-dll-*.json"),
                       key=lambda path: path.stat().st_mtime_ns, reverse=True)[:8]
    except OSError:
        return []
    recent = []
    for path in paths:
        try:
            item = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        if isinstance(item, dict) and item.get("generation") is not None:
            recent.append(item)
    return recent[:5]


def _game_dll_progress(checkout: str) -> tuple[str, str, dict]:
    """Read the root dev-loop's live whole-game DLL progress and identity."""
    if not checkout:
        return "Game DLL: unavailable", "No checkout path in status", {}
    root = Path(checkout)

    def read_json(path: Path) -> dict:
        try:
            value = json.loads(path.read_text(encoding="utf-8"))
            return value if isinstance(value, dict) else {}
        except (OSError, json.JSONDecodeError):
            return {}

    progress_path = root / "build" / "game" / "progress.json"
    progress = read_json(progress_path)
    current = read_json(root / "build" / "game" / "current.json")
    state_data = read_json(root / ".dev" / "state.json")
    change_report = read_json(root / "build" / "hot-reload" / "change-report.json")
    activation = _latest_generation_activation(root, state_data)
    history = _recent_game_generations(root)
    for item in history:
        if item.get("build_attempt_id") == progress.get("build_attempt_id"):
            item["phase_durations_ms"] = progress.get("phase_durations_ms", {})
            item["subphase_durations_ms"] = progress.get("subphase_durations_ms", {})
    try:
        progress_age_s = max(0.0, time.time() - progress_path.stat().st_mtime)
    except OSError:
        progress_age_s = None
    pending_notes = []
    for path in sorted((root / ".dev" / "build-notes" / "pending").glob("*.json"))[-8:]:
        note = read_json(path)
        if note:
            pending_notes.append(note)
    if not progress:
        return ("Game DLL: no active build progress",
                "Waiting for root dev-loop game build",
                {"pending_notes": pending_notes})

    progress_status = str(progress.get("status") or "").lower()
    phase = str(progress.get("phase") or progress_status or "unknown").upper()
    scan_phase = str(progress.get("scan_phase") or "").lower()
    active = progress_status in ("preparing", "running")
    manifest_progress = progress.get("manifest_progress")
    if not isinstance(manifest_progress, dict):
        manifest_progress = {}
    manifest_subphase = progress.get("subphase")
    if not isinstance(manifest_subphase, dict):
        manifest_subphase = {}
    revision = str(progress.get("revision") or "—")
    current_revision = str(current.get("revision") or "")
    source_count = int(progress.get("total") or 0)
    completed = int(progress.get("completed") or 0)
    scan_parsed = int(progress.get("scan_parsed") or 0)
    scan_total = int(progress.get("scan_total") or 0)
    progress_age_text = ("unknown" if progress_age_s is None
                         else f"{progress_age_s:.0f}s")
    if not active:
        health = "FAILED" if progress_status == "failed" else "PUBLISHED"
    elif progress_age_s is not None and progress_age_s >= 120:
        health = "NO PROGRESS " + progress_age_text
    elif progress_age_s is not None and progress_age_s >= 30:
        health = "SLOW " + progress_age_text
    else:
        health = "LIVE"
    if (current_revision == revision and current.get("source_count") is not None
            and not active):
        source_count = int(current.get("source_count") or source_count)
        completed = max(completed, source_count)
    percent = min(100, round(completed * 100 / source_count)) if source_count > 0 else None
    if progress.get("no_op") or progress_status == "no_op":
        headline = "Game DLL: NO-OP · published generation reused"
    elif scan_phase == "dependency_prepass":
        parsed_text = f"{scan_parsed}/{scan_total}" if scan_total else str(scan_parsed)
        headline = f"Game DLL: DEPENDENCY PREPASS {parsed_text}  ·  {health}"
    elif active and phase == "MANIFEST_SNAPSHOT":
        manifest_subphase_name = str(manifest_subphase.get("subphase") or "starting")
        manifest_done = int(manifest_progress.get("completed") or 0)
        manifest_total = int(manifest_progress.get("total") or 0)
        manifest_percent = manifest_progress.get("percent")
        if manifest_total > 0 and manifest_percent is not None:
            headline = (f"Game DLL: MANIFEST {manifest_subphase_name} "
                        f"{manifest_done}/{manifest_total} ({int(manifest_percent)}%)  ·  {health}")
        else:
            headline = f"Game DLL: MANIFEST {manifest_subphase_name}  ·  {health}"
    elif active and percent is not None:
        headline = f"Game DLL: {phase} {completed}/{source_count} ({percent}%)  ·  {health}"
    else:
        headline = f"Game DLL: {phase}  ·  {health}"

    details = [
        f"live: {health} · last progress {progress_age_text} ago",
        f"source gen {progress.get('source_generation') or state_data.get('source_generation') or '—'}"
        f" · published gen {current.get('generation') or '—'}",
        f"revision {revision[:12]}",
    ]
    if isinstance(progress.get("worker_elapsed_s"), (int, float)):
        details.append(f"worker elapsed {float(progress['worker_elapsed_s']):.3f}s")
    if isinstance(progress.get("elapsed_s"), (int, float)):
        attempt_elapsed_s = float(progress["elapsed_s"]) + (progress_age_s or 0.0)
        details.append(f"attempt elapsed {attempt_elapsed_s:.1f}s")
    if active and phase == "MANIFEST_SNAPSHOT" and manifest_subphase:
        started_monotonic = manifest_subphase.get("started_monotonic")
        if isinstance(started_monotonic, (int, float)):
            subphase_elapsed_s = max(0.0, time.monotonic() - float(started_monotonic))
        else:
            subphase_elapsed_s = float(manifest_subphase.get("elapsed_ms") or 0) / 1000.0
        subphase_text = (f"manifest {manifest_subphase.get('subphase', 'unknown')} "
                         f"· {subphase_elapsed_s:.1f}s")
        if int(manifest_progress.get("total") or 0) > 0:
            subphase_text += (f" · {int(manifest_progress.get('completed') or 0)}"
                              f"/{int(manifest_progress.get('total') or 0)}"
                              f" ({int(manifest_progress.get('percent') or 0)}%)")
        details.append(subphase_text)
    active_sources = progress.get("active_sources")
    if isinstance(active_sources, list) and active_sources:
        details.append("compiling: " + ", ".join(str(item) for item in active_sources[:3]))
    elif progress.get("scan_current"):
        details.append("prepass current: " + str(progress["scan_current"]))
    elif progress.get("current"):
        details.append("current: " + str(progress["current"]))
    if progress.get("compiled") is not None or progress.get("skipped") is not None:
        details.append(f"CXX {int(progress.get('compiled') or 0)}  ·  SKIP {int(progress.get('skipped') or 0)}")
    if scan_phase == "dependency_prepass":
        details.append(
            f"dependency prepass {scan_parsed}/{scan_total or '?'} sourced files"
            f"  ·  {float(progress.get('scan_prepass_elapsed_s') or 0):.1f}s")
    if isinstance(progress.get("object_scan_s"), (int, float)):
        scan_detail = f"scan {float(progress['object_scan_s']):.3f}s"
        if isinstance(progress.get("scan_unique_dependencies"), int):
            scan_detail += f"  ·  {progress['scan_unique_dependencies']} unique deps"
        if isinstance(progress.get("scan_parse_ms"), (int, float)):
            scan_detail += f"  ·  parse {float(progress['scan_parse_ms']):.0f} ms"
        if isinstance(progress.get("scan_stat_ms"), (int, float)):
            scan_detail += f"  ·  stat {float(progress['scan_stat_ms']):.0f} ms"
        details.append(scan_detail)
    file_events = progress.get("file_events")
    if isinstance(file_events, list) and file_events:
        recent = []
        for item in file_events[-4:]:
            if not isinstance(item, dict):
                continue
            marker = "SKIP" if item.get("status") == "skipped" else "CXX"
            seconds = float(item.get("seconds") or 0)
            recent.append(f"{marker} {seconds:.2f}s {item.get('path', '')}")
        if recent:
            details.append("latest files: " + "  ·  ".join(recent))
    notes = progress.get("notes") if isinstance(progress.get("notes"), list) else []
    if notes:
        details.extend("change note: " + str(item.get("changes", ""))
                       for item in notes if isinstance(item, dict))
    else:
        details.append("change note: none; use devscripts/build-note.py --changes \"...\"")
    if current_revision == revision and current.get("status") == "ok":
        details.append(f"published generation {current.get('generation')}  ·  DLL SHA256 {str(current.get('code_hash') or '—')[:16]}")
    if str(change_report.get("new_revision") or "") == revision:
        changed = (change_report.get("changed") or []) + (change_report.get("added") or [])
        changed += change_report.get("changed_headers") or []
        if changed:
            details.append("changed: " + ", ".join(str(path) for path in changed[:3]))
            if len(changed) > 3:
                details[-1] += f" (+{len(changed) - 3} more)"
    details.append("CXX = compiled  ·  SKIP = existing dependency-clean object reused")
    progress["latest_change_report"] = change_report
    progress["published_generation"] = current.get("generation")
    progress["latest_live_activation"] = activation
    progress["generation_history"] = history
    progress["pending_notes"] = pending_notes
    progress["build_health"] = health
    progress["progress_age_s"] = progress_age_s
    progress["source_generation"] = (
        progress.get("source_generation") or state_data.get("source_generation"))
    if activation and activation.get("generation") is not None:
        active_gen = activation.get("generation")
        published_gen = current.get("generation")
        live_text = (f"latest live activation G{active_gen} at "
                     f"{activation.get('time_utc') or 'unknown'}")
        if active_gen != published_gen:
            live_text += f" · published G{published_gen} is not yet proven active"
        details.append(live_text)
    elif activation and activation.get("scan_pending"):
        details.append("latest live activation: scanning server journal history")
    else:
        details.append("latest live activation: no game.generation.activated event found")
    return headline, "\n".join(details), progress


def _game_dll_queue_view(model: dict, progress: dict) -> tuple[str, str, str, float | None, str]:
    game_build = model.get("game_build") if isinstance(model.get("game_build"), dict) else {}
    requests = game_build.get("requests") if isinstance(game_build.get("requests"), dict) else {}
    queued = 1 if any(value == "queued" for value in requests.values()) else 0
    progress_status = str(progress.get("status") or "").lower()
    daemon_status = str(game_build.get("status") or "").lower()
    active = 1 if daemon_status == "building" or progress_status in (
        "preparing", "running") else 0
    queue_summary = f"{active} active DLL build  ·  {queued} queued generation"
    generation = progress.get("generation") or game_build.get("generation")
    health = str(progress.get("build_health") or "").upper()
    scan_phase = str(progress.get("scan_phase") or "").lower()
    progress_age = progress.get("progress_age_s")
    age_text = (f"{float(progress_age):.0f}s" if isinstance(progress_age, (int, float))
                else "unknown")
    if active:
        job_title = f"BUILDING game-g{generation}.dll" if generation else "BUILDING game DLL · selecting generation"
        if scan_phase == "dependency_prepass":
            job_title += "  ·  DEPENDENCY PREPASS"
        if health.startswith("NO PROGRESS"):
            job_title += f"  ·  NO PROGRESS {age_text}"
        elif health.startswith("SLOW"):
            job_title += f"  ·  SLOW {age_text}"
    elif queued:
        job_title = "QUEUED · edits coalesce into the next game DLL generation"
    elif daemon_status == "failed" or progress_status == "failed":
        job_title = "LAST GAME DLL BUILD FAILED"
    elif generation:
        job_title = f"IDLE · last published game-g{generation}.dll"
    else:
        job_title = "IDLE · no game DLL build queued"

    phase = str(progress.get("phase") or daemon_status or "idle").replace("_", " ").upper()
    elapsed = progress.get("elapsed_s")
    now = datetime.now(timezone.utc)
    total = _timestamp_elapsed(progress.get("started_utc"), now) if active else None
    if total is None and isinstance(elapsed, (int, float)):
        total = float(elapsed)
        if active:
            total += _timestamp_elapsed(progress.get("updated_utc"), now) or 0.0
    if total is None:
        total = game_build.get("last_duration_seconds")
    total_text = (_format_duration(float(total)) if isinstance(total, (int, float))
                  else "unavailable")

    phase_started = (progress.get("phase_started_utc")
                     or _timeline_stage_start(progress, str(progress.get("phase") or "").lower()))
    phase_elapsed = _timestamp_elapsed(phase_started, now) if active else None
    if phase_elapsed is None and isinstance(progress.get("phase_elapsed_ms"), (int, float)):
        phase_elapsed = float(progress["phase_elapsed_ms"]) / 1000.0
    phase_text = _format_duration(phase_elapsed) if phase_elapsed is not None else "not reported"
    subphase = str(progress.get("subphase") or "").strip()
    subphase_elapsed = (_timestamp_elapsed(progress.get("subphase_started_utc"), now)
                        if active and subphase else None)
    if subphase_elapsed is None and subphase and isinstance(
            progress.get("subphase_elapsed_ms"), (int, float)):
        subphase_elapsed = float(progress["subphase_elapsed_ms"]) / 1000.0
    timer = (f"G{generation}  ·  ATTEMPT {total_text}  ·  {'RUNNING' if active else 'TOTAL'}"
             if generation else f"DLL build  ·  ATTEMPT {total_text}  ·  {'RUNNING' if active else 'TOTAL'}")
    timer += f"\nPHASE {phase} {phase_text}"
    if subphase:
        subphase_text = (_format_duration(subphase_elapsed)
                         if subphase_elapsed is not None else "duration not reported")
        timer += f"  ·  SUBPHASE {subphase} {subphase_text}"
    updated = str(progress.get("last_progress_utc")
                  or progress.get("updated_utc")
                  or model.get("updated_at") or "unknown")
    stamp = (f"as of {updated}  ·  progress age {age_text}  ·  "
             f"health {health or 'UNKNOWN'}  ·  phase {phase}")
    total = int(progress.get("total") or 0)
    completed = int(progress.get("completed") or 0)
    scan_parsed = int(progress.get("scan_parsed") or 0)
    scan_total = int(progress.get("scan_total") or 0)
    ratio = None
    if active and scan_phase == "dependency_prepass":
        ratio = (min(1.0, max(0.0, scan_parsed / scan_total))
                 if scan_total else None)
        percent_text = f"  ·  {round(ratio * 100)}% of depfiles" if ratio is not None else ""
        text = (f"DEPENDENCY PREPASS  ·  {scan_parsed}/{scan_total or '?'} depfiles"
                f" parsed{percent_text}")
    elif active and total > 0 and phase in ("OBJECT CACHE SCAN", "COMPILE"):
        ratio = min(1.0, max(0.0, completed / total))
        text = f"{phase}  ·  {completed}/{total} source units  ·  {round(ratio * 100)}%"
    elif active:
        text = f"{phase}  ·  no completion percentage for this phase"
    else:
        text = job_title
    return queue_summary, job_title, stamp + "\n" + text, ratio, timer
def run(status_provider: Callable[[], dict],
        control: Callable[..., dict | bool],
        title: str,
        process_provider: Callable[[], list[dict]] | None = None) -> int:
    import tkinter as tk

    if os.name == "nt":
        try:
            shell = ctypes.WinDLL("shell32", use_last_error=True)
            set_app_id = shell.SetCurrentProcessExplicitAppUserModelID
            set_app_id.argtypes = [ctypes.c_wchar_p]
            set_app_id.restype = ctypes.c_long
            result = set_app_id(APP_USER_MODEL_ID)
            if result < 0:
                raise OSError(result, "could not register dashboard app identity")
        except (AttributeError, OSError) as error:
            # The top-level WS_EX_APPWINDOW style still keeps the window in the
            # taskbar and Alt+Tab if the optional shell grouping ID is unavailable.
            app_id_error = str(error)
        else:
            app_id_error = ""
    else:
        app_id_error = ""

    font_library = _private_font()
    family = "MingLiU" if font_library else "Courier New"
    saved_ui_state = _load_ui_state()
    saved_bounds = saved_ui_state.get("window_bounds")
    saved_geometry = str(saved_ui_state.get("geometry") or "")
    if (isinstance(saved_bounds, dict)
            and all(type(saved_bounds.get(key)) is int
                    for key in ("x", "y", "width", "height"))
            and saved_bounds["width"] >= MIN_WIDTH
            and saved_bounds["height"] >= MIN_HEIGHT):
        saved_geometry = _tk_geometry(
            saved_bounds["width"], saved_bounds["height"],
            saved_bounds["x"], saved_bounds["y"])
    elif not (re.fullmatch(r"\d+x\d+\+-?\d+\+-?\d+", saved_geometry)
              or re.fullmatch(r"\d+x\d+[+-]\d+[+-]\d+", saved_geometry)):
        saved_geometry = "880x760"
    root = tk.Tk()
    root.title(title)
    root.overrideredirect(True)
    root.resizable(True, True)
    root.minsize(MIN_WIDTH, MIN_HEIGHT)
    root.geometry(saved_geometry)
    root.configure(background=BLACK)

    state = {
        "frozen": False,
        "after_id": None,
        "refresh_after_id": None,
        "reflow_after_id": None,
        "last_reflow_size": None,
        "last_reflow_wrap": None,
        "closing": False,
        "maximized": False,
        "restore_geometry": None,
        "window_geometry": saved_geometry,
        "window_bounds": saved_bounds if isinstance(saved_bounds, dict) else None,
        "geometry_save_after": None,
        "drag": None,
        "resize": None,
        "compact": None,
        "wide": None,
        "app_id_error": app_id_error,
        "minimized": False,
        "pending_request_id": "",
        "pending_checkout_id": "main",
        "launch_started_monotonic": None,
        "launch_started_epoch": None,
        "current_events_path": "",
        "launch_events_path_before": "",
        "launch_request_id": "",
        "launch_client_started_elapsed": None,
        "launch_client_reused": False,
        "launch_readiness_elapsed": None,
        "launch_timing_status": "",
        "launch_timing_message": "",
        "launch_ack_elapsed": None,
        "control_queue": queue.Queue(),
        "control_results": queue.SimpleQueue(),
        "launch_modes": [],
        "active_mode": "",
        "latest_model": {},
        "status_refresh_queue": queue.SimpleQueue(),
        "status_refresh_in_flight": False,
        "status_refresh_last_started": 0.0,
        "status_refresh_error": "",
        "provider_control_lock": threading.RLock(),
        "last_slow_refresh_log": 0.0,
        "processes": [],
        "live_output": deque(maxlen=300),
        "last_live_message": "",
        "gutter_signature": None,
        "process_rows": {},
        "rendered_processes": [],
        "selected_process_key": "",
        "process_scan_queue": queue.SimpleQueue(),
        "process_scan_in_flight": False,
        "process_scan_last_started": 0.0,
        "process_scan_last_success": "",
        "process_scan_last_success_monotonic": 0.0,
        "process_scan_error": "",
        "checkout_rows": {},
        "dashboard_refresh_monotonic": 0.0,
        "dashboard_refresh_utc": "",
        "dashboard_refresh_count": 0,
        "collapsed": dict(saved_ui_state.get("collapsed", {}) or {}),
        "coordination_store": None,
        "coordination_error": "",
        "coordination_snapshot": {},
        "coordination_agent_rows": {},
        "coordination_recipient_names": (),
        "coordination_request_queue": queue.Queue(maxsize=1),
        "coordination_result_queue": queue.SimpleQueue(),
        "coordination_reader_started": False,
        "coordination_snapshot_in_flight": False,
        "coordination_request_last_monotonic": 0.0,
        "coordination_render_in_progress": False,
        "coordination_render_after_id": None,
        "checkout_render_in_progress": False,
        "checkout_render_after_id": None,
        "checkout_render_generation": 0,
        "last_coordination_log": 0.0,
        "coordination_send_status": "",
        "coordination_recipient": "all",
        "scroll_canvases": [],
    }

    def log_dashboard_event(event: str, fields: dict, *, cooldown_key: str = "",
                            cooldown_seconds: float = 1.0) -> None:
        now = time.monotonic()
        if cooldown_key:
            last = float(state.get(cooldown_key) or 0.0)
            if now - last < cooldown_seconds:
                return
            state[cooldown_key] = now
        record = {
            "category": "DASHBOARD",
            "event": event,
            "fields": fields,
            "pid": os.getpid(),
            "wall_time": datetime.now(timezone.utc).isoformat(
                timespec="milliseconds").replace("+00:00", "Z"),
        }
        try:
            print(json.dumps(record, separators=(",", ":")), flush=True)
        except (OSError, TypeError, ValueError):
            pass

    def measure_ui_stage(name: str, operation):
        started = time.perf_counter()
        try:
            return operation()
        finally:
            elapsed_ms = round((time.perf_counter() - started) * 1000.0, 1)
            if elapsed_ms >= 50.0:
                log_dashboard_event(
                    "dashboard.stage.slow",
                    {"stage": name, "duration_ms": elapsed_ms,
                     "refresh_number": state["dashboard_refresh_count"]},
                    cooldown_key=f"last_stage_log_{name}",
                    cooldown_seconds=2.0)

    def request_status_refresh() -> None:
        if state["status_refresh_in_flight"] or state["closing"]:
            return
        state["status_refresh_in_flight"] = True
        started = time.monotonic()
        state["status_refresh_last_started"] = started

        def read_status() -> None:
            model = {}
            error_text = ""
            try:
                with state["provider_control_lock"]:
                    value = status_provider() or {}
                model = value if isinstance(value, dict) else {}
                try:
                    headline, detail, progress = _game_dll_progress(
                        str(model.get("checkout") or ""))
                    summary, job_title, stamp, ratio, timer = _game_dll_queue_view(
                        model, progress)
                    model["_game_queue_snapshot"] = {
                        "headline": headline, "detail": detail, "progress": progress,
                        "summary": summary, "job_title": job_title,
                        "stamp": stamp, "ratio": ratio, "timer": timer,
                    }
                except Exception as error:  # noqa: BLE001 - game progress is ancillary
                    model["_game_queue_snapshot"] = {
                        "headline": "Game DLL: progress unavailable",
                        "detail": str(error)[:160], "progress": {},
                        "summary": "Build queue status unavailable",
                        "job_title": "Queue read failed", "stamp": "", "ratio": None,
                        "timer": "DLL build timer unavailable",
                    }
            except Exception as error:  # noqa: BLE001 - surface without blocking Tk
                error_text = str(error)
            finished = time.monotonic()
            state["status_refresh_queue"].put(
                (model, error_text, started, finished))

        threading.Thread(target=read_status, name="dashboard-status-read",
                         daemon=True).start()

    def apply_status_refresh() -> None:
        try:
            model, error_text, started, finished = state["status_refresh_queue"].get_nowait()
        except queue.Empty:
            return
        state["status_refresh_in_flight"] = False
        elapsed_ms = round(max(0.0, finished - started) * 1000.0, 1)
        if error_text:
            state["status_refresh_error"] = error_text
            log_dashboard_event(
                "dashboard.status.failed", {"duration_ms": elapsed_ms,
                                             "error": error_text[:240]},
                cooldown_key="last_status_error_log")
            return
        state["status_refresh_error"] = ""
        state["latest_model"] = model
        state["dashboard_refresh_monotonic"] = finished
        state["dashboard_refresh_count"] += 1
        state["dashboard_refresh_utc"] = _utc_now_iso()
        if elapsed_ms >= 100.0:
            log_dashboard_event(
                "dashboard.status.slow",
                {"duration_ms": elapsed_ms,
                 "refresh_number": state["dashboard_refresh_count"]},
                cooldown_key="last_status_slow_log", cooldown_seconds=10.0)

    def persist_window_geometry() -> None:
        state["geometry_save_after"] = None
        if state["maximized"]:
            return
        bounds = _window_bounds(root.winfo_id())
        if bounds:
            x, y, width, height = bounds
            geometry = _tk_geometry(width, height, x, y)
            saved_bounds = {"x": x, "y": y,
                            "width": width, "height": height}
        else:
            geometry = root.geometry()
            saved_bounds = state.get("window_bounds")
            if not (re.fullmatch(r"\d+x\d+\+-?\d+\+-?\d+", geometry)
                    or re.fullmatch(r"\d+x\d+[+-]\d+[+-]\d+", geometry)):
                return
        state["window_geometry"] = geometry
        state["window_bounds"] = saved_bounds
        _save_ui_state({"collapsed": state["collapsed"],
                        "geometry": geometry,
                        "window_bounds": saved_bounds})

    def queue_geometry_save(event) -> None:
        if event.widget is not root or state["maximized"]:
            return
        pending = state.get("geometry_save_after")
        if pending:
            try:
                root.after_cancel(pending)
            except tk.TclError:
                pass
        state["geometry_save_after"] = root.after(400, persist_window_geometry)

    root.bind("<Configure>", queue_geometry_save, add="+")

    def control_worker() -> None:
        """Send dashboard control requests without blocking Tk's frame loop."""
        while True:
            item = state["control_queue"].get()
            if item is None:
                return
            action, fields, press_epoch, press_monotonic = item
            response = None
            error = ""
            try:
                with state["provider_control_lock"]:
                    response = control(action, **fields)
            except Exception as exc:  # noqa: BLE001 - surfaced in the status line
                error = str(exc)
            state["control_results"].put(
                (action, response, error, press_epoch, press_monotonic,
                 time.monotonic()))

    threading.Thread(target=control_worker, name="dashboard-control",
                     daemon=True).start()

    def frame(parent, *, padx=0, pady=0, **kwargs):
        return tk.Frame(parent, bg=BLACK, highlightthickness=1,
                        highlightbackground=RED, highlightcolor=RED,
                        padx=padx, pady=pady, **kwargs)

    def label(parent, text="", *, size=11, bold=False, color=RED, **kwargs):
        weight = "bold" if bold else "normal"
        return tk.Label(parent, text=text, bg=BLACK, fg=color,
                        font=(family, size, weight), anchor="w",
                        justify="left", **kwargs)

    def button(parent, text, callback, *, width=None):
        options = {
            "text": text,
            "command": callback,
            "bg": BLACK,
            "fg": RED,
            "activebackground": "#330000",
            "activeforeground": WHITE,
            "disabledforeground": "#660000",
            "relief": "raised",
            "bd": 2,
            "highlightthickness": 1,
            "highlightbackground": RED,
            "highlightcolor": RED,
            "font": (family, 10, "bold"),
            "padx": 5,
            "pady": 2,
            "takefocus": True,
        }
        if width is not None:
            options["width"] = width
        return tk.Button(parent, **options)

    def collapsible_section(parent, row, title_text, key, default_open=True):
        """Wrap a dashboard section in a click-to-collapse header."""
        collapsed = state.setdefault("collapsed", {})
        is_open = [not bool(collapsed.get(key, not default_open))]
        container = tk.Frame(parent, bg=BLACK)
        container.grid(row=row, column=0, sticky="ew", pady=(3, 0))
        container.grid_columnconfigure(0, weight=1)
        header = tk.Frame(container, bg=BLACK)
        header.grid(row=0, column=0, sticky="ew")
        header.grid_columnconfigure(0, weight=1)
        tk.Label(header, text=title_text, bg=BLACK, fg=RED,
                 font=(family, 10, "bold"), anchor="w").grid(
            row=0, column=0, sticky="w")
        toggle_holder = {}

        def apply_state():
            if is_open[0]:
                body.grid()
                toggle_holder["button"].configure(text="[-]")
            else:
                body.grid_remove()
                toggle_holder["button"].configure(text="[+]")
            try:
                body_canvas.configure(scrollregion=body_canvas.bbox("all"))
            except (tk.TclError, NameError):
                pass

        def on_toggle():
            is_open[0] = not is_open[0]
            collapsed[key] = not is_open[0]
            _save_ui_state({"collapsed": collapsed,
                            "geometry": state["window_geometry"],
                            "window_bounds": state["window_bounds"]})
            apply_state()

        body = tk.Frame(container, bg=BLACK)
        body.grid(row=1, column=0, sticky="ew", pady=(1, 0))
        toggle = tk.Button(header, text="[-]", command=on_toggle, bg=BLACK, fg=RED,
                           activebackground="#330000", activeforeground=WHITE,
                           relief="flat", bd=0, highlightthickness=0,
                           font=(family, 11, "bold"), padx=2, pady=0, takefocus=True)
        toggle.grid(row=0, column=1, sticky="e")
        toggle_holder["button"] = toggle
        apply_state()
        return body

    # Make a taskbar entry for the borderless custom-caption window.
    if os.name == "nt":
        def mark_as_app_window():
            try:
                hwnd = root.winfo_id()
                user32 = ctypes.WinDLL("user32", use_last_error=True)
                from ctypes import wintypes

                user32.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
                user32.GetAncestor.restype = wintypes.HWND
                get_long = getattr(user32, "GetWindowLongPtrW", None) or user32.GetWindowLongW
                set_long = getattr(user32, "SetWindowLongPtrW", None) or user32.SetWindowLongW
                get_long.argtypes = [wintypes.HWND, ctypes.c_int]
                get_long.restype = ctypes.c_ssize_t
                set_long.argtypes = [wintypes.HWND, ctypes.c_int, ctypes.c_ssize_t]
                set_long.restype = ctypes.c_ssize_t
                user32.SetWindowPos.argtypes = [wintypes.HWND, wintypes.HWND,
                                                ctypes.c_int, ctypes.c_int,
                                                ctypes.c_int, ctypes.c_int, wintypes.UINT]
                user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
                user32.ShowWindow.restype = wintypes.BOOL
                # winfo_id may identify Tk's client child. Shell visibility
                # belongs to the real top-level window returned by GA_ROOT.
                hwnd = user32.GetAncestor(hwnd, 2) or hwnd
                ex_style = get_long(hwnd, -20)
                app_style = (ex_style | 0x00040000) & ~0x00000080
                set_long(hwnd, -20, app_style)
                user32.SetWindowPos(hwnd, None, 0, 0, 0, 0,
                                    0x0001 | 0x0002 | 0x0004 | 0x0020)
                # Force Explorer and the task switcher to refresh the frame's
                # app-window registration after Tk's override-redirect change.
                user32.ShowWindow(hwnd, 5)
                if get_long(hwnd, -20) != app_style:
                    state["window_registration_error"] = (
                        "Windows did not retain the taskbar/app-switcher window style")
            except (AttributeError, OSError):
                state["window_registration_error"] = "Windows app-window registration failed"

        root.after(180, mark_as_app_window)

    shell = tk.Frame(root, bg=BLACK, highlightthickness=1,
                     highlightbackground=RED, highlightcolor=RED)
    shell.pack(fill="both", expand=True)
    shell.grid_rowconfigure(1, weight=1)
    shell.grid_columnconfigure(0, weight=1)

    titlebar = tk.Frame(shell, bg=BLACK, height=27,
                        highlightthickness=0, bd=0)
    titlebar.grid(row=0, column=0, sticky="ew")
    titlebar.grid_columnconfigure(0, weight=1)
    app_title = label(titlebar, title, size=10, bold=True)
    app_title.grid(row=0, column=0, sticky="w", padx=(8, 4), pady=3)

    content = tk.Frame(shell, bg=BLACK, padx=7, pady=5)
    content.grid(row=1, column=0, sticky="nsew")
    content.grid_columnconfigure(0, weight=1)
    content.grid_rowconfigure(2, weight=1)

    caption_buttons = tk.Frame(titlebar, bg=BLACK)
    caption_buttons.grid(row=0, column=1, sticky="e", padx=(0, 4))

    def minimize():
        if os.name == "nt":
            try:
                user32 = ctypes.WinDLL("user32")
                user32.ShowWindow.argtypes = [ctypes.c_void_p, ctypes.c_int]
                user32.ShowWindow(root.winfo_id(), 6)
                state["minimized"] = True
                return
            except (AttributeError, OSError):
                pass
        root.iconify()

    def toggle_maximize():
        if state["maximized"]:
            geometry = state["restore_geometry"] or f"{MIN_WIDTH}x{MIN_HEIGHT}"
            root.geometry(geometry)
            state["maximized"] = False
            maximize_button.configure(text="□")
            return
        bounds = _window_bounds(root.winfo_id())
        state["restore_geometry"] = (
            _tk_geometry(bounds[2], bounds[3], bounds[0], bounds[1])
            if bounds else root.geometry())
        if os.name == "nt":
            work_area = _monitor_work_area(root.winfo_id())
            if work_area is None:
                # A global screen-size fallback would jump windows on secondary
                # monitors to the primary display. Leave position unchanged if
                # Windows cannot identify this window's monitor.
                state["restore_geometry"] = None
                return
            left, top, width, height = work_area
            root.geometry(_tk_geometry(width, height, left, top))
        else:
            root.geometry(f"{root.winfo_screenwidth()}x{root.winfo_screenheight()}+0+0")
        state["maximized"] = True
        maximize_button.configure(text="❐")

    def close_view():
        state["closing"] = True
        persist_window_geometry()
        pending = state.get("geometry_save_after")
        if pending:
            try:
                root.after_cancel(pending)
            except tk.TclError:
                pass
        for key in ("after_id", "refresh_after_id", "reflow_after_id",
                    "coordination_render_after_id", "checkout_render_after_id"):
            pending = state.get(key)
            if pending:
                try:
                    root.after_cancel(pending)
                except tk.TclError:
                    pass
                state[key] = None
        try:
            state["coordination_request_queue"].put_nowait(None)
        except queue.Full:
            pass
        state["control_queue"].put(None)
        root.destroy()

    button(caption_buttons, "_", minimize, width=2).pack(side="left", padx=1)
    maximize_button = button(caption_buttons, "□", toggle_maximize, width=2)
    maximize_button.pack(side="left", padx=1)
    button(caption_buttons, "X", close_view, width=2).pack(side="left", padx=1)

    def start_move(event):
        if state["maximized"]:
            return
        state["drag"] = (event.x_root - root.winfo_x(),
                         event.y_root - root.winfo_y())

    def move_window(event):
        drag = state["drag"]
        if drag:
            root.geometry(_tk_geometry(
                root.winfo_width(), root.winfo_height(),
                event.x_root - drag[0], event.y_root - drag[1]))

    for widget in (titlebar, app_title):
        widget.bind("<ButtonPress-1>", start_move)
        widget.bind("<B1-Motion>", move_window)
        widget.bind("<ButtonRelease-1>", lambda _event: state.__setitem__("drag", None))

    # Custom resize edges retain the normal window's resize behavior after the
    # native title bar has been replaced with the black caption above.
    resize_edges = (
        ("n", "size_ns", {"x": 7, "y": 0, "relwidth": 1, "width": -14, "height": 6}),
        ("s", "size_ns", {"x": 7, "rely": 1, "relwidth": 1, "width": -14, "height": 6}),
        ("e", "size_we", {"relx": 1, "y": 7, "rely": 0, "width": 6, "relheight": 1, "height": -14}),
        ("w", "size_we", {"x": 0, "y": 7, "relheight": 1, "height": -14, "width": 6}),
        ("nw", "size_nw_se", {"x": 0, "y": 0, "width": 10, "height": 10}),
        ("ne", "size_ne_sw", {"relx": 1, "x": -10, "y": 0, "width": 10, "height": 10}),
        ("sw", "size_ne_sw", {"x": 0, "rely": 1, "y": -10, "width": 10, "height": 10}),
        ("se", "size_nw_se", {"relx": 1, "rely": 1, "x": -10, "y": -10, "width": 10, "height": 10}),
    )

    def begin_resize(direction, event):
        state["resize"] = {
            "direction": direction,
            "pointer_x": event.x_root,
            "pointer_y": event.y_root,
            "x": root.winfo_x(),
            "y": root.winfo_y(),
            "width": root.winfo_width(),
            "height": root.winfo_height(),
        }
        return "break"

    def resize_window(event):
        drag = state["resize"]
        if not drag:
            return
        dx = event.x_root - drag["pointer_x"]
        dy = event.y_root - drag["pointer_y"]
        direction = drag["direction"]
        x, y = drag["x"], drag["y"]
        width, height = drag["width"], drag["height"]
        if "e" in direction:
            width = max(MIN_WIDTH, width + dx)
        if "s" in direction:
            height = max(MIN_HEIGHT, height + dy)
        if "w" in direction:
            new_width = max(MIN_WIDTH, width - dx)
            x += width - new_width
            width = new_width
        if "n" in direction:
            new_height = max(MIN_HEIGHT, height - dy)
            y += height - new_height
            height = new_height
        root.geometry(f"{width}x{height}+{x}+{y}")
        return "break"

    def finish_resize(_event):
        state["resize"] = None

    for direction, cursor, placement in resize_edges:
        edge = tk.Frame(root, bg=RED, cursor=cursor, bd=0)
        edge.place(**placement)
        edge.bind("<ButtonPress-1>", lambda event, d=direction: begin_resize(d, event))
        edge.bind("<B1-Motion>", resize_window)
        edge.bind("<ButtonRelease-1>", finish_resize)

    # The red labels remain fixed; only emphasized live values cycle hue.
    heading = label(content, "MiMITA DEV LOOP", size=10, bold=True)
    heading.grid(row=0, column=0, sticky="w", pady=(0, 4))
    subtitle = label(content, "Connecting to the build/runtime service…", size=11)
    subtitle.grid(row=1, column=0, sticky="ew", pady=(0, 4))

    body_shell = tk.Frame(content, bg=BLACK)
    body_shell.grid(row=2, column=0, sticky="nsew")
    body_shell.grid_rowconfigure(0, weight=1)
    body_shell.grid_columnconfigure(0, weight=1)
    body_canvas = tk.Canvas(body_shell, background=BLACK, highlightthickness=0, bd=0)
    body_canvas.grid(row=0, column=0, sticky="nsew")
    body_scroll = tk.Scrollbar(
        body_shell, orient="vertical", command=body_canvas.yview,
        bg=BLACK, troughcolor=BLACK, activebackground=RED,
        highlightbackground=RED, bd=1)
    body_scroll.grid(row=0, column=1, sticky="ns")
    body_canvas.configure(yscrollcommand=body_scroll.set)
    body = tk.Frame(body_canvas, bg=BLACK)
    body.grid_columnconfigure(0, weight=1)
    body_window = body_canvas.create_window((0, 0), window=body, anchor="nw")
    body.bind("<Configure>", lambda _event: body_canvas.configure(
        scrollregion=body_canvas.bbox("all")))
    body_canvas.bind("<Configure>", lambda event: body_canvas.itemconfigure(
        body_window, width=event.width))
    wheel_remainder = {"pixels": 0.0, "units": 0.0}

    def scroll_body(event):
        widget = root.winfo_containing(event.x_root, event.y_root)
        if widget is None:
            return None
        target = widget
        while target is not None and target.winfo_class() != "Text":
            target = getattr(target, "master", None)
        delta = float(event.delta or 0)
        if target is not None:
            wheel_remainder["units"] += -delta / 120.0
            amount = int(wheel_remainder["units"])
            if amount:
                wheel_remainder["units"] -= amount
                target.yview_scroll(amount, "units")
            return "break"
        target = next((canvas for canvas in state["scroll_canvases"]
                       if canvas.winfo_rootx() <= event.x_root
                       < canvas.winfo_rootx() + canvas.winfo_width()
                       and canvas.winfo_rooty() <= event.y_root
                       < canvas.winfo_rooty() + canvas.winfo_height()), None)
        if target is None:
            return None
        wheel_remainder["pixels"] += -delta * 0.25
        pixels = int(wheel_remainder["pixels"])
        if pixels:
            wheel_remainder["pixels"] -= pixels
            try:
                _left, top, _right, bottom = target.bbox("all")
                scroll_height = max(1, bottom - top - target.winfo_height())
                first = target.yview()[0]
                target.yview_moveto(max(0.0, min(1.0,
                                                 first + pixels / scroll_height)))
            except (TypeError, tk.TclError):
                pass
        return "break"

    body_canvas.bind_all("<MouseWheel>", scroll_body)

    def set_text_content(widget, value: str, cache_key: str) -> None:
        """Update a text view only when its contents change, retaining its viewport."""
        if state.get(cache_key) == value:
            return
        first, last = widget.yview()
        was_at_bottom = last >= 0.999
        state[cache_key] = value
        widget.configure(state="normal")
        widget.delete("1.0", "end")
        if value:
            widget.insert("1.0", value)
        widget.configure(state="disabled")

        def restore():
            try:
                widget.yview_moveto(1.0 if was_at_bottom else first)
            except tk.TclError:
                pass
        root.after_idle(restore)

    cards_host = collapsible_section(body, 0, "BUILD", "build")
    cards = tk.Frame(cards_host, bg=BLACK)
    cards.grid(row=0, column=0, sticky="ew")
    cards.grid_columnconfigure(0, weight=1)
    cards.grid_columnconfigure(1, weight=1)

    def make_card(title_text: str):
        card = frame(cards, padx=5, pady=3)
        card.grid_columnconfigure(0, weight=1)
        label(card, title_text.upper(), size=10, bold=True).grid(
            row=0, column=0, sticky="w")
        value = label(card, "—", size=12, bold=True, wraplength=500)
        value.grid(row=1, column=0, sticky="ew", pady=(2, 0))
        detail = label(card, "", size=10, wraplength=500)
        detail.grid(row=2, column=0, sticky="ew")
        detail.full_text = ""
        detail.short_text = ""
        return card, value, detail

    running_card, running_value, running_detail = make_card("Running build")
    latest_card, latest_value, latest_detail = make_card("Latest published")
    activity_card, phase_value, phase_detail = make_card("Build activity")
    game_build_value = label(activity_card, "Game DLL: —", size=10, bold=True,
                             wraplength=500)
    game_build_value.grid(row=3, column=0, sticky="ew", pady=(4, 0))
    game_build_detail = label(activity_card, "", size=9, wraplength=500)
    game_build_detail.grid(row=4, column=0, sticky="ew")

    queue_host = collapsible_section(body, 1, "WHOLE-GAME DLL BUILD QUEUE", "queue")
    game_queue_card = frame(queue_host, padx=7, pady=5)
    game_queue_card.grid(row=0, column=0, sticky="ew")
    game_queue_card.grid_columnconfigure(0, weight=1)
    game_queue_title = label(game_queue_card,
                             "Queue detail", size=11, bold=True)
    game_queue_title.grid(row=0, column=0, sticky="w")
    game_queue_timer = label(game_queue_card, "DLL build timer…",
                             size=20, bold=True)
    game_queue_timer.grid(row=1, column=0, sticky="ew", pady=(2, 0))
    game_queue_summary = label(game_queue_card, "Checking build queue…",
                               size=11, bold=True)
    game_queue_summary.grid(row=2, column=0, sticky="ew", pady=(1, 0))
    game_queue_time = label(game_queue_card, "", size=9)
    game_queue_time.grid(row=3, column=0, sticky="ew")
    game_queue_bar = tk.Canvas(game_queue_card, height=18, background="#180000",
                               highlightthickness=1, highlightbackground=RED, bd=0)
    game_queue_bar.grid(row=4, column=0, sticky="ew", pady=(4, 3))
    game_queue_rows = tk.Text(
        game_queue_card, height=12, wrap="word", bg=BLACK, fg=WHITE,
        insertbackground=WHITE, relief="flat", bd=0, highlightthickness=0,
        font=(family, 10), state="disabled", padx=3, pady=2)
    game_queue_rows.grid(row=5, column=0, sticky="ew")
    game_queue_scroll = tk.Scrollbar(
        game_queue_card, orient="vertical", command=game_queue_rows.yview,
        bg=BLACK, troughcolor=BLACK, activebackground=RED,
        highlightbackground=RED, bd=1)
    game_queue_scroll.grid(row=5, column=1, sticky="ns")
    game_queue_rows.configure(yscrollcommand=game_queue_scroll.set)
    runtime_card, runtime_value, runtime_detail = make_card("Runtime and mode/map")
    scenario_card, scenario_value, scenario_detail = make_card("Scenario evidence")

    checkout_host = collapsible_section(body, 2, "CHECKOUTS AND AGENT LANES", "checkouts")
    checkout_card = frame(checkout_host, padx=5, pady=3)
    checkout_card.grid(row=0, column=0, sticky="ew")
    checkout_card.grid_columnconfigure(0, weight=1)
    label(checkout_card, "Checkout detail", size=10, bold=True).grid(
        row=0, column=0, sticky="w")
    checkout_canvas = tk.Canvas(checkout_card, height=126, background=BLACK,
                                highlightthickness=0, bd=0)
    checkout_canvas.grid(row=1, column=0, sticky="ew")
    checkout_scroll = tk.Scrollbar(
        checkout_card, orient="vertical", command=checkout_canvas.yview,
        bg=BLACK, troughcolor=BLACK, activebackground=RED,
        highlightbackground=RED, bd=1)
    checkout_scroll.grid(row=1, column=1, sticky="ns")
    checkout_canvas.configure(yscrollcommand=checkout_scroll.set)
    checkout_rows_frame = tk.Frame(checkout_canvas, bg=BLACK)
    checkout_rows_window = checkout_canvas.create_window(
        (0, 0), window=checkout_rows_frame, anchor="nw")
    checkout_rows_frame.bind(
        "<Configure>", lambda _event: checkout_canvas.configure(
            scrollregion=checkout_canvas.bbox("all")))
    checkout_canvas.bind(
        "<Configure>", lambda event: checkout_canvas.itemconfigure(
            checkout_rows_window, width=event.width))

    process_host = collapsible_section(body, 3, "MIMITA PROCESSES", "processes")
    process_card = frame(process_host, padx=5, pady=3)
    process_card.grid(row=0, column=0, sticky="ew")
    process_card.grid_columnconfigure(0, weight=1)
    process_card.grid_columnconfigure(1, weight=0)
    label(process_card, "Process detail", size=10, bold=True).grid(
        row=0, column=0, sticky="w")
    process_summary = label(process_card, "Checking for running MiMITA processes…",
                            size=10, wraplength=700)
    process_summary.grid(row=1, column=0, columnspan=2, sticky="ew", pady=(1, 2))
    process_canvas = tk.Canvas(process_card, height=190, background=BLACK,
                               highlightthickness=0, bd=0)
    process_canvas.grid(row=2, column=0, sticky="ew")
    process_scroll = tk.Scrollbar(
        process_card, orient="vertical", command=process_canvas.yview,
        bg=BLACK, troughcolor=BLACK, activebackground=RED,
        highlightbackground=RED, bd=1)
    process_scroll.grid(row=2, column=1, sticky="ns")
    process_canvas.configure(yscrollcommand=process_scroll.set)
    process_rows_frame = tk.Frame(process_canvas, bg=BLACK)
    process_rows_window = process_canvas.create_window(
        (0, 0), window=process_rows_frame, anchor="nw")
    process_rows_frame.bind(
        "<Configure>",
        lambda _event: process_canvas.configure(
            scrollregion=process_canvas.bbox("all")))
    process_canvas.bind(
        "<Configure>",
        lambda event: process_canvas.itemconfigure(
            process_rows_window, width=event.width))

    runtimes_host = collapsible_section(body, 4, "RUNTIMES", "runtimes")
    runtimes_card = frame(runtimes_host, padx=5, pady=3)
    runtimes_card.grid(row=0, column=0, sticky="ew")
    runtimes_card.grid_columnconfigure(0, weight=1)
    runtimes_summary = label(runtimes_card, "No independent runtimes.", size=10)
    runtimes_summary.grid(row=0, column=0, sticky="w")
    runtimes_rows = tk.Frame(runtimes_card, bg=BLACK)
    runtimes_rows.grid(row=1, column=0, sticky="ew")

    def start_runtime_dialog():
        runtime_id = simpledialog.askstring(
            "Start runtime", "Runtime id (for example juggernaut-a):", parent=root)
        if not runtime_id:
            return
        mode = simpledialog.askstring(
            "Start runtime", "Launch mode:", initialvalue="9", parent=root)
        if not mode:
            return
        send("start-runtime", runtime_id=runtime_id.strip(),
             runtime_launch_mode=mode.strip())

    button(runtimes_card, "Start runtime", start_runtime_dialog, width=12).grid(
        row=0, column=1, sticky="e")

    def render_runtimes(model: dict) -> None:
        rows = list(model.get("runtimes") or [])
        runtimes_summary.configure(
            text=f"{len(rows)} runtime(s)" if rows else "No independent runtimes.")
        for child in runtimes_rows.winfo_children():
            child.destroy()
        for row in rows[:8]:
            runtime_id = str(row.get("runtime_id") or "")
            alive = bool(row.get("server_alive"))
            client_alive = bool(row.get("client_alive"))
            line = tk.Frame(runtimes_rows, bg=BLACK)
            line.grid(sticky="ew", pady=1)
            line.grid_columnconfigure(0, weight=1)
            label(line,
                  f"{runtime_id}  mode={row.get('launch_mode')} "
                  f"map={row.get('map')} "
                  f"server={'alive' if alive else 'dead'} "
                  f"client={'alive' if client_alive else 'dead'} "
                  f"port={row.get('port') or '-'}", size=9).grid(
                row=0, column=0, sticky="w")
            button(line, "Stop",
                   lambda rid=runtime_id: send("stop-runtime", runtime_id=rid),
                   width=6).grid(row=0, column=1, padx=(4, 0))
            button(line, "Reload",
                   lambda rid=runtime_id: send("reload-runtime", runtime_id=rid),
                   width=7).grid(row=0, column=2, padx=(4, 0))

    coordination_host = collapsible_section(
        body, 5, "COORDINATION", "coordination")
    coordination_card = frame(coordination_host, padx=5, pady=3)
    coordination_card.grid(row=0, column=0, sticky="ew")
    coordination_card.grid_columnconfigure(0, weight=1)
    coordination_summary = label(
        coordination_card, "Checking agent coordination…", size=10, bold=True)
    coordination_summary.grid(row=0, column=0, sticky="w")
    coordination_focus = label(coordination_card, "", size=9, color=WHITE)
    coordination_focus.grid(row=1, column=0, sticky="w", pady=(0, 3))

    agent_canvas = tk.Canvas(coordination_card, height=120, background=BLACK,
                             highlightthickness=0, bd=0)
    agent_canvas.grid(row=2, column=0, sticky="ew")
    agent_scroll = tk.Scrollbar(
        coordination_card, orient="vertical", command=agent_canvas.yview,
        bg=BLACK, troughcolor=BLACK, activebackground=RED,
        highlightbackground=RED, bd=1)
    agent_scroll.grid(row=2, column=1, sticky="ns")
    agent_canvas.configure(yscrollcommand=agent_scroll.set)
    agent_rows_frame = tk.Frame(agent_canvas, bg=BLACK)
    agent_rows_window = agent_canvas.create_window(
        (0, 0), window=agent_rows_frame, anchor="nw")
    agent_rows_frame.bind(
        "<Configure>", lambda _event: agent_canvas.configure(
            scrollregion=agent_canvas.bbox("all")))
    agent_canvas.bind(
        "<Configure>", lambda event: agent_canvas.itemconfigure(
            agent_rows_window, width=event.width))

    label(coordination_card, "RECENT MESSAGES", size=9, bold=True).grid(
        row=3, column=0, sticky="w", pady=(3, 1))
    coordination_messages = tk.Text(
        coordination_card, height=8, wrap="word", bg=BLACK, fg=WHITE,
        insertbackground=WHITE, relief="flat", bd=0, highlightthickness=0,
        font=(family, 9), state="disabled", padx=3, pady=2)
    coordination_messages.grid(row=4, column=0, sticky="ew")
    coordination_messages_scroll = tk.Scrollbar(
        coordination_card, orient="vertical", command=coordination_messages.yview,
        bg=BLACK, troughcolor=BLACK, activebackground=RED,
        highlightbackground=RED, bd=1)
    coordination_messages_scroll.grid(row=4, column=1, sticky="ns")
    coordination_messages.configure(
        yscrollcommand=coordination_messages_scroll.set)

    label(coordination_card, "OPEN ISSUES", size=9, bold=True).grid(
        row=5, column=0, sticky="w", pady=(3, 1))
    coordination_issues = tk.Text(
        coordination_card, height=5, wrap="word", bg=BLACK, fg=RED,
        insertbackground=RED, relief="flat", bd=0, highlightthickness=0,
        font=(family, 9), state="disabled", padx=3, pady=2)
    coordination_issues.grid(row=6, column=0, sticky="ew")
    coordination_issues_scroll = tk.Scrollbar(
        coordination_card, orient="vertical", command=coordination_issues.yview,
        bg=BLACK, troughcolor=BLACK, activebackground=RED,
        highlightbackground=RED, bd=1)
    coordination_issues_scroll.grid(row=6, column=1, sticky="ns")
    coordination_issues.configure(
        yscrollcommand=coordination_issues_scroll.set)
    state["scroll_canvases"] = [
        body_canvas, checkout_canvas, process_canvas, agent_canvas]

    composer = tk.Frame(coordination_card, bg=BLACK)
    composer.grid(row=7, column=0, sticky="ew", pady=(4, 0))
    composer.grid_columnconfigure(1, weight=1)
    label(composer, "TO", size=9, bold=True).grid(row=0, column=0, sticky="w")
    recipient_var = tk.StringVar(value="all")
    recipient_menu = tk.OptionMenu(composer, recipient_var, "all")
    recipient_menu.configure(
        bg=BLACK, fg=RED, activebackground="#330000", activeforeground=WHITE,
        highlightthickness=1, highlightbackground=RED, relief="flat",
        font=(family, 10), width=18)
    recipient_menu["menu"].configure(
        bg=BLACK, fg=RED, activebackground="#330000",
        activeforeground=WHITE, font=(family, 10))
    recipient_menu.grid(row=0, column=1, sticky="w", padx=(4, 8))
    subject_entry = tk.Entry(
        composer, bg=BLACK, fg=WHITE, insertbackground=WHITE,
        highlightthickness=1, highlightbackground=RED, relief="flat",
        font=(family, 10))
    subject_entry.grid(row=0, column=2, sticky="ew", padx=(0, 4))
    composer.grid_columnconfigure(2, weight=1)
    message_entry = tk.Text(
        composer, height=3, wrap="word", bg=BLACK, fg=WHITE,
        insertbackground=WHITE, highlightthickness=1, highlightbackground=RED,
        relief="flat", font=(family, 10), padx=3, pady=2)
    message_entry.grid(row=1, column=0, columnspan=3, sticky="ew",
                       pady=(4, 2))
    composer_footer = tk.Frame(composer, bg=BLACK)
    composer_footer.grid(row=2, column=0, columnspan=3, sticky="ew")
    composer_footer.grid_columnconfigure(0, weight=1)
    coordination_send_status = label(composer_footer, "", size=9, color=GREEN)
    coordination_send_status.grid(row=0, column=0, sticky="w")

    def coordination_store():
        if state["coordination_store"] is not None:
            return state["coordination_store"]
        try:
            from devscripts.agent_coord import CoordStore
            state["coordination_store"] = CoordStore(publish_docs=False)
        except Exception as error:  # noqa: BLE001 - dashboard must stay usable
            state["coordination_error"] = str(error)
        return state["coordination_store"]

    def start_coordination_reader() -> None:
        if state["coordination_reader_started"]:
            return
        state["coordination_reader_started"] = True

        def read_coordination() -> None:
            reader = None
            while True:
                request = state["coordination_request_queue"].get()
                if request is None:
                    break
                started = time.monotonic()
                snapshot = None
                error_text = ""
                try:
                    if reader is None:
                        from devscripts.agent_coord import CoordStore
                        reader = CoordStore(publish_docs=False)
                    snapshot = reader.coordination_snapshot(limit_messages=80)
                except Exception as error:  # noqa: BLE001 - dashboard stays responsive
                    error_text = str(error)
                finished = time.monotonic()
                state["coordination_result_queue"].put(
                    (snapshot, error_text, started, finished))
            if reader is not None:
                try:
                    reader.close()
                except Exception:
                    pass

        threading.Thread(target=read_coordination,
                         name="dashboard-coordination-read", daemon=True).start()

    def request_coordination_snapshot() -> None:
        if state["coordination_snapshot_in_flight"] or state["closing"]:
            return
        now = time.monotonic()
        if now - state["coordination_request_last_monotonic"] < 2.0:
            return
        start_coordination_reader()
        try:
            state["coordination_request_queue"].put_nowait(True)
        except queue.Full:
            return
        state["coordination_request_last_monotonic"] = now
        state["coordination_snapshot_in_flight"] = True

    def send_coordination_message():
        store = coordination_store()
        if store is None:
            coordination_send_status.configure(
                text="Coordination unavailable: " + state["coordination_error"],
                fg=RED)
            return
        recipient = recipient_var.get().strip() or "all"
        subject = subject_entry.get().strip()
        body = message_entry.get("1.0", "end").strip()
        if not body:
            coordination_send_status.configure(
                text="Message body is empty", fg=RED)
            return
        try:
            result = store.send_human(recipient, subject, body)
        except Exception as error:  # noqa: BLE001 - show the human the reason
            coordination_send_status.configure(text=str(error), fg=RED)
            return
        coordination_send_status.configure(
            text=f"Sent to {result['recipient']} · {result['message']}", fg=GREEN)
        subject_entry.delete(0, "end")
        message_entry.delete("1.0", "end")
        render_coordination()

    send_button = button(composer_footer, "Send", send_coordination_message,
                         width=8)
    send_button.grid(row=0, column=1, sticky="e")

    def finish_coordination_render(snapshot, agent_scroll_position):
        if state["closing"]:
            return
        message_lines = []
        for item in snapshot.get("messages") or []:
            created = str(item.get("created_utc") or "")
            stamp = created[11:19] if len(created) >= 19 else created
            sender = str(item.get("sender") or "?")
            recipient = str(item.get("recipient") or "?")
            kind = str(item.get("kind") or "message")
            receipts = item.get("read_receipts") or []
            unread = " *" if not receipts else ""
            read_by = ", ".join(
                f"{entry.get('agent')}@{str(entry.get('read_at') or '')[11:19]}"
                for entry in receipts)
            receipt_label = f" read by {read_by}" if read_by else ""
            header = (f"{stamp} {sender} → {recipient} [{kind}]"
                      f"{unread}{receipt_label}")
            subject = str(item.get("subject") or "")
            body = str(item.get("body") or "")
            message_lines.append(header)
            if subject:
                message_lines.append(f"  {subject}")
            if body:
                message_lines.append("  " + body)
            message_lines.append("")
        measure_ui_stage(
            "coordination_messages",
            lambda: set_text_content(coordination_messages, "\n".join(message_lines),
                                     "coordination_messages_text"))
        state["coordination_render_after_id"] = root.after(
            1, lambda: finish_coordination_issues(snapshot, agent_scroll_position))

    def finish_coordination_issues(snapshot, agent_scroll_position):
        if state["closing"]:
            return
        issue_lines = []
        for item in snapshot.get("issues") or []:
            issue_lines.extend((
                f"{item.get('id')} {item.get('state')} {item.get('kind')} "
                f"{item.get('priority')} owner={item.get('owner') or '-'}",
                f"  {item.get('title') or item.get('summary') or ''}"))
        measure_ui_stage(
            "coordination_issues",
            lambda: set_text_content(coordination_issues, "\n".join(issue_lines),
                                     "coordination_issues_text"))
        state["coordination_render_after_id"] = None
        state["coordination_render_in_progress"] = False
        root.after_idle(lambda: agent_canvas.yview_moveto(agent_scroll_position))
        request_coordination_snapshot()

    def render_coordination_agent_chunk(snapshot, agents, work_by_agent,
                                        index, rendered, agent_scroll_position):
        if state["closing"]:
            return
        started = time.perf_counter()
        stop = min(index + 1, len(agents))
        try:
            for item in agents[index:stop]:
                activity = item.get("activity") or {}
                status = str(item.get("status") or "unknown")
                color = (GREEN if status == "active" else
                         "#ffcc00" if status == "stale" else "#888888")
                agent_name = str(item.get("agent") or "?")
                task = _short_text(item.get("task") or activity.get("activity"), 120)
                issue = _short_text(activity.get("issue_id") or "", 80)
                phase = _short_text(activity.get("phase") or "", 80)
                paths = activity.get("paths") or ""
                try:
                    parsed_paths = json.loads(paths) if isinstance(paths, str) else paths
                    paths_text = ", ".join(str(path) for path in (parsed_paths or [])[:3])
                except (TypeError, json.JSONDecodeError):
                    paths_text = str(paths)
                work = work_by_agent.get(agent_name) or {}
                work_ms = int(work.get("elapsed_ms") or 0)
                detail = " · ".join(part for part in (
                    f"issue {issue}" if issue else "",
                    f"phase {phase}" if phase else "",
                    f"work {work.get('phase')} {work_ms}ms" if work else "",
                    f"paths {paths_text}" if paths_text else "",
                    f"build {activity.get('build_id')}" if activity.get('build_id') else "",
                    f"run {activity.get('run_id')}" if activity.get('run_id') else "",
                    f"blocker {activity.get('blocker')}" if activity.get('blocker') else "",
                    f"wait {activity.get('wait_reason')}" if activity.get('wait_reason') else "",
                    f"next {activity.get('next_step')}" if activity.get('next_step') else "",
                ) if part)
                rendered.add(agent_name)
                widgets = state["coordination_agent_rows"].get(agent_name)
                if widgets is None:
                    row = tk.Frame(agent_rows_frame, bg=BLACK)
                    row.grid(sticky="ew", pady=1)
                    row.grid_columnconfigure(0, weight=1)
                    title_widget = label(row, size=9, bold=True)
                    title_widget.grid(row=0, column=0, sticky="w")
                    task_widget = label(row, size=9, color=WHITE)
                    task_widget.grid(row=1, column=0, sticky="w")
                    detail_widget = label(row, size=8, wraplength=760)
                    detail_widget.grid(row=2, column=0, sticky="w")
                    widgets = {"row": row, "title": title_widget,
                               "task": task_widget, "detail": detail_widget}
                    state["coordination_agent_rows"][agent_name] = widgets
                title_text = (f"{agent_name}  {status.upper()}  "
                              f"{_age_text(item.get('last_seen_utc'))}")
                if widgets["title"].cget("text") != title_text:
                    widgets["title"].configure(text=title_text, fg=color)
                if widgets["task"].cget("text") != task:
                    widgets["task"].configure(text=task)
                if widgets["detail"].cget("text") != detail:
                    widgets["detail"].configure(text=detail, fg=color)
        finally:
            elapsed_ms = round((time.perf_counter() - started) * 1000.0, 1)
            if elapsed_ms >= 50.0:
                log_dashboard_event(
                    "dashboard.stage.slow",
                    {"stage": "coordination_agent_chunk", "duration_ms": elapsed_ms,
                     "agent_count": stop - index,
                     "refresh_number": state["dashboard_refresh_count"]},
                    cooldown_key="last_stage_log_coordination_agent_chunk",
                    cooldown_seconds=2.0)
        if stop < len(agents):
            state["coordination_render_after_id"] = root.after(
                1, lambda: render_coordination_agent_chunk(
                    snapshot, agents, work_by_agent, stop, rendered,
                    agent_scroll_position))
            return
        for agent_name in list(state["coordination_agent_rows"]):
            if agent_name not in rendered:
                state["coordination_agent_rows"].pop(agent_name)["row"].destroy()
        state["coordination_render_after_id"] = root.after(
            1, lambda: finish_coordination_render(snapshot, agent_scroll_position))

    def render_coordination() -> None:
        if state["coordination_render_in_progress"]:
            return
        try:
            snapshot, error_text, started, finished = (
                state["coordination_result_queue"].get_nowait())
        except queue.Empty:
            snapshot = None
            error_text = ""
            started = finished = 0.0
        if finished:
            state["coordination_snapshot_in_flight"] = False
            elapsed_ms = round(max(0.0, finished - started) * 1000.0, 1)
            if elapsed_ms >= 100.0:
                log_dashboard_event(
                    "dashboard.coordination.slow", {"duration_ms": elapsed_ms},
                    cooldown_key="last_coordination_log")
        if error_text:
            state["coordination_error"] = error_text
            coordination_summary.configure(
                text="Coordination unavailable: " + error_text[:160], fg=RED)
            request_coordination_snapshot()
            return
        if snapshot is not None:
            state["coordination_snapshot"] = snapshot
            state["coordination_error"] = ""
        else:
            request_coordination_snapshot()
            snapshot = state.get("coordination_snapshot") or {}
            if not snapshot:
                coordination_summary.configure(
                    text="Loading coordination data…", fg=WHITE)
                return
        snapshot_signature = dict(snapshot)
        snapshot_signature.pop("generated_utc", None)
        if snapshot_signature == state.get("coordination_snapshot_signature"):
            request_coordination_snapshot()
            return
        agent_scroll_position = agent_canvas.yview()[0]
        state["coordination_snapshot"] = snapshot
        state["coordination_snapshot_signature"] = snapshot_signature
        state["coordination_error"] = ""
        agents = list(snapshot.get("agents") or [])
        issues = list(snapshot.get("issues") or [])
        leases = list(snapshot.get("leases") or [])
        work_by_agent = {
            str(item.get("agent")): item
            for item in (snapshot.get("work") or [])
            if item.get("agent")}
        active = [item for item in agents if item.get("status") == "active"]
        coordination_summary.configure(
            text=(f"{len(active)} active · {len(issues)} open issue(s) · "
                  f"{len(leases)} lease(s) · {snapshot.get('unread', 0)} unread"),
            fg=GREEN if active else RED)
        focus = snapshot.get("focus") or {}
        if focus:
            coordination_focus.configure(
                text=f"FOCUS {focus.get('id')} · {focus.get('state')} · "
                     f"{focus.get('priority')} · {_short_text(focus.get('title'))}")
        else:
            coordination_focus.configure(text="FOCUS none · no open issue")
        names = ["all"] + [str(item.get("agent")) for item in agents
                           if item.get("agent")]
        if recipient_var.get() not in names:
            recipient_var.set("all")
        names_signature = tuple(names)
        if names_signature != state["coordination_recipient_names"]:
            menu = recipient_menu["menu"]
            menu.delete(0, "end")
            for name in names:
                menu.add_command(label=name,
                                 command=lambda value=name: recipient_var.set(value))
            state["coordination_recipient_names"] = names_signature
        state["coordination_render_in_progress"] = True
        render_coordination_agent_chunk(
            snapshot, agents, work_by_agent, 0, set(), agent_scroll_position)

    process_buttons = tk.Frame(content, bg=BLACK)
    process_buttons.grid(row=3, column=0, sticky="ew", pady=(3, 0))
    for column in range(4):
        process_buttons.grid_columnconfigure(column, weight=1)

    refresh_process_button = button(
        process_card, "Refresh", lambda: manual_process_refresh(), width=8)
    refresh_process_button.grid(row=0, column=1, sticky="e")

    details_host = collapsible_section(content, 4, "LIVE BUILD AND LOOP OUTPUT", "output")
    details = frame(details_host, padx=5, pady=3)
    details.grid(row=0, column=0, sticky="ew")
    details.grid_columnconfigure(0, weight=1)
    label(details, "FOLLOWING LATEST",
          size=10, bold=True).pack(anchor="w", pady=(0, 2))
    refresh_indicator = label(details, "", size=9, color=GREEN)
    refresh_indicator.pack(anchor="w", pady=(0, 1))
    source_summary = label(details, "", size=9, color=RED, wraplength=460)
    source_summary.pack(anchor="w", pady=(0, 2))
    output_frame = tk.Frame(details, bg=BLACK)
    output_frame.pack(fill="both", expand=True)
    gutter = tk.Canvas(output_frame, width=28, bg=BLACK, highlightthickness=0,
                       bd=0, takefocus=False)
    gutter.pack(side="left", fill="y")
    output = tk.Text(output_frame, height=5, wrap="word", state="disabled",
                     background=BLACK, foreground=RED, insertbackground=RED,
                     selectbackground="#660000", selectforeground=WHITE,
                     relief="flat", bd=0, highlightthickness=0,
                     font=(family, 11), padx=1, pady=1)
    output.pack(side="left", fill="both", expand=True)
    output_scroll = tk.Scrollbar(output_frame, orient="vertical", command=output.yview,
                                 bg=BLACK, troughcolor=BLACK, activebackground=RED,
                                 highlightbackground=RED, bd=1)
    output_scroll.pack(side="right", fill="y")

    def update_gutter(first=None, last=None):
        if first is not None:
            output_scroll.set(first, last)
        try:
            line_height = max(1, int(output.tk.call("font", "metrics", output.cget("font"), "-linespace")))
            line_height += 1
            height = max(1, output.winfo_height())
            y = 1
            visible_lines = []
            while y < height:
                index = output.index(f"@1,{y}")
                line_number = int(index.split(".", 1)[0])
                visible_lines.append((line_number, y))
                y += line_height
            signature = (height, tuple(visible_lines))
            if signature == state["gutter_signature"]:
                return
            state["gutter_signature"] = signature
            gutter.delete("all")
            for line_number, line_y in visible_lines:
                gutter.create_text(25, line_y, text=f"{line_number:02d}", anchor="ne",
                                   fill=RED, font=(family, 11))
        except (tk.TclError, ValueError):
            pass

    output.configure(yscrollcommand=lambda first, last: measure_ui_stage(
        "gutter", lambda: update_gutter(first, last)))
    output.bind("<Configure>", lambda _event: update_gutter())

    action_status = label(content, "Ready", size=10)
    action_status.grid(row=5, column=0, sticky="ew", pady=(3, 1))
    if state.get("window_registration_error"):
        action_status.configure(text=state["window_registration_error"])
    elif state.get("app_id_error"):
        action_status.configure(text="Windows app identity unavailable; taskbar window still enabled")
    controls = tk.Frame(content, bg=BLACK)
    controls.grid(row=6, column=0, sticky="ew")
    controls.grid_columnconfigure(0, weight=3)
    controls.grid_columnconfigure(1, weight=1)
    controls.grid_columnconfigure(2, weight=1)

    def send(action: str, **fields) -> None:
        if action == "build-or-launch" and state["launch_timing_status"] == "pending":
            action_status.configure(
                text="Launch already in progress · duplicate request ignored")
            return
        state["pending_checkout_id"] = str(fields.get("checkout_id") or "main")
        launch_start = time.monotonic() if action == "build-or-launch" else None
        press_epoch = time.time()
        if launch_start is not None:
            state["launch_started_monotonic"] = launch_start
            state["launch_started_epoch"] = press_epoch
            state["launch_events_path_before"] = state["current_events_path"]
            state["launch_request_id"] = ""
            state["launch_client_started_elapsed"] = None
            state["launch_client_reused"] = False
            state["launch_readiness_elapsed"] = None
            state["launch_ack_elapsed"] = None
            state["launch_timing_status"] = "pending"
            state["launch_timing_message"] = "Waiting for the dev loop"
            fields["dashboard_press_epoch"] = press_epoch
            action_status.configure(text="Launch request queued · waiting for acknowledgement")
        else:
            action_status.configure(text=f"{action}: queued")
        state["control_queue"].put(
            (action, dict(fields), press_epoch, launch_start))

    def render_checkout_row(checkout, index):
        checkout_id = str(checkout.get("checkout_id") or "")
        if not checkout_id:
            return
        widgets = state["checkout_rows"].get(checkout_id)
        if widgets is None:
            row_frame = tk.Frame(checkout_rows_frame, bg=BLACK)
            row_frame.grid_columnconfigure(1, weight=1)
            name = label(row_frame, size=10, bold=True)
            name.grid(row=0, column=0, sticky="w", padx=(2, 10))
            detail = label(row_frame, size=10)
            detail.grid(row=0, column=1, sticky="ew")
            action_frame = tk.Frame(row_frame, bg=BLACK)
            action_frame.grid(row=0, column=2, sticky="e", padx=(6, 2))
            for action, caption in (("build", "Build"), ("launch", "Launch"),
                                    ("start-server", "Server"), ("start-client", "Client")):
                button(action_frame, caption,
                       lambda selected=checkout_id, verb=action:
                       send(verb, checkout_id=selected), width=8).pack(side="left", padx=1)
            widgets = {"frame": row_frame, "name": name, "detail": detail,
                       "row": None, "name_text": None, "detail_text": None,
                       "detail_color": None}
            state["checkout_rows"][checkout_id] = widgets
        status = checkout.get("state") or {}
        if not checkout.get("available"):
            availability = str(checkout.get("availability") or "unavailable").upper()
            headline, color = (f"{availability} · status not available", "#888888")
        else:
            try:
                updated = datetime.fromisoformat(
                    str(status.get("updated_at") or "").replace("Z", "+00:00"))
                age = max(0.0, (datetime.now(timezone.utc) - updated).total_seconds())
            except (TypeError, ValueError):
                age = None
            if age is None or age > 6.0:
                headline, color = "STALE · status has not refreshed", "#ffcc00"
            else:
                build = str(status.get("build_status") or "build unknown").upper()
                phase = str(status.get("active_build_phase") or "").replace("_", " ")
                elapsed = status.get("active_build_elapsed_seconds")
                elapsed_text = f" {int(float(elapsed))}s" if elapsed is not None else ""
                instances = status.get("instances") or []
                servers = sum(1 for item in instances if item.get("role") == "server"
                              and item.get("state") not in ("offline", "stopped", "failed"))
                clients = sum(1 for item in instances if item.get("role") == "client"
                              and item.get("state") not in ("offline", "stopped", "failed"))
                headline = (f"{build}{(' · ' + phase) if phase else ''}{elapsed_text}"
                            f" · {servers} server(s) / {clients} client(s)")
                color = RED if build in ("BUILDING", "FAILED", "ERROR") else GREEN
        name_text = str(checkout.get("name") or checkout_id)
        if widgets["name_text"] != name_text:
            widgets["name"].configure(text=name_text)
            widgets["name_text"] = name_text
        if (widgets["detail_text"], widgets["detail_color"]) != (headline, color):
            widgets["detail"].configure(text=headline, fg=color)
            widgets["detail_text"], widgets["detail_color"] = headline, color
        if widgets["row"] != index:
            widgets["frame"].grid(row=index, column=0, sticky="ew", pady=1)
            widgets["row"] = index

    def render_checkout_chunk(checkouts, index, generation):
        if state["closing"] or generation != state["checkout_render_generation"]:
            return
        started = time.perf_counter()
        if index < len(checkouts):
            render_checkout_row(checkouts[index], index)
        elapsed_ms = round((time.perf_counter() - started) * 1000.0, 1)
        if elapsed_ms >= 50.0:
            log_dashboard_event(
                "dashboard.stage.slow",
                {"stage": "checkout_row", "duration_ms": elapsed_ms,
                 "refresh_number": state["dashboard_refresh_count"]},
                cooldown_key="last_stage_log_checkout_row", cooldown_seconds=2.0)
        next_index = index + 1
        if next_index < len(checkouts):
            state["checkout_render_after_id"] = root.after(
                1, lambda: render_checkout_chunk(checkouts, next_index, generation))
            return
        state["checkout_render_after_id"] = None
        state["checkout_render_in_progress"] = False
        checkout_rows_frame.grid_columnconfigure(0, weight=1)

    def render_checkout_rows(model: dict) -> None:
        if state["checkout_render_in_progress"]:
            return
        checkouts = list(model.get("checkouts") or [])
        checkout_ids = {str(item.get("checkout_id") or "") for item in checkouts}
        for checkout_id in list(state["checkout_rows"]):
            if checkout_id not in checkout_ids:
                state["checkout_rows"].pop(checkout_id)["frame"].destroy()
        state["checkout_render_generation"] += 1
        generation = state["checkout_render_generation"]
        state["checkout_render_in_progress"] = True
        render_checkout_chunk(checkouts, 0, generation)

    def selected_process() -> dict | None:
        selected_key = state.get("selected_process_key")
        return next((row for row in state.get("rendered_processes", [])
                     if row.get("process_key") == selected_key), None)

    def selected_instance() -> str:
        process = selected_process()
        if not process or process.get("source") != "managed":
            return ""
        return str(process.get("instance_id") or "")

    def send_instance(action: str) -> None:
        iid = selected_instance()
        if not iid:
            action_status.configure(text="Select a running dev-loop instance first")
            return
        send(action, instance_id=iid)

    def stop_selected() -> None:
        targets = []
        for row in state.get("rendered_processes", []):
            row_state = state.get("process_rows", {}).get(row.get("process_key"))
            if not row_state or not row_state["variable"].get():
                continue
            targets.append({key: row.get(key) for key in (
                "source", "instance_id", "pid", "role", "executable",
                "command_line", "process_started_utc")})
        if not targets:
            action_status.configure(text="Check one or more running processes first")
            return
        send("stop-selected", targets=targets)

    def start_client() -> None:
        clients = {int(row["pid"]) for row in state.get("rendered_processes", [])
                   if row.get("role") == "client" and row.get("pid")}
        client_count = len(clients)
        send("start-client", name=f"Client {client_count + 1}")

    button(process_buttons, "Start server", lambda: send("start-server")).grid(
        row=0, column=0, sticky="ew", padx=(0, 3))
    button(process_buttons, "Start client", start_client).grid(
        row=0, column=1, sticky="ew", padx=(0, 3))
    button(process_buttons, "Stop selected", stop_selected).grid(
        row=0, column=2, sticky="ew", padx=(0, 3))
    button(process_buttons, "Restart selected",
           lambda: send_instance("restart-instance")).grid(
        row=0, column=3, sticky="ew")

    launch_button = button(controls, "Launch latest stable MiMITA.exe",
                           lambda: send("build-or-launch"))
    launch_button.grid(row=0, column=0, sticky="ew", padx=(0, 3))
    button(controls, "Build", lambda: send("build")).grid(
        row=0, column=1, sticky="ew", padx=2)

    # Optional control-path smoke hook: invoke the same Tk button callback used
    # by a person, which makes dashboard launch wiring verifiable in a GUI run.
    if os.environ.pop("MIMITA_DEV_LOOP_GUI_TEST_ACTION", "") == "launch_latest":
        root.after(250, launch_button.invoke)

    options_button = button(controls, "More", lambda: show_options())
    options_button.grid(row=0, column=2, sticky="ew", padx=(3, 0))
    options_menu = tk.Menu(root, tearoff=False, bg=BLACK, fg=RED,
                           activebackground="#330000", activeforeground=WHITE,
                           bd=1, relief="raised", font=(family, 10))
    options_menu.add_command(label="Toggle game visibility", command=lambda: send("toggle-visibility"))
    options_menu.add_command(label="Toggle auto-restart", command=lambda: send("toggle-auto-restart"))
    options_menu.add_separator()
    def show_mode_picker() -> None:
        modes = state["launch_modes"] or _configured_launch_modes()
        active_mode = str(state["active_mode"] or "")
        popup = tk.Toplevel(root)
        popup.title("Select launch mode")
        popup.configure(bg=BLACK)
        popup.transient(root)
        popup.resizable(True, True)
        popup.minsize(300, 280)
        popup.geometry("340x390")
        popup.grid_columnconfigure(0, weight=1)
        popup.grid_rowconfigure(1, weight=1)

        label(popup, "SELECT LAUNCH MODE", size=10, bold=True).grid(
            row=0, column=0, sticky="ew", padx=8, pady=(8, 4))
        list_frame = tk.Frame(popup, bg=BLACK)
        list_frame.grid(row=1, column=0, sticky="nsew", padx=8, pady=4)
        list_frame.grid_rowconfigure(0, weight=1)
        list_frame.grid_columnconfigure(0, weight=1)
        mode_list = tk.Listbox(
            list_frame, bg=BLACK, fg=RED, selectbackground="#330000",
            selectforeground=WHITE, activestyle="none", selectmode="browse",
            exportselection=False, relief="flat", bd=0, highlightthickness=1,
            highlightbackground=RED, highlightcolor=RED,
            font=(family, 10), height=14)
        scrollbar = tk.Scrollbar(
            list_frame, orient="vertical", command=mode_list.yview,
            bg=BLACK, troughcolor=BLACK, activebackground=RED,
            highlightbackground=RED, bd=1)
        mode_list.configure(yscrollcommand=scrollbar.set)
        mode_list.grid(row=0, column=0, sticky="nsew")
        scrollbar.grid(row=0, column=1, sticky="ns")

        selected_index = None
        for index, item in enumerate(modes):
            mode_id = str(item.get("id", ""))
            name = str(item.get("name", "unnamed"))
            marker = "✓" if mode_id == active_mode else " "
            mode_list.insert("end", f"{marker}  {mode_id}  ·  {name}")
            if mode_id == active_mode:
                selected_index = index
        if selected_index is not None:
            mode_list.selection_set(selected_index)
            mode_list.activate(selected_index)
            mode_list.see(selected_index)
        if not modes:
            mode_list.insert("end", "No launch modes available")
            mode_list.configure(state="disabled")

        actions = tk.Frame(popup, bg=BLACK)
        actions.grid(row=2, column=0, sticky="ew", padx=8, pady=(4, 8))
        actions.grid_columnconfigure(0, weight=1)
        actions.grid_columnconfigure(1, weight=1)

        def select_mode() -> None:
            selection = mode_list.curselection()
            if not selection or not modes:
                action_status.configure(text="Select a configured launch mode")
                return
            mode_id = str(modes[int(selection[0])].get("id", ""))
            send("set-launch-mode", launch_mode=mode_id)
            popup.destroy()

        button(actions, "Select mode", select_mode).grid(
            row=0, column=0, sticky="ew", padx=(0, 3))
        button(actions, "Cancel", popup.destroy).grid(
            row=0, column=1, sticky="ew", padx=(3, 0))
        mode_list.bind("<Double-Button-1>", lambda _event: select_mode())
        popup.bind("<Return>", lambda _event: select_mode())
        popup.bind("<Escape>", lambda _event: popup.destroy())
        popup.protocol("WM_DELETE_WINDOW", popup.destroy)
        popup.update_idletasks()
        x = root.winfo_rootx() + max(0, (root.winfo_width() - popup.winfo_width()) // 2)
        y = root.winfo_rooty() + max(0, (root.winfo_height() - popup.winfo_height()) // 2)
        popup.geometry(f"+{x}+{y}")
        popup.grab_set()
        mode_list.focus_set()

    options_menu.add_command(label="Launch mode…", command=show_mode_picker)
    options_menu.add_separator()

    def toggle_freeze():
        state["frozen"] = not state["frozen"]
        options_menu.entryconfigure(pause_menu_index, label="Resume dashboard view" if state["frozen"]
                                    else "Pause dashboard view")
        action_status.configure(text=("Dashboard paused; game and daemon continue"
                                      if state["frozen"] else "Dashboard view resumed"))
        if not state["frozen"]:
            refresh()

    pause_menu_index = options_menu.index("end") + 1
    options_menu.add_command(label="Pause dashboard view", command=toggle_freeze)
    options_menu.add_separator()
    options_menu.add_command(label="Stop daemon", command=lambda: send("stop"))

    def show_options():
        options_menu.tk_popup(options_button.winfo_rootx(),
                              options_button.winfo_rooty() + options_button.winfo_height())

    def copy_build_identity(event, artifact, digest):
        text = f"{artifact}  SHA-256 {digest}"
        root.clipboard_clear()
        root.clipboard_append(text)
        action_status.configure(text="Build identity copied")
        return "break"

    for value in (running_value, latest_value):
        value.configure(cursor="hand2")

    def reflow(_event=None):
        state["reflow_after_id"] = None
        if state["closing"]:
            return
        started = time.perf_counter()
        try:
            width, height = root.winfo_width(), root.winfo_height()
            size = (width, height)
            if size == state["last_reflow_size"]:
                return
            state["last_reflow_size"] = size
            wide = width >= 760
            expanded = width >= 620 or height >= 650
            layout_changed = (wide, expanded) != (
                state["wide"], not state["compact"])
            state["wide"], state["compact"] = wide, not expanded
            card_wrap = max(240, (width - 36) // (2 if wide else 1) - 16)
            if card_wrap != state["last_reflow_wrap"]:
                for _card, value, detail in (
                        (running_card, running_value, running_detail),
                        (latest_card, latest_value, latest_detail),
                        (activity_card, phase_value, phase_detail),
                        (runtime_card, runtime_value, runtime_detail),
                        (scenario_card, scenario_value, scenario_detail)):
                    value.configure(wraplength=card_wrap)
                    detail.configure(wraplength=card_wrap)
                state["last_reflow_wrap"] = card_wrap
            subtitle.configure(wraplength=max(240, width - 30))
            action_status.configure(wraplength=max(240, width - 30))
            if not layout_changed:
                return
            for card in (running_card, latest_card, activity_card,
                         runtime_card, scenario_card):
                card.grid_forget()
            if wide:
                running_card.grid(row=0, column=0, sticky="ew", padx=(0, 3), pady=(0, 3))
                latest_card.grid(row=0, column=1, sticky="ew", padx=(3, 0), pady=(0, 3))
                activity_card.grid(row=1, column=0, sticky="ew", padx=(0, 3), pady=(0, 3))
                runtime_card.grid(row=1, column=1, sticky="ew", padx=(3, 0), pady=(0, 3))
            else:
                running_card.grid(row=0, column=0, sticky="ew", pady=(0, 3))
                latest_card.grid(row=1, column=0, sticky="ew", pady=(0, 3))
                activity_card.grid(row=2, column=0, sticky="ew", pady=(0, 3))
                runtime_card.grid(row=3, column=0, sticky="ew", pady=(0, 3))
            if expanded:
                scenario_card.grid(row=2 if wide else 4, column=0,
                                   columnspan=2 if wide else 1, sticky="ew")
            for detail in (running_detail, latest_detail):
                detail.configure(text=detail.full_text if expanded else detail.short_text)
            if state["after_id"] is not None:
                try:
                    root.after_cancel(state["after_id"])
                except tk.TclError:
                    pass
                state["after_id"] = None
            if state["refresh_after_id"] is None:
                state["refresh_after_id"] = root.after_idle(refresh)
        except tk.TclError as error:
            log_dashboard_event("dashboard.reflow.failed", {"error": str(error)[:200]},
                                cooldown_key="last_reflow_error_log")
        finally:
            elapsed_ms = round((time.perf_counter() - started) * 1000.0, 1)
            if elapsed_ms >= 50.0:
                log_dashboard_event("dashboard.reflow.slow",
                                    {"duration_ms": elapsed_ms, "size": state["last_reflow_size"]},
                                    cooldown_key="last_reflow_slow_log")

    def schedule_reflow(event=None):
        if state["closing"] or (event is not None and event.widget is not root):
            return
        if (event is not None and
                (int(event.width), int(event.height)) == state["last_reflow_size"]):
            return
        pending = state["reflow_after_id"]
        if pending is not None:
            try:
                root.after_cancel(pending)
            except tk.TclError:
                pass
        state["reflow_after_id"] = root.after(50, reflow)

    def queue_process_scan_result(processes=None, error=""):
        state["process_scan_queue"].put((processes, error))

    def request_process_scan(force=False):
        now = time.monotonic()
        if state["process_scan_in_flight"]:
            if force:
                state["process_scan_force_pending"] = True
            return
        if not force and now - state["process_scan_last_started"] < 2.0:
            return
        state["process_scan_in_flight"] = True
        state["process_scan_last_started"] = now

        def scan():
            try:
                if process_provider is not None:
                    processes = process_provider()
                else:
                    processes = [dict(row) for row in
                                 (state.get("latest_model", {}).get("instances") or [])
                                 if row.get("pid") and row.get("state") not in
                                 ("offline", "stopped", "failed")]
                queue_process_scan_result(processes=processes or [])
            except Exception as error:
                queue_process_scan_result(error=str(error))

        threading.Thread(target=scan, name="mimita-process-scan", daemon=True).start()

    def apply_process_scan_result():
        try:
            processes, error = state["process_scan_queue"].get_nowait()
        except queue.Empty:
            return
        state["process_scan_in_flight"] = False
        if state.pop("process_scan_force_pending", False):
            state["process_scan_last_started"] = 0.0
        if error:
            state["process_scan_error"] = error
            return
        state["process_scan_error"] = ""
        state["processes"] = processes if isinstance(processes, list) else []
        state["process_scan_last_success"] = _utc_now_iso()
        state["process_scan_last_success_monotonic"] = time.monotonic()

    def manual_process_refresh():
        process_summary.configure(text="Scanning running MiMITA processes…")
        action_status.configure(text="Refreshing process list…")
        request_process_scan(force=True)

    def live_process_rows(model):
        managed_by_pid = {}
        for checkout in model.get("checkouts", []) or []:
            checkout_state = checkout.get("state") or {}
            try:
                update_time = datetime.fromisoformat(
                    str(checkout_state.get("updated_at") or "").replace("Z", "+00:00"))
                checkout_stale = (datetime.now(timezone.utc) - update_time).total_seconds() > 6
            except (TypeError, ValueError):
                checkout_stale = True
            for instance in (checkout.get("state") or {}).get("instances", []) or []:
                try:
                    pid = int(instance.get("pid", 0) or 0)
                except (TypeError, ValueError):
                    pid = 0
                if pid and instance.get("state") not in ("offline", "stopped", "failed"):
                    managed_by_pid[pid] = dict(
                        instance, checkout_id=checkout.get("checkout_id"),
                        checkout_available=checkout.get("available", False),
                        checkout_stale=checkout_stale)

        rows = []
        for process in state.get("processes", []):
            try:
                pid = int(process.get("pid", 0) or 0)
            except (TypeError, ValueError):
                pid = 0
            if pid <= 0:
                continue
            command_line = str(process.get("command_line") or "")
            command_flags = {
                token.strip("\"'").casefold()
                for token in command_line.split()
            }
            instance = managed_by_pid.get(pid)
            role = str(instance.get("role") or "unknown") if instance else (
                "server" if "--server" in command_flags else
                "client" if command_flags.intersection({"--connect", "--room"}) else
                "unverified")
            started = str(process.get("process_started_utc") or "")
            row = dict(instance or {})
            if instance and (not instance.get("checkout_available", False)
                             or instance.get("checkout_stale", False)):
                row["state"] = "stale · daemon status unavailable"
            row.update({
                "source": "managed" if instance else "external",
                "process_key": f"{pid}:{started}",
                "pid": pid,
                "role": role,
                "state": str((instance or {}).get("state") or "running"),
                "executable": str(process.get("executable") or
                                   (instance or {}).get("executable") or ""),
                "command_line": command_line,
                "process_started_utc": started,
            })
            try:
                relative = Path(row["executable"]).resolve().relative_to(
                    Path(__file__).resolve().parents[2])
                if len(relative.parts) > 2 and relative.parts[:2] == (".dev", "agent-lanes"):
                    row["checkout_id"] = relative.parts[2]
            except (OSError, ValueError):
                pass
            if not instance:
                row["instance_id"] = f"external-{pid}"
                row["label"] = f"external {role}"
            rows.append(row)
        return sorted(rows, key=lambda row: (
            row.get("role") != "server", row.get("source") != "managed", row.get("pid", 0)))

    def render_process_rows(model):
        rows = live_process_rows(model)
        state["rendered_processes"] = rows
        count = len(rows)
        daemon_count = model.get("daemon_count")
        if daemon_count is None:
            daemon_summary = "main dev-loop daemon count unknown"
        else:
            daemon_pids = model.get("daemon_pids") or []
            pid_text = ",".join(str(pid) for pid in daemon_pids) or "none"
            duplicate = " · DUPLICATE DAEMONS" if int(daemon_count) > 1 else ""
            daemon_summary = (
                f"main dev-loop daemons: {int(daemon_count)} (expected 1; PID {pid_text})"
                f"{duplicate}")
        daemon_error = str(model.get("daemon_inventory_error") or "")
        if daemon_error:
            daemon_summary += f" · daemon scan: {daemon_error[:100]}"
        if state.get("process_scan_error"):
            summary = (f"{daemon_summary} · PROCESS SCAN FAILED · showing stale list · "
                       f"{state['process_scan_error'][:100]}")
        else:
            servers = sum(1 for row in rows if row.get("role") == "server")
            clients = sum(1 for row in rows if row.get("role") == "client")
            unverified = count - servers - clients
            summary = (f"{daemon_summary} · {count} live · {servers} server(s) · "
                       f"{clients} client(s) · {unverified} unverified")
            if state.get("process_scan_in_flight"):
                summary += " · scan in progress"
        if process_summary.cget("text") != summary:
            process_summary.configure(text=summary)

        current_keys = {row["process_key"] for row in rows}
        for key in list(state["process_rows"]):
            if key in current_keys:
                continue
            widget = state["process_rows"].pop(key)["widget"]
            widget.destroy()
            if state.get("selected_process_key") == key:
                state["selected_process_key"] = ""

        for index, row in enumerate(rows):
            key = row["process_key"]
            row_state = state["process_rows"].get(key)
            uptime = row.get("uptime_seconds")
            uptime_text = (f"up={int(uptime) // 60:02d}:{int(uptime) % 60:02d}"
                           if uptime is not None else "")
            snapshot_age = row.get("snapshot_age_ms")
            snapshot_text = (f"snap={snapshot_age / 1000.0:.1f}s"
                             if snapshot_age is not None else "")
            target = (row.get("connected_to") or row.get("server_instance_id") or
                      row.get("connection_target") or "")
            shown_state = ("stale · scan failed" if state.get("process_scan_error")
                           else str(row.get("state") or "running"))
            parts = [str(row.get("checkout_id") or "main"),
                     str(row.get("instance_id") or "MiMITA"), str(row.get("role") or "unknown"),
                     shown_state, f"pid={row['pid']}"]
            parts.extend(value for value in (uptime_text, snapshot_text) if value)
            if target:
                parts.append(f"-> {target}")
            text = "  ".join(parts)
            if row_state is None:
                variable = tk.BooleanVar(value=False)
                widget = tk.Checkbutton(
                    process_rows_frame, text=text, variable=variable, anchor="w",
                    bg=BLACK, fg=RED, activebackground=BLACK, activeforeground=WHITE,
                    selectcolor=BLACK, disabledforeground="#660000", relief="flat",
                    bd=0, highlightthickness=0, font=(family, 11),
                    command=lambda process_key=key:
                    state.__setitem__("selected_process_key", process_key))
                row_state = {"variable": variable, "widget": widget,
                             "text": text, "row": None}
                state["process_rows"][key] = row_state
            else:
                widget = row_state["widget"]
                if row_state.get("text") != text:
                    widget.configure(text=text)
                    row_state["text"] = text
            if row_state.get("row") != index:
                widget.grid(row=index, column=0, sticky="ew")
                row_state["row"] = index
        process_rows_frame.grid_columnconfigure(0, weight=1)
        if not rows:
            if state.get("process_scan_error"):
                empty_text = "Process scan unavailable"
            elif state.get("process_scan_in_flight"):
                empty_text = "Scanning running MiMITA processes…"
            else:
                empty_text = "0 running MiMITA processes found"
            if not state.get("process_empty_label"):
                state["process_empty_label"] = label(process_rows_frame, empty_text, size=10)
            else:
                if state["process_empty_label"].cget("text") != empty_text:
                    state["process_empty_label"].configure(text=empty_text)
            if not state.get("process_empty_visible"):
                state["process_empty_label"].grid(row=0, column=0, sticky="w")
                state["process_empty_visible"] = True
        elif state.get("process_empty_label"):
            if state.get("process_empty_visible"):
                state["process_empty_label"].grid_forget()
                state["process_empty_visible"] = False

    def update_game_queue_panel(model: dict) -> None:
        snapshot = model.get("_game_queue_snapshot") or {}
        game_headline = str(snapshot.get("headline") or "Game DLL: checking…")
        game_detail = str(snapshot.get("detail") or "Reading root dev-loop game-build state")
        progress = snapshot.get("progress") or {}
        game_build_value.configure(text=game_headline)
        game_build_detail.configure(text=game_detail)
        summary = str(snapshot.get("summary") or "Checking build queue…")
        job_title = str(snapshot.get("job_title") or "Queue detail")
        stamp = str(snapshot.get("stamp") or "")
        ratio = snapshot.get("ratio")
        timer = str(snapshot.get("timer") or "DLL build timer unavailable")
        timer_color = (RED if "FAILED" in job_title else
                       ("#ffcc00" if "BUILDING" in job_title else GREEN))
        game_queue_timer.configure(text=timer, fg=timer_color)
        game_queue_summary.configure(text=summary)
        job_color = RED if ("FAILED" in job_title or "NO PROGRESS" in job_title) else (
            "#ffcc00" if "SLOW" in job_title else (GREEN if "IDLE" in job_title else WHITE))
        game_queue_title.configure(text=job_title, fg=job_color)
        game_queue_time.configure(text=stamp)

        width = max(1, game_queue_bar.winfo_width())
        height = max(18, game_queue_bar.winfo_height())
        game_queue_bar.delete("all")
        game_queue_bar.create_rectangle(1, 1, width - 1, height - 1,
                                        fill="#180000", outline=RED)
        if ratio is not None:
            game_queue_bar.create_rectangle(2, 2, max(2, int((width - 2) * ratio)),
                                            height - 2, fill=RED, outline="")
        phase_text = stamp.split("\n")[-1]
        game_queue_bar.create_text(width // 2, height // 2, text=phase_text,
                                   fill=WHITE, font=(family, 9, "bold"))

        lines = []
        attempt = str(progress.get("build_attempt_id") or "")
        if attempt:
            lines.append(f"attempt {attempt}")
        source_generation = progress.get("source_generation")
        if source_generation is not None:
            lines.append(f"source generation {source_generation}")
        health = str(progress.get("build_health") or "UNKNOWN")
        age_value = progress.get("progress_age_s")
        age_text = (f"{float(age_value):.0f}s"
                    if isinstance(age_value, (int, float)) else "unknown")
        lines.append(f"live health {health}  ·  progress age {age_text}")
        if progress.get("published_generation") is not None:
            lines.append(f"published generation {progress.get('published_generation')}")
        activation = progress.get("latest_live_activation")
        if isinstance(activation, dict) and activation.get("generation") is not None:
            lines.append(
                f"LATEST LIVE ACTIVATION: G{activation.get('generation')} · "
                f"{activation.get('time_utc') or 'time unknown'} · "
                f"{activation.get('dll') or 'DLL path unknown'}")
            if activation.get("generation") != progress.get("published_generation"):
                lines.append("  published generation is newer; runtime activation is not proven yet")
        elif isinstance(activation, dict) and activation.get("scan_pending"):
            lines.append("LATEST LIVE ACTIVATION: scanning recent server journal history…")
        else:
            lines.append("LATEST LIVE ACTIVATION: no activation event found in the current server journal")
        revision = str(progress.get("revision") or "")
        if revision:
            lines.append(f"source revision {revision}")
        if progress.get("build_mode"):
            lines.append(f"build mode {progress['build_mode']}")
        if progress.get("started_utc"):
            lines.append(f"attempt started {progress['started_utc']}")
        active_sources = progress.get("active_sources")
        if isinstance(active_sources, list) and active_sources:
            lines.append("currently compiling: " + ", ".join(str(path) for path in active_sources[:4]))
            active_started = progress.get("active_source_started") or {}
            for path in active_sources[:4]:
                started = active_started.get(path, {}) if isinstance(active_started, dict) else {}
                lines.append(f"  {path}  queued {float(started.get('queue_wait_ms') or 0):.0f} ms")

        notes = progress.get("notes") if isinstance(progress.get("notes"), list) else []
        if notes:
            lines.append("CHANGES:")
            for note in notes:
                if isinstance(note, dict):
                    lines.append("  " + str(note.get("changes") or "(empty note)"))
                    if note.get("files"):
                        lines.append("    files: " + ", ".join(map(str, note["files"])))
        elif progress.get("status") in ("preparing", "running"):
            lines.append("CHANGES: no agent note attached; run python devscripts/build-note.py --changes \"...\"")
        pending_notes = progress.get("pending_notes")
        if isinstance(pending_notes, list) and pending_notes:
            lines.append("NEXT BUILD NOTES:")
            lines.extend("  " + str(note.get("changes", "")) for note in pending_notes
                         if isinstance(note, dict))

        paths = progress.get("changed_paths")
        if isinstance(paths, list) and paths:
            lines.append("changed inputs: " + ", ".join(map(str, paths[:12])))

        timeline = progress.get("timeline")
        if isinstance(timeline, list) and timeline:
            phase_durations = [item for item in timeline
                               if isinstance(item, dict) and item.get("state") == "completed"]
            if phase_durations:
                lines.append("phase timeline:")
                lines.extend(
                    f"  {item.get('stage')}: {float(item.get('duration_ms') or 0):.0f} ms"
                    for item in phase_durations[-6:])

        recent = progress.get("file_events")
        if isinstance(recent, list) and recent:
            lines.append("LATEST SOURCE RESULTS:")
            for item in recent[-10:]:
                if not isinstance(item, dict):
                    continue
                result = {"compiled": "CXX", "skipped": "SKIP", "failed": "FAIL"}.get(
                    str(item.get("status")), str(item.get("status", "?")).upper())
                detail = (f"{float(item.get('compile_ms') or 0):.0f} ms compiler time"
                          if result == "CXX" else
                          f"{float(item.get('dependency_check_ms') or 0):.1f} ms for this TU dep check")
                queue_wait = float(item.get("queue_wait_ms") or 0)
                if queue_wait:
                    detail += f"  ·  {queue_wait:.0f} ms queued"
                lines.append(f"  {result}  {item.get('path', '')}  ·  {detail}")

        phase = str(progress.get("phase") or "")
        phase_elapsed = float(progress.get("phase_elapsed_ms") or 0) / 1000.0
        elapsed = progress.get("elapsed_s")
        subphase = str(progress.get("subphase") or "")
        if subphase:
            subphase_elapsed = float(progress.get("subphase_elapsed_ms") or 0) / 1000.0
            lines.append(f"active substep: {subphase} · {subphase_elapsed:.2f}s")
        if phase == "no_op" or progress.get("no_op"):
            lines.append(f"NO-OP: reusing published generation "
                         f"{progress.get('generation') or '?'}; no identity, compile, link, or publish")
        elif phase == "changed_source_preflight":
            candidates = int(progress.get("preflight_candidates") or 0)
            lines.append(f"changed-source compile preflight in progress: "
                         f"{phase_elapsed:.3f}s · {candidates} source(s)")
        elif phase == "object_cache_scan":
            scanned = int(progress.get("scan_scanned") or 0)
            total = int(progress.get("total") or 0)
            stale = int(progress.get("stale_count") or 0)
            lines.append(f"object/dependency scan in progress: {phase_elapsed:.3f}s · "
                         f"{scanned}/{total} checked · {stale} stale")
        elif isinstance(progress.get("object_scan_s"), (int, float)):
            lines.append(f"object/dependency scan: {float(progress['object_scan_s']):.3f}s")
        for key, phase_name, label_text in (
                ("compile_s", "compile", "compile wall time"),
                ("link_s", "link", "link"),
                ("publish_s", "publish", "publish")):
            value = progress.get(key)
            if phase == phase_name:
                lines.append(f"{label_text} in progress: {phase_elapsed:.3f}s")
            elif isinstance(value, (int, float)) and float(value) > 0:
                lines.append(f"{label_text}: {float(value):.3f}s")
        if isinstance(elapsed, (int, float)):
            total_text = "whole attempt" if progress.get("status") != "running" else "attempt elapsed"
            lines.append(f"{total_text}: {float(elapsed):.3f}s")
        if isinstance(progress.get("worker_elapsed_s"), (int, float)):
            lines.append(f"worker entry elapsed: {float(progress['worker_elapsed_s']):.3f}s")
        if progress.get("dependency_check_ms") is not None:
            lines.append(f"dependency checks sum: {float(progress['dependency_check_ms']):.1f} ms")
        if progress.get("scan_parse_ms") is not None or progress.get("scan_stat_ms") is not None:
            lines.append(
                f"scan detail: parse {float(progress.get('scan_parse_ms') or 0):.1f} ms · "
                f"stat {float(progress.get('scan_stat_ms') or 0):.1f} ms · "
                f"{int(progress.get('scan_unique_dependencies') or 0)} unique deps")
        if progress.get("dependency_prepass_ms") is not None:
            lines.append(
                "scan subtimers: prepass "
                f"{float(progress.get('dependency_prepass_ms') or 0) / 1000:.2f}s · "
                "path/fanout "
                f"{float(progress.get('dependency_path_and_fanout_ms') or 0) / 1000:.2f}s · "
                "other prepass "
                f"{float(progress.get('dependency_prepass_other_ms') or 0) / 1000:.2f}s · "
                "stat prewarm "
                f"{float(progress.get('stat_prewarm_ms') or 0) / 1000:.2f}s · "
                "object checks "
                f"{float(progress.get('object_check_wall_ms') or 0) / 1000:.2f}s")
        if progress.get("dependency_path_normalization_ms") is not None:
            lines.append(
                "dependency path normalization (includes filesystem resolve): "
                f"{float(progress.get('dependency_path_normalization_ms') or 0) / 1000:.2f}s · "
                "external path classification "
                f"{float(progress.get('external_dependency_classification_ms') or 0) / 1000:.2f}s")
        check_count = progress.get("dependency_check_count")
        if isinstance(check_count, int) and check_count:
            lines.append(
                f"per-TU dependency checks: {check_count} · min "
                f"{float(progress.get('dependency_check_min_ms') or 0):.2f} ms · "
                f"avg {float(progress.get('dependency_check_avg_ms') or 0):.2f} ms · "
                f"max {float(progress.get('dependency_check_max_ms') or 0):.2f} ms")
        invalidations = progress.get("cache_invalidations")
        if isinstance(invalidations, list) and invalidations:
            lines.append("REBUILD REASONS:")
            lines.extend(
                f"  {item.get('path')}: {item.get('reason')}"
                for item in invalidations[:12] if isinstance(item, dict))
        compile_sum_ms = progress.get("compile_sum_ms")
        if isinstance(compile_sum_ms, (int, float)) and compile_sum_ms:
            lines.append(
                f"compiler time summed across TUs {float(compile_sum_ms) / 1000:.2f}s · "
                f"min {float(progress.get('compile_min_ms') or 0) / 1000:.2f}s · "
                f"max {float(progress.get('compile_max_ms') or 0) / 1000:.2f}s")
        if progress.get("object_input_count") is not None:
            lines.append(
                f"link inputs: {int(progress.get('object_input_count') or 0)} objects · "
                f"{int(progress.get('linker_argument_count') or 0)} response-file entries · "
                f"{int(progress.get('object_input_bytes') or 0) / (1024 * 1024):.1f} MiB object data")
        if progress.get("cache_record_overhead_ms") is not None:
            lines.append(f"legacy per-skip reporting sum: {float(progress['cache_record_overhead_ms']):.1f} ms")
        if progress.get("dev_loop_output_handler_ms") is not None:
            lines.append(f"dev-loop output handling sum: {float(progress['dev_loop_output_handler_ms']):.1f} ms")
        phase_durations = progress.get("phase_durations_ms")
        if isinstance(phase_durations, dict) and phase_durations:
            lines.append("phase durations:")
            lines.extend(f"  {str(name).replace('_', ' ')}: {float(duration) / 1000:.2f}s"
                         for name, duration in list(phase_durations.items())[-8:])
        recent_generations = progress.get("generation_history")
        if isinstance(recent_generations, list) and recent_generations:
            lines.append("RECENT DLL GENERATIONS (total · preflight · scan · compile · link · publish):")
            for item in recent_generations:
                metrics = item.get("worker_metrics") if isinstance(item.get("worker_metrics"), dict) else {}
                phases = item.get("phase_durations_ms") if isinstance(item.get("phase_durations_ms"), dict) else {}
                total_seconds = float(item.get("total_seconds") or 0)
                phase_value = lambda key: float(phases.get(key) or 0) / 1000.0
                publish_seconds = phase_value("publish") or float(metrics.get("publish_seconds") or 0)
                lines.append(
                    f"  G{item.get('generation')}: {total_seconds:.1f}s · "
                    f"{float(metrics.get('changed_source_preflight_seconds') or 0):.1f} · "
                    f"{float(metrics.get('object_scan_seconds') or 0):.1f} · "
                    f"{float(metrics.get('compile_seconds') or 0):.1f} · "
                    f"{float(metrics.get('link_seconds') or 0):.1f} · {publish_seconds:.1f}s")
                changed = item.get("changed_paths") or []
                lines.append(
                    f"    changed {', '.join(map(str, changed[:2])) or 'unknown'} · "
                    f"compiled {metrics.get('compiled', '?')} / skipped {metrics.get('skipped', '?')} · "
                    f"{item.get('build_attempt_id', '')}")
                subphases = item.get("subphase_durations_ms")
                if isinstance(subphases, dict) and subphases:
                    nested = [f"{str(name).replace('_', ' ')} {float(duration) / 1000:.2f}s"
                              for name, duration in subphases.items()
                              if str(name).startswith(("publish.", "link."))]
                    if nested:
                        lines.append("    link/publish subtimers: " + " · ".join(nested))
                invalidations = item.get("cache_invalidations")
                if isinstance(invalidations, list) and invalidations:
                    reason_counts: dict[str, int] = {}
                    for invalidation in invalidations:
                        reason = str(invalidation.get("reason") or "unknown")
                        reason_counts[reason] = reason_counts.get(reason, 0) + 1
                    reasons = ", ".join(
                        f"{count}× {reason}" for reason, count in
                        sorted(reason_counts.items(), key=lambda pair: (-pair[1], pair[0])))
                    lines.append(f"    stale reasons: {reasons}")

        if not lines:
            lines.extend(["No active game DLL build.",
                          "Agents can annotate the next build with:",
                          "python devscripts/build-note.py --changes \"short explanation\""])
        set_text_content(game_queue_rows, "\n".join(lines),
                         "game_queue_rows_text")

    def apply_control_results() -> None:
        while True:
            try:
                (action, response, error, press_epoch,
                 press_monotonic, completed) = state["control_results"].get_nowait()
            except queue.Empty:
                return
            if press_monotonic is not None:
                state["launch_ack_elapsed"] = max(0.0, completed - press_monotonic)
            if error:
                action_status.configure(text=f"{action}: {error}")
                if press_monotonic is not None:
                    state["launch_timing_status"] = "failed"
                    state["launch_timing_message"] = error
                continue
            if isinstance(response, dict):
                accepted = bool(response.get("accepted"))
                message = response.get("message")
                request_id = str(response.get("request_id") or "")
            else:
                accepted = bool(response)
                message = None
                request_id = ""
            if accepted:
                state["pending_request_id"] = request_id
                if press_monotonic is not None:
                    state["launch_request_id"] = request_id
                action_status.configure(text=message or (
                    f"{action}: queued · request {request_id}" if request_id
                    else f"{action}: accepted"))
            else:
                action_status.configure(text=str(message or f"{action}: not accepted"))
                if press_monotonic is not None:
                    state["launch_timing_status"] = "failed"
                    state["launch_readiness_elapsed"] = state["launch_ack_elapsed"]
                    state["launch_timing_message"] = str(
                        message or "Launch request was not accepted")

    def refresh() -> None:
        state["refresh_after_id"] = None
        state["after_id"] = None
        if state["closing"]:
            return
        refresh_started = time.perf_counter()
        measure_ui_stage("control_results", apply_control_results)
        if not state["frozen"]:
            measure_ui_stage("status_result_apply", apply_status_refresh)
            request_status_refresh()
            dashboard_scroll_position = body_canvas.yview()[0]
            try:
                measure_ui_stage("process_scan_apply", apply_process_scan_result)
                model = state.get("latest_model") or {}
                measure_ui_stage("checkout_rows", lambda: render_checkout_rows(model))
                measure_ui_stage("runtime_rows", lambda: render_runtimes(model))
                measure_ui_stage("coordination_render", render_coordination)
                measure_ui_stage("process_scan_request", request_process_scan)
                modes = measure_ui_stage("configured_modes", _configured_launch_modes)
                modes_changed = modes != state["launch_modes"]
                if modes_changed:
                    state["launch_modes"] = modes
                active_mode = str(model.get("launch_mode") or "")
                state["active_mode"] = active_mode

                events_path = str(model.get("journal") or "")
                state["current_events_path"] = events_path
                launch = (
                    _launch_timing_from_journal(
                        events_path, state["launch_request_id"],
                        float(state["launch_started_epoch"] or time.time()),
                        allow_unmatched=bool(
                            events_path and events_path != state["launch_events_path_before"]))
                    if state["launch_timing_status"] == "pending" else {}
                )
                pending_checkout_id = state.get("pending_checkout_id") or "main"
                pending_checkout = next((item for item in model.get("checkouts", []) or []
                                         if str(item.get("checkout_id") or "") == pending_checkout_id), {})
                pending_state = pending_checkout.get("state") or {}
                last_control = (pending_state.get("last_control")
                                if pending_checkout_id != "main" else model.get("last_control")) or {}
                if (state["launch_request_id"]
                        and last_control.get("request_id") == state["launch_request_id"]
                        and last_control.get("status") in ("failed", "rejected")
                        and not launch.get("status")):
                    launch = {
                        "status": "failed",
                        "readiness_seconds": max(
                            0.0, time.monotonic() - state["launch_started_monotonic"]),
                        "message": str(last_control.get("message") or "Launch request failed"),
                    }
                if state["launch_request_id"] and launch:
                    client_started = launch.get("client_start_seconds")
                    client_reused = bool(launch.get("client_reused"))
                    readiness = launch.get("readiness_seconds")
                    if client_started is not None and state["launch_client_started_elapsed"] is None:
                        state["launch_client_started_elapsed"] = float(client_started)
                    if readiness is not None and state["launch_readiness_elapsed"] is None:
                        state["launch_readiness_elapsed"] = float(readiness)
                    state["launch_client_reused"] = client_reused
                    state["launch_timing_status"] = str(launch.get("status") or "pending")
                    state["launch_timing_message"] = str(launch.get("message") or "")
                elif (state["launch_timing_status"] == "pending"
                      and str(model.get("phase") or "").lower() in ("failed", "build_failed")
                      and str(model.get("artifact_id") or "—") == "—"):
                    state["launch_timing_status"] = "failed"
                    state["launch_readiness_elapsed"] = max(
                        0.0, time.monotonic() - state["launch_started_monotonic"])
                    state["launch_timing_message"] = str(
                        model.get("message") or "Build failed before a client could launch")

                if state["launch_timing_status"] == "pending" and state["launch_started_monotonic"] is not None:
                    elapsed = max(0.0, time.monotonic() - state["launch_started_monotonic"])
                    client = state["launch_client_started_elapsed"]
                    ready = state["launch_readiness_elapsed"]
                    ack = state["launch_ack_elapsed"]
                    ack_text = f"ack {ack * 1000.0:.0f} ms  ·  " if ack is not None else ""
                    if ready is not None:
                        client_text = (f"client {client:.2f}s" if client is not None else
                                       "client reused" if state["launch_client_reused"] else
                                       "client not started")
                        launch_text = f"Launch: {ack_text}{client_text}  ·  readiness {ready:.2f}s"
                    elif client is not None:
                        launch_text = f"Launch: {ack_text}client {client:.2f}s  ·  readiness {elapsed:.2f}s…"
                    elif state["launch_client_reused"]:
                        launch_text = f"Launch: {ack_text}client reused  ·  readiness {elapsed:.2f}s…"
                    else:
                        launch_text = f"Launch: {ack_text}waiting for client  ·  {elapsed:.2f}s"
                    action_status.configure(text=launch_text)
                elif state["launch_timing_status"] in ("succeeded", "failed"):
                    client = state["launch_client_started_elapsed"]
                    ready = state["launch_readiness_elapsed"]
                    ack = state["launch_ack_elapsed"]
                    ack_text = f"ack {ack * 1000.0:.0f} ms  ·  " if ack is not None else ""
                    client_text = (f"client {client:.2f}s" if client is not None else
                                   "client reused" if state["launch_client_reused"] else
                                   "client not started")
                    ready_text = f"readiness {ready:.2f}s" if ready is not None else "readiness unavailable"
                    message = state["launch_timing_message"]
                    message_text = f"  ·  {message[:100]}" if (
                        state["launch_timing_status"] == "failed" and message) else ""
                    action_status.configure(text=(
                        f"Launch {state['launch_timing_status']}: {ack_text}{client_text}  ·  "
                        f"{ready_text}{message_text}"))

                artifact = str(model.get("artifact_id") or "—")
                latest_hash = str(model.get("sha256") or "—")
                latest_id = str(model.get("build_id") or "—")
                running_artifact = str(model.get("running_artifact_id") or "—")
                running_hash = str(model.get("running_sha256") or "—")
                running_id = str(model.get("running_build_id") or "—")
                latest_artifact_key = _artifact_key(artifact)
                running_artifact_key = _artifact_key(running_artifact)
                same_build = bool(
                    (latest_id != "—" and latest_id == running_id)
                    or (latest_hash != "—" and latest_hash == running_hash
                        and latest_artifact_key == running_artifact_key)
                )

                running_value.configure(text=running_artifact)
                latest_value.configure(text=artifact)
                running_full = f"SHA-256 {running_hash}\nBuild ID {running_id}"
                latest_full = f"SHA-256 {latest_hash}\nBuild ID {latest_id}"
                running_short = f"SHA-256 {running_hash[:12]}…  ·  click to copy"
                latest_short = f"SHA-256 {latest_hash[:12]}…  ·  click to copy"
                running_detail.full_text, running_detail.short_text = running_full, running_short
                latest_detail.full_text, latest_detail.short_text = latest_full, latest_short
                expanded = state["wide"] or not state["compact"]
                running_detail.configure(text=running_full if expanded else running_short)
                latest_detail.configure(text=latest_full if expanded else latest_short)
                running_value.bind("<Button-1>", lambda event, a=running_artifact, h=running_hash:
                                   copy_build_identity(event, a, h))
                latest_value.bind("<Button-1>", lambda event, a=artifact, h=latest_hash:
                                  copy_build_identity(event, a, h))
                running_detail.bind("<Button-1>", lambda event, a=running_artifact, h=running_hash:
                                    copy_build_identity(event, a, h))
                latest_detail.bind("<Button-1>", lambda event, a=artifact, h=latest_hash:
                                   copy_build_identity(event, a, h))
                match_color = GREEN if same_build else ACCENT
                running_value.configure(fg=match_color)
                latest_value.configure(fg=match_color)
                running_detail.configure(fg=match_color)
                latest_detail.configure(fg=match_color)

                phase = str(model.get("phase") or "unknown").lower()
                if model.get("stale") and phase == "published_current":
                    phase_text = "SOURCE CHANGED · BUILD NEEDED"
                    phase_detail.configure(text=model.get("phase_detail") or "Current source is newer than the published build")
                    phase_color = ACCENT
                elif phase == "published_current" and not model.get("identity_valid", True):
                    phase_text = "BUILD IDENTITY INVALID"
                    phase_detail.configure(text=model.get("phase_detail") or
                                           "The latest artifact identity could not be verified")
                    phase_color = RED
                elif phase == "published_current":
                    phase_text = "UP TO DATE"
                    phase_detail.configure(text=f"Most recent build: {artifact}")
                    phase_color = GREEN
                elif phase in ("building", "pending", "queued"):
                    phase_text = f"{phase.upper()}  ·  {model.get('elapsed') or '00:00:00'}"
                    phase_detail.configure(text=model.get("phase_detail") or "Build is active")
                    phase_color = ACCENT
                elif "fail" in phase or "error" in phase:
                    phase_text = phase.upper()
                    phase_detail.configure(text=model.get("phase_detail") or "Build failed")
                    phase_color = RED
                else:
                    phase_text = phase.replace("_", " ").upper()
                    phase_detail.configure(text=model.get("phase_detail") or "Waiting for build activity")
                    phase_color = ACCENT
                phase_value.configure(text=phase_text, fg=phase_color)

                measure_ui_stage("game_queue_panel",
                                 lambda: update_game_queue_panel(model))

                runtime_value.configure(text=model.get("runtime", "Not running"),
                                         fg=GREEN if same_build else ACCENT)
                instance_lines = []
                for inst in model.get("instances", []) or []:
                    age = inst.get("snapshot_age_ms")
                    age_text = f"{age / 1000.0:.1f}s" if age is not None else "-"
                    instance_lines.append(
                        f"{inst.get('instance_id', '?')} {inst.get('role', '?')} "
                        f"{inst.get('state', '?')} pid={inst.get('pid') or '-'} "
                        f"snap={age_text}")
                runtime_detail.configure(text=(
                    f"{model.get('server_status', 'server unknown')}  ·  "
                    f"{model.get('client_status', 'client unknown')}  ·  "
                    f"builds {'MATCH' if same_build else 'DIFFER'}"
                    + ("\n" + "\n".join(instance_lines) if instance_lines else "")
                ))
                measure_ui_stage("process_rows", lambda: render_process_rows(model))
                mode = str(model.get("mode") or "—")
                map_name = str(model.get("map") or "—")
                mode_value.configure(text=mode, fg=ACCENT)
                map_value.configure(text=map_name, fg=ACCENT)

                scenario = model.get("scenario") or {}
                scenario_value.configure(
                    text=f"{scenario.get('verdict', '—')}  ·  {scenario.get('id', '—')}",
                    fg=ACCENT)
                scenario_detail.configure(text=(
                    f"{scenario.get('state', 'No scenario recorded')}  ·  "
                    f"frame {scenario.get('frame', '—')}  ·  tick {scenario.get('tick', '—')}  ·  "
                    f"checkpoint {scenario.get('checkpoint', '—')} "
                    f"{scenario.get('checkpoint_progress', '')}  ·  "
                    f"capture {scenario.get('complete', 'unknown')}\n"
                    f"map {scenario.get('map', '—')}  ·  "
                    f"build {scenario.get('build_id', latest_id)}\n"
                    f"journal {model.get('journal') or '(none)'}\n"
                    f"{scenario.get('log_usage', '')}"
                ))

                subtitle.configure(text=(
                    f"Single coordinator dashboard  ·  {len(model.get('checkouts') or [])} checkout(s)/lane(s)  ·  "
                    f"main daemon {model.get('daemon_pid', '—')}"
                ))
                refreshed = state.get("dashboard_refresh_monotonic") or time.monotonic()
                refresh_age = int(max(0.0, time.monotonic() - refreshed))
                journal_path = str(model.get("journal") or "(no active run journal)")
                checkout_path = str(model.get("checkout") or "")
                if checkout_path and journal_path != "(no active run journal)":
                    try:
                        journal_path = Path(journal_path).resolve().relative_to(
                            Path(checkout_path).resolve()).as_posix()
                    except (OSError, ValueError):
                        pass
                refresh_indicator.configure(
                    text=((f"Status read failed · {state['status_refresh_error'][:100]}  ·  "
                           if state["status_refresh_error"] else "")
                          + f"Updated {refresh_age}s ago  ·  "
                          f"{state['dashboard_refresh_utc']}  ·  "
                          f"GUI refresh #{state['dashboard_refresh_count']:06d}"))
                source_summary.configure(
                    text=("Build input: main dev-loop status RPC "
                          "(build_output_tail/message)\n"
                          f"Loop input: {journal_path}  ·  "
                          f"daemon status timestamp: {model.get('updated_at') or '—'}"))
                build_output = [str(line) for line in (model.get("build_output") or [])]
                build_lines = [f"[BUILD] {line}" for line in build_output]
                message = str(model.get("message") or "")
                if message and message != state["last_live_message"]:
                    if message not in build_output:
                        state["live_output"].append(f"[LIVE] {message}")
                    state["last_live_message"] = message
                activity_lines = [f"[LOOP] {line}" for line in (model.get("recent") or [])]
                source_line = model.get("source_activity")
                if source_line:
                    activity_lines.append(f"[SOURCE] {source_line}")
                lines = build_lines + list(state["live_output"]) + activity_lines
                if not lines:
                    lines = ["Waiting for build or contributor activity…"]
                measure_ui_stage(
                    "output_text",
                    lambda: set_text_content(output, "\n".join(lines), "output_text"))
                measure_ui_stage("gutter", update_gutter)
                last_control = (pending_state.get("last_control")
                                if pending_checkout_id != "main" else model.get("last_control")) or {}
                pending_request = state.get("pending_request_id")
                if (pending_request and last_control.get("request_id") == pending_request
                        and last_control.get("status") in ("completed", "failed", "rejected")):
                    results = last_control.get("results") or []
                    result_text = " · ".join(
                        f"pid={result.get('pid', '?')} "
                        f"{'stopped' if result.get('stopped') else 'failed'}"
                        for result in results[:6] if isinstance(result, dict))
                    if not (pending_request == state["launch_request_id"]
                            and state["launch_timing_status"] in ("succeeded", "failed")):
                        action_status.configure(text=last_control.get("message") or
                                                 f"{last_control.get('action', 'action')}: "
                                                 f"{last_control.get('status')}"
                                                 + (f" · {result_text}" if result_text else ""))
                    state["pending_request_id"] = ""
                root.after_idle(lambda: body_canvas.yview_moveto(
                    dashboard_scroll_position))
            except Exception as error:
                try:
                    action_status.configure(text=f"Status refresh failed: {error}")
                except tk.TclError:
                    pass
        refresh_ms = round((time.perf_counter() - refresh_started) * 1000.0, 1)
        if refresh_ms >= 100.0:
            log_dashboard_event(
                "dashboard.refresh.slow", {"duration_ms": refresh_ms,
                                            "refresh_number": state["dashboard_refresh_count"]},
                cooldown_key="last_slow_refresh_log")
        if not state["closing"]:
            state["after_id"] = root.after(1000, refresh)

    mode_map = tk.Frame(runtime_card, bg=BLACK)
    mode_map.grid(row=3, column=0, sticky="ew")
    label(mode_map, "Mode:", size=10).grid(row=0, column=0, sticky="w")
    mode_value = label(mode_map, "—", size=10, bold=True)
    mode_value.grid(row=0, column=1, sticky="w", padx=(3, 12))
    label(mode_map, "Map:", size=10).grid(row=0, column=2, sticky="w")
    map_value = label(mode_map, "—", size=10, bold=True)
    map_value.grid(row=0, column=3, sticky="w", padx=(3, 0))

    root.bind("<Configure>", schedule_reflow, add="+")
    def on_map(_event):
        state["minimized"] = False
        if os.name == "nt":
            root.after_idle(mark_as_app_window)

    root.bind("<Map>", on_map, add="+")
    root.bind("<Key-1>", lambda _event: send("build-or-launch"))
    root.bind("<Key-b>", lambda _event: send("build"))
    root.bind("<Key-v>", lambda _event: send("toggle-visibility"))
    root.bind("<Key-a>", lambda _event: send("toggle-auto-restart"))
    root.bind("<Key-p>", lambda _event: toggle_freeze())
    root.bind("<Key-x>", lambda _event: close_view())

    def cleanup():
        for key in ("after_id",):
            if state[key] is not None:
                try:
                    root.after_cancel(state[key])
                except tk.TclError:
                    pass
        coordination_store_value = state.get("coordination_store")
        if coordination_store_value is not None:
            try:
                coordination_store_value.close()
            except Exception:  # noqa: BLE001 - view shutdown must not raise
                pass
            state["coordination_store"] = None
        if font_library:
            try:
                font_library.RemoveFontResourceExW.argtypes = [ctypes.c_wchar_p,
                                                               ctypes.c_uint,
                                                               ctypes.c_void_p]
                font_library.RemoveFontResourceExW.restype = ctypes.c_int
                font_library.RemoveFontResourceExW(str(FONT_PATH), 0x10, None)
            except (AttributeError, OSError):
                pass

    root.protocol("WM_DELETE_WINDOW", close_view)
    root.bind("<Destroy>", lambda event: cleanup() if event.widget is root else None, add="+")
    schedule_reflow()
    state["refresh_after_id"] = root.after(0, refresh)
    root.mainloop()
    return 0
