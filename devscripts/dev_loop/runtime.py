"""Runtime timestamps, journals, and exact executable identity."""

from __future__ import annotations

import datetime
import hashlib
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
_EXECUTABLE_IDENTITY_CACHE: dict[str, tuple[tuple[int, int], dict]] = {}


def invalidate_executable_identity(path: Path) -> None:
    """Forget a cached identity after replacing an executable at a stable path."""
    _EXECUTABLE_IDENTITY_CACHE.pop(str(path), None)


def create_shared_events_path() -> str:
    """Create one timestamped journal path shared by a server and client."""
    now = datetime.datetime.now(datetime.timezone.utc)
    run_dir = ROOT / "logs" / now.strftime("%Y-%m-%d") / now.strftime("%Y%m%d_%H%M%S")
    suffix = 1
    base = run_dir
    while run_dir.exists():
        run_dir = Path(f"{base}_{suffix:02d}")
        suffix += 1
    run_dir.mkdir(parents=True, exist_ok=True)
    return str(run_dir / "events.jsonl")


def utc_now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec="milliseconds").replace("+00:00", "Z")


def executable_snapshot_is_valid(path: Path) -> tuple[bool, str, str]:
    """Check PE structure, executable sections, entry point, and file SHA-256."""
    try:
        size = path.stat().st_size
        with path.open("rb") as handle:
            header = handle.read(64)
            if size < 0x1000 or header[:2] != b"MZ":
                return False, "invalid DOS header or file too small", ""
            pe_offset = struct.unpack_from("<I", header, 0x3C)[0]
            if pe_offset + 24 > size:
                return False, "invalid PE header", ""
            handle.seek(pe_offset)
            pe_header = handle.read(24)
            if pe_header[:4] != b"PE\0\0":
                return False, "invalid PE header", ""
            section_count = struct.unpack_from("<H", pe_header, 6)[0]
            optional_size = struct.unpack_from("<H", pe_header, 20)[0]
            section_offset = pe_offset + 24 + optional_size
            if optional_size < 20 or section_offset + section_count * 40 > size:
                return False, "invalid optional header or section table", ""
            optional_header = handle.read(optional_size)
            magic = struct.unpack_from("<H", optional_header)[0]
            entry_rva = struct.unpack_from("<I", optional_header, 16)[0]
            if magic not in (0x10B, 0x20B):
                return False, "invalid optional header or section table", ""

            executable_sections = 0
            entrypoint_checked = False
            for index in range(section_count):
                handle.seek(section_offset + index * 40)
                section = handle.read(40)
                virtual_size, virtual_address = struct.unpack_from("<II", section, 8)
                raw_size, raw_pointer = struct.unpack_from("<II", section, 16)
                characteristics = struct.unpack_from("<I", section, 36)[0]
                if not characteristics & 0x20000000:
                    continue
                executable_sections += 1
                if raw_size == 0 or raw_pointer + raw_size > size:
                    return False, "executable section is outside the file", ""
                handle.seek(raw_pointer)
                remaining = raw_size
                has_code = False
                while remaining:
                    chunk = handle.read(min(1024 * 1024, remaining))
                    if not chunk:
                        break
                    if any(chunk):
                        has_code = True
                        break
                    remaining -= len(chunk)
                if not has_code:
                    return False, "executable section is entirely zero-filled", ""
                if virtual_address <= entry_rva < virtual_address + max(virtual_size, raw_size):
                    handle.seek(raw_pointer + entry_rva - virtual_address)
                    entry_bytes = handle.read(64)
                    if len(entry_bytes) < 16 or not any(entry_bytes):
                        return False, "entrypoint bytes are zero-filled", ""
                    entrypoint_checked = True

            if executable_sections == 0:
                return False, "no executable sections", ""
            if not entrypoint_checked:
                return False, "entrypoint is not in an executable section", ""
            handle.seek(0)
            digest = hashlib.sha256()
            for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                digest.update(chunk)
            return True, "ok", digest.hexdigest()
    except OSError as error:
        return False, "read failed", str(error)


def executable_identity(path: Path) -> dict:
    try:
        stat = path.stat()
    except OSError:
        stat = None
    key = str(path)
    signature = (stat.st_size, stat.st_mtime_ns) if stat else None
    cached = _EXECUTABLE_IDENTITY_CACHE.get(key)
    if signature is not None and cached and cached[0] == signature:
        return dict(cached[1])
    valid, reason, digest = executable_snapshot_is_valid(path)
    identity = {
        "path": str(path),
        "exists": bool(stat),
        "valid": valid,
        "validation": reason,
        "sha256": digest if valid else "",
        "created_utc": datetime.datetime.fromtimestamp(
            stat.st_ctime, datetime.timezone.utc
        ).isoformat(timespec="milliseconds").replace("+00:00", "Z") if stat else "",
        "modified_utc": datetime.datetime.fromtimestamp(
            stat.st_mtime, datetime.timezone.utc
        ).isoformat(timespec="milliseconds").replace("+00:00", "Z") if stat else "",
        "size_bytes": stat.st_size if stat else 0,
    }
    if signature is not None:
        _EXECUTABLE_IDENTITY_CACHE[key] = (signature, identity)
    return dict(identity)
