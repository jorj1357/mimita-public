#!/usr/bin/env python3
"""Apply small, reversible VPS storage-policy edits from a reviewed checkout."""

from __future__ import annotations

import argparse
from collections import Counter
from contextlib import ExitStack
from datetime import datetime, timezone
import os
from pathlib import Path
import re
import shlex
import shutil
import stat
import subprocess
import tempfile
from urllib.parse import urlsplit

try:
    import fcntl
except ImportError:  # Windows hosts can run the pure transformation tests only.
    fcntl = None

try:
    import grp
except ImportError:  # Windows hosts can run the pure transformation tests only.
    grp = None


def remove_coturn_verbose(text: str) -> tuple[str, bool]:
    lines = text.splitlines(keepends=True)
    matches = [index for index, line in enumerate(lines) if line.rstrip("\r\n") == "verbose"]
    if len(matches) > 1:
        raise ValueError("multiple_standalone_verbose_directives")
    if not matches:
        return text, False
    del lines[matches[0]]
    return "".join(lines), True


def _apt_entry(line: str):
    try:
        parts = shlex.split(line, comments=True)
    except ValueError:
        return None
    if not parts or parts[0] not in {"deb", "deb-src"}:
        return None
    uri_index = next(
        (index for index, item in enumerate(parts) if item.startswith(("http://", "https://"))),
        None,
    )
    if uri_index is None or uri_index + 1 >= len(parts):
        return None
    uri = urlsplit(parts[uri_index])
    try:
        host = uri.hostname
        port = uri.port
        username = uri.username
        password = uri.password
    except ValueError:
        return None
    if not host or username is not None or password is not None or uri.query or uri.fragment:
        return None
    path = uri.path.rstrip("/")
    key = tuple(
        parts[:uri_index]
        + [uri.scheme.lower(), port, path, uri.query]
        + parts[uri_index + 1 :]
    )
    return host.lower(), key


def remove_duplicate_ubuntu_mirror_entries(text: str) -> tuple[str, int]:
    lines = text.splitlines(keepends=True)
    canonical = Counter()
    mirror = Counter()
    for line in lines:
        entry = _apt_entry(line)
        if not entry:
            continue
        host, key = entry
        if host == "archive.ubuntu.com":
            canonical[key] += 1
        elif host == "ubuntu.mirror.constant.com":
            mirror[key] += 1
    if not mirror:
        return text, 0
    if any(canonical[key] < count for key, count in mirror.items()):
        raise ValueError("ubuntu_mirror_definitions_not_exact_duplicates")

    result = []
    removed = 0
    for line in lines:
        entry = _apt_entry(line)
        if entry and entry[0] == "ubuntu.mirror.constant.com":
            removed += 1
            continue
        result.append(line)
    return "".join(result), removed


def update_rsyslog_rotation(text: str) -> tuple[str, bool]:
    lines = text.splitlines(keepends=True)
    paths = [index for index, line in enumerate(lines) if line.strip() == "/var/log/syslog"]
    if len(paths) != 1:
        raise ValueError("expected_one_syslog_logrotate_rule")
    start = paths[0]
    opening = next((i for i in range(start + 1, len(lines)) if lines[i].strip() == "{"), None)
    if opening is None:
        raise ValueError("syslog_logrotate_rule_missing_open_brace")
    closing = next((i for i in range(opening + 1, len(lines)) if lines[i].strip() == "}"), None)
    if closing is None:
        raise ValueError("syslog_logrotate_rule_missing_close_brace")

    body = lines[opening + 1 : closing]
    filtered = []
    for line in body:
        directive = line.strip().split(maxsplit=1)[0] if line.strip() else ""
        if directive in {"daily", "weekly", "monthly", "yearly", "rotate", "size", "maxsize", "minsize"}:
            continue
        filtered.append(line)
    directives = {line.strip().split(maxsplit=1)[0] for line in filtered if line.strip()}
    additions = ["\tdaily\n", "\trotate 7\n", "\tmaxsize 100M\n"]
    if "compress" not in directives:
        additions.append("\tcompress\n")
    if "delaycompress" not in directives:
        additions.append("\tdelaycompress\n")
    new_lines = lines[: opening + 1] + additions + filtered + lines[closing:]
    updated = "".join(new_lines)
    return updated, updated != text


def remove_redundant_backup_cron_lines(text: str) -> tuple[str, frozenset[str]]:
    lines = text.splitlines(keepends=True)
    removed = set()
    output = []
    signatures = {
        "old_direct_dump": lambda line: "pg_dumpall" in line and "gdrive:ravgrowth-db-backup-v4" in line and "/root/db_backups/" in line,
        "duplicate_dump_script": lambda line: "/root/scripts/dump-all-dbs.sh" in line,
        "old_backup_prune": lambda line: "find /root/db_backups" in line and "-mtime +30" in line and "-delete" in line,
    }
    counts = Counter()
    for line in lines:
        match = next((name for name, predicate in signatures.items() if not line.lstrip().startswith("#") and predicate(line)), None)
        if match:
            counts[match] += 1
            removed.add(match)
            continue
        output.append(line)
    if any(count > 1 for count in counts.values()):
        raise ValueError("duplicate_legacy_backup_cron_signatures")
    return "".join(output), frozenset(removed)


def _atomic_write(path: Path, content: str, info: os.stat_result) -> None:
    fd, temp_name = tempfile.mkstemp(prefix=f".{path.name}.mimita-", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8", newline="") as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.chown(temp_name, info.st_uid, info.st_gid)
        os.chmod(temp_name, stat.S_IMODE(info.st_mode))
        os.replace(temp_name, path)
    except BaseException:
        try:
            os.unlink(temp_name)
        except FileNotFoundError:
            pass
        raise


def _atomic_replace(
    path: Path,
    content: str,
    backup_directory: Path | None = None,
) -> tuple[bool, Path | None]:
    info = path.lstat()
    if not stat.S_ISREG(info.st_mode):
        raise ValueError("target_must_be_regular_file")
    old = path.read_text(encoding="utf-8")
    if old == content:
        return False, None

    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    backup_root = backup_directory or path.parent
    if backup_directory:
        backup_root.mkdir(mode=0o700, parents=True, exist_ok=True)
        if (backup_root.is_symlink() or backup_root.stat().st_uid != 0 or
                stat.S_IMODE(backup_root.stat().st_mode) != 0o700):
            raise ValueError("private_config_backup_directory_unavailable")
    backup = backup_root / f"{path.name}.pre-mimita-{timestamp}.bak"
    if backup.exists():
        raise FileExistsError("timestamped_backup_already_exists")
    shutil.copy2(path, backup, follow_symlinks=False)
    os.chown(backup, 0, 0)
    os.chmod(backup, 0o600)
    _atomic_write(path, content, info)
    return True, backup


def _require_root() -> None:
    if os.geteuid() != 0:
        raise PermissionError("run_as_root")


def apply_coturn() -> None:
    _require_root()
    if grp is None:
        raise RuntimeError("coturn_group_lookup_unavailable")
    path = Path("/etc/turnserver.conf")
    if (path.is_symlink() or path.stat().st_uid != 0 or
            path.stat().st_gid != grp.getgrnam("turnserver").gr_gid or
            stat.S_IMODE(path.stat().st_mode) != 0o640):
        raise ValueError("coturn_config_owner_or_mode_unexpected")
    updated, changed = remove_coturn_verbose(path.read_text(encoding="utf-8"))
    if not changed:
        print("coturn_verbose_already_disabled")
        return
    _, backup = _atomic_replace(path, updated)
    print(f"coturn_logging_config_updated backup={backup.name}")


def restore_coturn(backup_name: str) -> None:
    _require_root()
    if not re.fullmatch(r"turnserver\.conf\.pre-mimita-[0-9]{8}T[0-9]{6}Z\.bak", backup_name):
        raise ValueError("invalid_coturn_backup_name")
    backup = Path("/etc") / backup_name
    info = backup.lstat()
    if not stat.S_ISREG(info.st_mode) or info.st_uid != 0 or stat.S_IMODE(info.st_mode) != 0o600:
        raise ValueError("coturn_backup_owner_or_mode_unexpected")
    _atomic_replace(Path("/etc/turnserver.conf"), backup.read_text(encoding="utf-8"))
    print("coturn_config_restored_from_root_only_backup")


def apply_log_policy(repository_root: Path) -> None:
    _require_root()
    backup_directory = Path("/root/mimita-vps-config-backups")
    logrotate_path = Path("/etc/logrotate.d/rsyslog")
    updated, changed = update_rsyslog_rotation(logrotate_path.read_text(encoding="utf-8"))
    backup = None
    if changed:
        _, backup = _atomic_replace(logrotate_path, updated, backup_directory)
    check = shutil.which("logrotate")
    if not check:
        if backup:
            _atomic_write(logrotate_path, backup.read_text(encoding="utf-8"), logrotate_path.stat())
        raise FileNotFoundError("logrotate_missing")
    result = subprocess.run(
        [check, "--debug", "/etc/logrotate.conf"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    if result.returncode:
        if backup:
            _atomic_write(logrotate_path, backup.read_text(encoding="utf-8"), logrotate_path.stat())
        raise RuntimeError("logrotate_configuration_invalid")

    journal_source = repository_root / "deploy" / "journald-mimita-storage.conf"
    journal_target = Path("/etc/systemd/journald.conf.d/50-mimita-storage.conf")
    journal_target.parent.mkdir(mode=0o755, parents=True, exist_ok=True)
    journal_content = journal_source.read_text(encoding="utf-8")
    if journal_target.exists():
        _atomic_replace(journal_target, journal_content, backup_directory)
    else:
        fd = os.open(journal_target, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o644)
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            stream.write(journal_content)
            stream.flush()
            os.fsync(stream.fileno())
        os.chown(journal_target, 0, 0)
        os.chmod(journal_target, 0o644)
    subprocess.run(["systemctl", "restart", "systemd-journald"], check=True)
    print("vps_log_retention_policy_applied")


def apply_apt_mirror() -> None:
    _require_root()
    if fcntl is None:
        raise RuntimeError("apt_lock_check_unavailable")
    path = Path("/etc/apt/sources.list")
    updated, removed = remove_duplicate_ubuntu_mirror_entries(path.read_text(encoding="utf-8"))
    if removed == 0:
        print("apt_duplicate_ubuntu_mirror_not_present")
        return
    fuser = shutil.which("fuser")
    if not fuser:
        raise RuntimeError("package_lock_inspection_unavailable")
    with ExitStack() as locks:
        for lock_path in ("/var/lib/apt/lists/lock", "/var/lib/dpkg/lock-frontend", "/var/lib/dpkg/lock"):
            owner_check = subprocess.run([fuser, "-s", lock_path], check=False)
            if owner_check.returncode == 0:
                raise RuntimeError("package_manager_busy")
            if owner_check.returncode != 1:
                raise RuntimeError("package_lock_inspection_failed")
            stream = locks.enter_context(open(lock_path, "r+", encoding="utf-8"))
            try:
                fcntl.lockf(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError as error:
                raise RuntimeError("package_manager_busy") from error
        _, backup = _atomic_replace(path, updated)
    print(f"apt_duplicate_ubuntu_sources_removed count={removed} backup={backup.name}")


def retire_legacy_backup_cron() -> None:
    _require_root()
    status_path = Path("/var/lib/mimita-database-backup/status")
    match = re.fullmatch(r"success ([0-9]+)\s*", status_path.read_text(encoding="utf-8"))
    now = int(datetime.now(timezone.utc).timestamp())
    if not match or int(match.group(1)) > now or now - int(match.group(1)) > 26 * 60 * 60:
        raise RuntimeError("new_verified_backup_missing_or_stale")
    systemctl = shutil.which("systemctl")
    crontab = shutil.which("crontab")
    if not systemctl or not crontab:
        raise RuntimeError("cron_or_systemd_tool_missing")
    if subprocess.run([systemctl, "is-enabled", "--quiet", "mimita-database-backup.timer"], check=False).returncode:
        raise RuntimeError("new_backup_timer_not_enabled")
    if subprocess.run([systemctl, "is-active", "--quiet", "mimita-database-backup.timer"], check=False).returncode:
        raise RuntimeError("new_backup_timer_not_active")

    current = subprocess.run([crontab, "-l"], capture_output=True, text=True, check=False)
    if current.returncode:
        raise RuntimeError("root_crontab_unavailable")
    updated, removed = remove_redundant_backup_cron_lines(current.stdout)
    if not removed:
        print("legacy_database_backup_cron_entries_not_present")
        return

    backup_dir = Path("/root/mimita-db-verified-backups")
    if backup_dir.is_symlink() or not backup_dir.is_dir() or backup_dir.stat().st_uid != 0 or stat.S_IMODE(backup_dir.stat().st_mode) != 0o700:
        raise RuntimeError("protected_backup_directory_unavailable")
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    backup = backup_dir / f"root-crontab.pre-mimita-{timestamp}.bak"
    fd = os.open(backup, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as stream:
        stream.write(current.stdout)
        stream.flush()
        os.fsync(stream.fileno())

    result = subprocess.run([crontab, "-"], input=updated, text=True, capture_output=True, check=False)
    if result.returncode:
        subprocess.run([crontab, "-"], input=current.stdout, text=True, capture_output=True, check=False)
        raise RuntimeError("root_crontab_update_failed")
    print(f"legacy_backup_cron_retired count={len(removed)} categories={','.join(sorted(removed))} backup={backup.name}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    choices = parser.add_mutually_exclusive_group(required=True)
    choices.add_argument("--coturn", action="store_true")
    choices.add_argument("--restore-coturn-backup")
    choices.add_argument("--log-policy", action="store_true")
    choices.add_argument("--apt-mirror", action="store_true")
    choices.add_argument("--retire-legacy-db-cron", action="store_true")
    args = parser.parse_args()
    try:
        if args.coturn:
            apply_coturn()
        elif args.restore_coturn_backup:
            restore_coturn(args.restore_coturn_backup)
        elif args.log_policy:
            apply_log_policy(Path(__file__).resolve().parent.parent)
        elif args.apt_mirror:
            apply_apt_mirror()
        elif args.retire_legacy_db_cron:
            retire_legacy_backup_cron()
    except Exception as error:
        print(f"vps_storage_policy_failed category={type(error).__name__}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
