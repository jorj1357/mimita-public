#!/usr/bin/env python3
# 2026-11-19
"""Shared persistent coordination brain for concurrent MiMITA agents.

The brain owns tasks, user directives, append-only notes, decisions, messages,
short edit leases, shared-file regions, freezes, and the global agent halt.
It is deliberately independent of builds, gameplay, and the dev-loop daemon.

Agents are named workers. The brain is the shared source of truth.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import sqlite3
import subprocess
import sys
import time
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from devscripts.path_containment import resolve_within  # noqa: E402

DEFAULT_TTL_MINUTES = 15
DEFAULT_QUESTION_MINUTES = 5
LONG_QUESTION_MINUTES = 15
LEASE_CHECK_MINUTES = 10
EVENT_LEVEL = "IMPORTANT"
PROTECTED_PREFIXES = (".git/", ".dev/", "build/", "logs/")
DECISION_DOC_ROOT = ROOT / "docs" / "coordination" / "decisions"
ISSUE_DOC_ROOT = ROOT / "docs" / "issues"


class CoordError(RuntimeError):
    def __init__(self, message: str, code: int = 2):
        super().__init__(message)
        self.code = code


def utc_now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec="milliseconds").replace("+00:00", "Z")


def utc_after(minutes: float) -> str:
    value = datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(
        minutes=minutes)
    return value.isoformat(timespec="milliseconds").replace("+00:00", "Z")


def new_id(prefix: str) -> str:
    return f"{prefix}-{uuid.uuid4().hex[:12]}"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as handle:
            for block in iter(lambda: handle.read(1024 * 1024), b""):
                digest.update(block)
    except OSError:
        return ""
    return digest.hexdigest()


class FileLock:
    """Short-lived cross-platform advisory lock for the audit stream."""

    def __init__(self, path: Path):
        self.path = path
        self.handle = None
        self.platform = "windows" if os.name == "nt" else "posix"

    def __enter__(self) -> "FileLock":
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.handle = self.path.open("a+b")
        self.handle.seek(0, os.SEEK_END)
        if self.handle.tell() == 0:
            self.handle.write(b"\0")
            self.handle.flush()
        self.handle.seek(0)
        for _ in range(100):
            try:
                if self.platform == "windows":
                    import msvcrt
                    msvcrt.locking(self.handle.fileno(), msvcrt.LK_NBLCK, 1)
                else:
                    import fcntl
                    fcntl.flock(self.handle.fileno(),
                                fcntl.LOCK_EX | fcntl.LOCK_NB)
                return self
            except OSError:
                time.sleep(0.02)
        raise CoordError("coordination audit lock is busy", 4)

    def __exit__(self, *_exc) -> None:
        if self.handle is None:
            return
        try:
            self.handle.seek(0)
            if self.platform == "windows":
                import msvcrt
                msvcrt.locking(self.handle.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                import fcntl
                fcntl.flock(self.handle.fileno(), fcntl.LOCK_UN)
        except OSError:
            pass
        finally:
            self.handle.close()
            self.handle = None


class CoordStore:
    def __init__(self, root: Path | None = None, *, publish_docs: bool = True,
                 docs_root: Path | None = None,
                 issue_docs_root: Path | None = None):
        configured = os.environ.get("MIMITA_COORD_ROOT", "").strip()
        candidate = Path(configured) if configured else (root or ROOT / ".dev" / "agent-coord")
        self.root = resolve_within(ROOT, candidate, label="coordination root")
        self.root.mkdir(parents=True, exist_ok=True)
        (self.root / "proposals").mkdir(parents=True, exist_ok=True)
        (self.root / "reviews").mkdir(parents=True, exist_ok=True)
        self.publish_docs = publish_docs
        self.docs_root = docs_root or DECISION_DOC_ROOT
        self.issue_docs_root = issue_docs_root or ISSUE_DOC_ROOT
        self.conn = sqlite3.connect(self.root / "brain.db", timeout=10.0)
        self.conn.row_factory = sqlite3.Row
        self.conn.execute("PRAGMA journal_mode=WAL")
        self.conn.execute("PRAGMA synchronous=NORMAL")
        self._schema()

    def close(self) -> None:
        self.conn.close()

    def _schema(self) -> None:
        self.conn.executescript(
            """
            CREATE TABLE IF NOT EXISTS agents (
                agent TEXT PRIMARY KEY,
                task TEXT NOT NULL DEFAULT '',
                status TEXT NOT NULL DEFAULT 'active',
                pid INTEGER NOT NULL DEFAULT 0,
                session TEXT NOT NULL DEFAULT '',
                started_utc TEXT NOT NULL,
                last_seen_utc TEXT NOT NULL,
                cursor INTEGER NOT NULL DEFAULT 0
            );
            CREATE TABLE IF NOT EXISTS tasks (
                id TEXT PRIMARY KEY,
                title TEXT NOT NULL DEFAULT '',
                state TEXT NOT NULL DEFAULT 'open',
                owner TEXT NOT NULL DEFAULT '',
                method TEXT NOT NULL DEFAULT '',
                created_utc TEXT NOT NULL,
                updated_utc TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS task_deps (
                task_id TEXT NOT NULL,
                before_id TEXT NOT NULL,
                PRIMARY KEY (task_id, before_id)
            );
            CREATE TABLE IF NOT EXISTS directives (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                agent TEXT NOT NULL,
                source TEXT NOT NULL,
                via_agent TEXT NOT NULL DEFAULT '',
                task_id TEXT NOT NULL DEFAULT '',
                text TEXT NOT NULL,
                method TEXT NOT NULL DEFAULT '',
                priority TEXT NOT NULL DEFAULT 'normal',
                status TEXT NOT NULL DEFAULT 'active',
                supersedes INTEGER NOT NULL DEFAULT 0,
                created_utc TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS notes (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                agent TEXT NOT NULL,
                task_id TEXT NOT NULL DEFAULT '',
                body TEXT NOT NULL,
                tags TEXT NOT NULL DEFAULT '',
                created_utc TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                msg_id TEXT NOT NULL UNIQUE,
                thread TEXT NOT NULL,
                sender TEXT NOT NULL,
                recipient TEXT NOT NULL,
                kind TEXT NOT NULL,
                subject TEXT NOT NULL DEFAULT '',
                body TEXT NOT NULL DEFAULT '',
                paths TEXT NOT NULL DEFAULT '[]',
                requires_response INTEGER NOT NULL DEFAULT 0,
                due_utc TEXT NOT NULL DEFAULT '',
                created_utc TEXT NOT NULL,
                read_at TEXT NOT NULL DEFAULT '',
                answered_at TEXT NOT NULL DEFAULT '',
                reply_to TEXT NOT NULL DEFAULT ''
            );
            CREATE TABLE IF NOT EXISTS leases (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                lease_id TEXT NOT NULL UNIQUE,
                path TEXT NOT NULL,
                region TEXT NOT NULL DEFAULT '',
                holder TEXT NOT NULL,
                participants TEXT NOT NULL DEFAULT '[]',
                base_hash TEXT NOT NULL DEFAULT '',
                state TEXT NOT NULL DEFAULT 'active',
                created_utc TEXT NOT NULL,
                expires_utc TEXT NOT NULL,
                released_utc TEXT NOT NULL DEFAULT '',
                released_reason TEXT NOT NULL DEFAULT ''
            );
            CREATE TABLE IF NOT EXISTS shared_files (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                shared_id TEXT NOT NULL UNIQUE,
                path TEXT NOT NULL,
                participants TEXT NOT NULL,
                regions TEXT NOT NULL,
                state TEXT NOT NULL DEFAULT 'pending',
                created_utc TEXT NOT NULL,
                ended_utc TEXT NOT NULL DEFAULT ''
            );
            CREATE TABLE IF NOT EXISTS conflicts (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                conflict_id TEXT NOT NULL UNIQUE,
                task_id TEXT NOT NULL DEFAULT '',
                kind TEXT NOT NULL,
                summary TEXT NOT NULL,
                state TEXT NOT NULL DEFAULT 'open',
                created_utc TEXT NOT NULL,
                resolved_decision TEXT NOT NULL DEFAULT ''
            );
            CREATE TABLE IF NOT EXISTS options (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                option_id TEXT NOT NULL UNIQUE,
                conflict_id TEXT NOT NULL,
                title TEXT NOT NULL,
                tradeoffs TEXT NOT NULL DEFAULT '',
                patch TEXT NOT NULL DEFAULT '',
                created_utc TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS votes (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                conflict_id TEXT NOT NULL,
                option_id TEXT NOT NULL,
                agent TEXT NOT NULL,
                rationale TEXT NOT NULL DEFAULT '',
                created_utc TEXT NOT NULL,
                UNIQUE(conflict_id, agent)
            );
            CREATE TABLE IF NOT EXISTS decisions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                decision_id TEXT NOT NULL UNIQUE,
                task_id TEXT NOT NULL DEFAULT '',
                conflict_id TEXT NOT NULL DEFAULT '',
                inputs TEXT NOT NULL DEFAULT '[]',
                chosen TEXT NOT NULL,
                rejected TEXT NOT NULL DEFAULT '[]',
                rationale TEXT NOT NULL DEFAULT '',
                tradeoffs TEXT NOT NULL DEFAULT '',
                affected_paths TEXT NOT NULL DEFAULT '[]',
                dissent TEXT NOT NULL DEFAULT '[]',
                status TEXT NOT NULL DEFAULT 'active',
                document TEXT NOT NULL DEFAULT '',
                created_utc TEXT NOT NULL,
                supersedes TEXT NOT NULL DEFAULT ''
            );
            CREATE TABLE IF NOT EXISTS proposals (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                proposal_id TEXT NOT NULL UNIQUE,
                path TEXT NOT NULL,
                sender TEXT NOT NULL,
                recipient TEXT NOT NULL,
                patch_path TEXT NOT NULL,
                patch_sha TEXT NOT NULL,
                state TEXT NOT NULL DEFAULT 'open',
                created_utc TEXT NOT NULL,
                applied_utc TEXT NOT NULL DEFAULT ''
            );
            CREATE TABLE IF NOT EXISTS freezes (
                path TEXT PRIMARY KEY,
                reason TEXT NOT NULL DEFAULT '',
                created_utc TEXT NOT NULL,
                released_utc TEXT NOT NULL DEFAULT ''
            );
            CREATE TABLE IF NOT EXISTS halt (
                id INTEGER PRIMARY KEY CHECK (id = 1),
                active INTEGER NOT NULL DEFAULT 0,
                reason TEXT NOT NULL DEFAULT '',
                by_agent TEXT NOT NULL DEFAULT '',
                created_utc TEXT NOT NULL,
                released_utc TEXT NOT NULL DEFAULT ''
            );
            CREATE TABLE IF NOT EXISTS events (
                seq INTEGER PRIMARY KEY AUTOINCREMENT,
                event TEXT NOT NULL,
                agent TEXT NOT NULL DEFAULT '',
                fields TEXT NOT NULL DEFAULT '{}',
                created_utc TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS user_context (
                context_id TEXT PRIMARY KEY,
                quote TEXT NOT NULL,
                quote_hash TEXT NOT NULL,
                source TEXT NOT NULL DEFAULT 'user',
                source_agent TEXT NOT NULL DEFAULT '',
                source_session TEXT NOT NULL DEFAULT '',
                source_ref TEXT NOT NULL DEFAULT '',
                priority TEXT NOT NULL DEFAULT 'normal',
                tags TEXT NOT NULL DEFAULT '[]',
                status TEXT NOT NULL DEFAULT 'new',
                canonical_issue_id TEXT NOT NULL DEFAULT '',
                created_utc TEXT NOT NULL,
                updated_utc TEXT NOT NULL
            );
            CREATE UNIQUE INDEX IF NOT EXISTS user_context_source_ref
                ON user_context(source_ref) WHERE source_ref<>'';
            CREATE TABLE IF NOT EXISTS user_context_links (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                context_id TEXT NOT NULL,
                issue_id TEXT NOT NULL,
                relation TEXT NOT NULL DEFAULT 'related',
                linked_by TEXT NOT NULL DEFAULT '',
                reason TEXT NOT NULL DEFAULT '',
                active INTEGER NOT NULL DEFAULT 1,
                created_utc TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS user_context_acks (
                context_id TEXT NOT NULL,
                agent TEXT NOT NULL,
                read_utc TEXT NOT NULL,
                PRIMARY KEY(context_id, agent)
            );
            CREATE TABLE IF NOT EXISTS user_context_corrections (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                context_id TEXT NOT NULL,
                from_issue TEXT NOT NULL DEFAULT '',
                to_issue TEXT NOT NULL DEFAULT '',
                agent TEXT NOT NULL,
                reason TEXT NOT NULL,
                created_utc TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS issues (
                id TEXT PRIMARY KEY,
                title TEXT NOT NULL,
                kind TEXT NOT NULL DEFAULT 'bug',
                state TEXT NOT NULL DEFAULT 'new',
                severity TEXT NOT NULL DEFAULT 'normal',
                priority TEXT NOT NULL DEFAULT 'normal',
                source TEXT NOT NULL DEFAULT 'agent',
                reporter TEXT NOT NULL DEFAULT '',
                via_agent TEXT NOT NULL DEFAULT '',
                owner TEXT NOT NULL DEFAULT '',
                reviewers TEXT NOT NULL DEFAULT '[]',
                human_quote TEXT NOT NULL DEFAULT '',
                summary TEXT NOT NULL DEFAULT '',
                doc_path TEXT NOT NULL DEFAULT '',
                evidence TEXT NOT NULL DEFAULT '{}',
                clarification TEXT NOT NULL DEFAULT '',
                deferred_reason TEXT NOT NULL DEFAULT '',
                deferred_until TEXT NOT NULL DEFAULT '',
                resolution TEXT NOT NULL DEFAULT '',
                created_utc TEXT NOT NULL,
                updated_utc TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS issue_tasks (
                issue_id TEXT NOT NULL,
                task_id TEXT NOT NULL,
                step_order INTEGER NOT NULL DEFAULT 0,
                PRIMARY KEY (issue_id, task_id)
            );
            CREATE TABLE IF NOT EXISTS task_proofs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                proof_id TEXT NOT NULL UNIQUE,
                task_id TEXT NOT NULL,
                agent TEXT NOT NULL,
                status TEXT NOT NULL DEFAULT 'unverified',
                build_id TEXT NOT NULL DEFAULT '',
                game_generation INTEGER NOT NULL DEFAULT 0,
                run_id TEXT NOT NULL DEFAULT '',
                journal_path TEXT NOT NULL DEFAULT '',
                event TEXT NOT NULL DEFAULT '',
                field_predicate TEXT NOT NULL DEFAULT '',
                paths TEXT NOT NULL DEFAULT '[]',
                source_hashes TEXT NOT NULL DEFAULT '{}',
                result TEXT NOT NULL DEFAULT 'unproven',
                reason TEXT NOT NULL DEFAULT '',
                verified_utc TEXT NOT NULL DEFAULT '',
                created_utc TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS issue_quotes (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                issue_id TEXT NOT NULL,
                agent TEXT NOT NULL,
                quote TEXT NOT NULL,
                supports TEXT NOT NULL DEFAULT '',
                created_utc TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS issue_reviews (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                issue_id TEXT NOT NULL,
                agent TEXT NOT NULL,
                verdict TEXT NOT NULL,
                body TEXT NOT NULL DEFAULT '',
                created_utc TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS issue_evidence (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                evidence_id TEXT NOT NULL UNIQUE,
                issue_id TEXT NOT NULL,
                task_id TEXT NOT NULL DEFAULT '',
                proof_kind TEXT NOT NULL DEFAULT 'integration',
                build_generation INTEGER NOT NULL DEFAULT 0,
                dll_path TEXT NOT NULL DEFAULT '',
                dll_sha256 TEXT NOT NULL DEFAULT '',
                runtime_id TEXT NOT NULL DEFAULT '',
                run_id TEXT NOT NULL DEFAULT '',
                server_journal TEXT NOT NULL DEFAULT '',
                client_journal TEXT NOT NULL DEFAULT '',
                required_events TEXT NOT NULL DEFAULT '[]',
                matched_events TEXT NOT NULL DEFAULT '[]',
                scenario_passed INTEGER NOT NULL DEFAULT 0,
                result TEXT NOT NULL DEFAULT 'unproven',
                reason TEXT NOT NULL DEFAULT '',
                captured_by TEXT NOT NULL DEFAULT '',
                created_utc TEXT NOT NULL,
                verified_utc TEXT NOT NULL DEFAULT ''
            );
            CREATE TABLE IF NOT EXISTS activity (
                agent TEXT PRIMARY KEY,
                task_id TEXT NOT NULL DEFAULT '',
                issue_id TEXT NOT NULL DEFAULT '',
                phase TEXT NOT NULL DEFAULT '',
                activity TEXT NOT NULL DEFAULT '',
                paths TEXT NOT NULL DEFAULT '[]',
                build_id TEXT NOT NULL DEFAULT '',
                game_generation INTEGER NOT NULL DEFAULT 0,
                run_id TEXT NOT NULL DEFAULT '',
                blocker TEXT NOT NULL DEFAULT '',
                next_step TEXT NOT NULL DEFAULT '',
                wait_reason TEXT NOT NULL DEFAULT '',
                updated_utc TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS build_requests (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                request_id TEXT NOT NULL UNIQUE,
                agent TEXT NOT NULL,
                cohort TEXT NOT NULL DEFAULT '',
                issue_id TEXT NOT NULL DEFAULT '',
                kind TEXT NOT NULL DEFAULT 'integration',
                base_generation INTEGER NOT NULL DEFAULT 0,
                paths TEXT NOT NULL DEFAULT '[]',
                overlay_paths TEXT NOT NULL DEFAULT '[]',
                priority TEXT NOT NULL DEFAULT 'normal',
                status TEXT NOT NULL DEFAULT 'proposed',
                wait_reason TEXT NOT NULL DEFAULT '',
                generation INTEGER NOT NULL DEFAULT 0,
                dll_path TEXT NOT NULL DEFAULT '',
                dll_sha256 TEXT NOT NULL DEFAULT '',
                build_attempt_id TEXT NOT NULL DEFAULT '',
                created_utc TEXT NOT NULL,
                updated_utc TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS runtime_registry (
                runtime_id TEXT PRIMARY KEY,
                agent TEXT NOT NULL DEFAULT '',
                cohort TEXT NOT NULL DEFAULT '',
                issue_id TEXT NOT NULL DEFAULT '',
                generation INTEGER NOT NULL DEFAULT 0,
                dll_path TEXT NOT NULL DEFAULT '',
                dll_sha256 TEXT NOT NULL DEFAULT '',
                run_id TEXT NOT NULL DEFAULT '',
                server_pid INTEGER NOT NULL DEFAULT 0,
                client_pids TEXT NOT NULL DEFAULT '[]',
                server_journal TEXT NOT NULL DEFAULT '',
                client_journals TEXT NOT NULL DEFAULT '[]',
                state TEXT NOT NULL DEFAULT 'starting',
                created_utc TEXT NOT NULL,
                updated_utc TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS build_owner (
                id INTEGER PRIMARY KEY CHECK (id = 1),
                agent TEXT NOT NULL DEFAULT '',
                updated_utc TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS work_sessions (
                session_id TEXT PRIMARY KEY,
                agent TEXT NOT NULL,
                task_id TEXT NOT NULL DEFAULT '',
                issue_id TEXT NOT NULL DEFAULT '',
                received_message_id TEXT NOT NULL DEFAULT '',
                state TEXT NOT NULL DEFAULT 'active',
                phase TEXT NOT NULL DEFAULT 'received',
                resume_phase TEXT NOT NULL DEFAULT '',
                started_utc TEXT NOT NULL,
                ended_utc TEXT NOT NULL DEFAULT '',
                elapsed_ms INTEGER NOT NULL DEFAULT 0,
                notes TEXT NOT NULL DEFAULT ''
            );
            CREATE TABLE IF NOT EXISTS work_phases (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                session_id TEXT NOT NULL,
                name TEXT NOT NULL,
                started_utc TEXT NOT NULL,
                ended_utc TEXT NOT NULL DEFAULT '',
                elapsed_ms INTEGER NOT NULL DEFAULT 0,
                wait_reason TEXT NOT NULL DEFAULT '',
                build_id TEXT NOT NULL DEFAULT '',
                run_id TEXT NOT NULL DEFAULT '',
                notes TEXT NOT NULL DEFAULT '',
                state TEXT NOT NULL DEFAULT 'active'
            );
            """
        )
        self._ensure_task_columns()
        self._ensure_agent_columns()
        self._ensure_work_columns()
        self.conn.execute(
            "INSERT OR IGNORE INTO halt(id, active, reason, by_agent, created_utc) "
            "VALUES (1, 0, '', '', ?)", (utc_now(),))
        self.conn.commit()

    def _ensure_agent_columns(self) -> None:
        existing = {str(row[1]) for row in self.conn.execute(
            "PRAGMA table_info(agents)")}
        additions = {
            "parent": "TEXT NOT NULL DEFAULT ''",
            "cohort": "TEXT NOT NULL DEFAULT ''",
            "capabilities": "TEXT NOT NULL DEFAULT '[]'",
            "budget": "TEXT NOT NULL DEFAULT '{}'",
        }
        with self.conn:
            for name, definition in additions.items():
                if name not in existing:
                    self.conn.execute(
                        f"ALTER TABLE agents ADD COLUMN {name} {definition}")

    def _ensure_task_columns(self) -> None:
        existing = {str(row[1]) for row in self.conn.execute(
            "PRAGMA table_info(tasks)")}
        additions = {
            "issue_id": "TEXT NOT NULL DEFAULT ''",
            "priority": "TEXT NOT NULL DEFAULT 'normal'",
            "acceptance": "TEXT NOT NULL DEFAULT ''",
            "evidence": "TEXT NOT NULL DEFAULT ''",
            "deferred_reason": "TEXT NOT NULL DEFAULT ''",
            "deferred_until": "TEXT NOT NULL DEFAULT ''",
            "proof_exempt_reason": "TEXT NOT NULL DEFAULT ''",
            "proof_exempt_paths": "TEXT NOT NULL DEFAULT '[]'",
        }
        with self.conn:
            for name, definition in additions.items():
                if name not in existing:
                    self.conn.execute(
                        f"ALTER TABLE tasks ADD COLUMN {name} {definition}")
            issue_columns = {str(row[1]) for row in self.conn.execute(
                "PRAGMA table_info(issues)")}
            issue_additions = {
                "proof_exempt_reason": "TEXT NOT NULL DEFAULT ''",
                "proof_exempt_paths": "TEXT NOT NULL DEFAULT '[]'",
                "reproduction": "TEXT NOT NULL DEFAULT ''",
                "expected_behavior": "TEXT NOT NULL DEFAULT ''",
                "acceptance": "TEXT NOT NULL DEFAULT ''",
                "paths": "TEXT NOT NULL DEFAULT '[]'",
                "related_build": "TEXT NOT NULL DEFAULT ''",
                "related_run": "TEXT NOT NULL DEFAULT ''",
                "dependencies": "TEXT NOT NULL DEFAULT '[]'",
                "risk": "TEXT NOT NULL DEFAULT ''",
                "proof_expectation": "TEXT NOT NULL DEFAULT ''",
            }
            for name, definition in issue_additions.items():
                if name not in issue_columns:
                    self.conn.execute(
                        f"ALTER TABLE issues ADD COLUMN {name} {definition}")

    def _ensure_work_columns(self) -> None:
        existing = {str(row[1]) for row in self.conn.execute(
            "PRAGMA table_info(work_sessions)")}
        if "resume_phase" not in existing:
            with self.conn:
                self.conn.execute(
                    "ALTER TABLE work_sessions ADD COLUMN resume_phase "
                    "TEXT NOT NULL DEFAULT ''")

    # ── audit ────────────────────────────────────────────────────────────
    def event(self, name: str, agent: str = "", **fields) -> int:
        created = utc_now()
        payload = json.dumps(fields, ensure_ascii=True, sort_keys=True)
        with self.conn:
            cursor = self.conn.execute(
                "INSERT INTO events(event, agent, fields, created_utc) VALUES (?,?,?,?)",
                (name, agent, payload, created))
            seq = int(cursor.lastrowid or 0)
        record = {
            "category": "AGENT_COORD",
            "event": name,
            "event_id": f"COORD_{seq}",
            "fields": fields,
            "level": EVENT_LEVEL,
            "pid": os.getpid(),
            "process": "agent-coord",
            "process_role": "agent-coord",
            "reason": str(fields.get("reason", "")),
            "run_id": str(fields.get("session", "")),
            "wall_time": created,
            "seq": seq,
        }
        with FileLock(self.root / "coord.lock"):
            with (self.root / "events.jsonl").open("a", encoding="utf-8",
                                                   newline="\n") as handle:
                handle.write(json.dumps(record, separators=(",", ":")) + "\n")
        return seq

    # ── helpers ──────────────────────────────────────────────────────────
    def _agent_row(self, agent: str) -> sqlite3.Row | None:
        return self.conn.execute(
            "SELECT * FROM agents WHERE agent=?", (agent,)).fetchone()

    def require_agent(self, agent: str) -> sqlite3.Row:
        row = self._agent_row(agent)
        if row is None:
            raise CoordError(f"unknown agent {agent!r}; run begin first", 2)
        with self.conn:
            self.conn.execute(
                "UPDATE agents SET last_seen_utc=? WHERE agent=?", (utc_now(), agent))
        return row

    def _resolve_path(self, raw: str, *, allow_generated: bool = False) -> str:
        text = str(raw).strip().replace("\\", "/")
        if not text:
            raise CoordError("path is required", 3)
        path = Path(text)
        if not path.is_absolute():
            path = ROOT / path
        try:
            resolved = resolve_within(ROOT, path, label="coordination path")
            relative = resolved.relative_to(ROOT).as_posix()
        except (RuntimeError, ValueError) as error:
            raise CoordError(f"path escapes repository: {raw}", 3) from error
        if not allow_generated and relative.startswith(PROTECTED_PREFIXES):
            raise CoordError(
                f"coordination path is generated or private: {relative}", 3)
        return relative

    def _absolute(self, relative: str) -> Path:
        return ROOT / relative

    def _file_hash(self, relative: str) -> str:
        return sha256_file(self._absolute(relative))

    def _sweep(self, agent: str) -> None:
        now = utc_now()
        with self.conn:
            rows = self.conn.execute(
                "SELECT * FROM leases WHERE state='active' AND expires_utc<=?",
                (now,)).fetchall()
            for row in rows:
                self.conn.execute(
                    "UPDATE leases SET state='expired', released_utc=?, "
                    "released_reason='expired' WHERE id=?",
                    (now, row["id"]))
                self.event("coord.lease.expired", agent,
                           lease=row["lease_id"], path=row["path"],
                           holder=row["holder"], session=agent)

    def _active_lease(self, path: str) -> sqlite3.Row | None:
        self._sweep("")
        return self.conn.execute(
            "SELECT * FROM leases WHERE path=? AND state='active' "
            "ORDER BY id DESC LIMIT 1", (path,)).fetchone()

    def _shared_for_path(self, path: str) -> sqlite3.Row | None:
        return self.conn.execute(
            "SELECT * FROM shared_files WHERE path=? AND state='accepted' "
            "ORDER BY id DESC LIMIT 1", (path,)).fetchone()

    def _halted(self) -> sqlite3.Row | None:
        row = self.conn.execute("SELECT * FROM halt WHERE id=1").fetchone()
        return row if row and int(row["active"]) else None

    def _frozen(self, path: str) -> sqlite3.Row | None:
        rows = self.conn.execute(
            "SELECT * FROM freezes WHERE released_utc='' ORDER BY path").fetchall()
        for row in rows:
            frozen = str(row["path"])
            if path == frozen or path.startswith(frozen.rstrip("/") + "/"):
                return row
        return None

    def _ensure_writable(self, path: str) -> None:
        halted = self._halted()
        if halted:
            raise CoordError(
                f"coordination is halted: {halted['reason'] or 'no reason'}", 2)
        frozen = self._frozen(path)
        if frozen:
            raise CoordError(
                f"path is frozen: {path} ({frozen['reason'] or 'no reason'})", 2)

    # ── agents and sync ──────────────────────────────────────────────────
    def ensure_agent(self, agent: str, task: str = "system") -> dict:
        if self._agent_row(agent) is None:
            return self.begin(agent, task)
        with self.conn:
            self.conn.execute(
                "UPDATE agents SET last_seen_utc=? WHERE agent=?",
                (utc_now(), agent))
        return {"agent": agent, "existing": True}

    def begin(self, agent: str, task: str, *, parent: str = "",
              cohort: str = "", capabilities: list[str] | None = None) -> dict:
        if not agent.strip():
            raise CoordError("agent name is required", 2)
        now = utc_now()
        session = new_id("session")
        with self.conn:
            self.conn.execute(
                "INSERT INTO agents(agent, task, status, pid, session, started_utc, "
                "last_seen_utc, cursor, parent, cohort, capabilities) "
                "VALUES (?,?,'active',?,?,?,?,0,?,?,?) "
                "ON CONFLICT(agent) DO UPDATE SET task=excluded.task, status='active', "
                "pid=excluded.pid, session=excluded.session, "
                "last_seen_utc=excluded.last_seen_utc, parent=excluded.parent, "
                "cohort=excluded.cohort, capabilities=excluded.capabilities",
                (agent, task, os.getpid(), session, now, now, parent, cohort,
                 json.dumps(capabilities or [])))
        with self.conn:
            self.conn.execute(
                "INSERT INTO activity(agent,task_id,phase,activity,updated_utc) "
                "VALUES (?,?,?,?,?) ON CONFLICT(agent) DO UPDATE SET "
                "phase=excluded.phase, activity=excluded.activity, "
                "updated_utc=excluded.updated_utc",
                (agent, "", "starting", "session started", now))
        self.event("coord.agent.registered", agent, task=task, session=session)
        work = self._active_work_session(agent)
        if work is None:
            work = self.work_start(agent, notes=f"activation: {task}")
        return {"agent": agent, "task": task, "session": session,
                "work_session": str(work["session_id"])}

    def end(self, agent: str) -> dict:
        self.require_agent(agent)
        work = self._active_work_session(agent)
        if work is not None:
            self.work_end(agent, str(work["session_id"]), notes="agent end")
        now = utc_now()
        with self.conn:
            leases = self.conn.execute(
                "SELECT lease_id, path FROM leases WHERE holder=? AND state='active'",
                (agent,)).fetchall()
            self.conn.execute(
                "UPDATE leases SET state='released', released_utc=?, "
                "released_reason='session_end' WHERE holder=? AND state='active'",
                (now, agent))
            self.conn.execute(
                "UPDATE agents SET status='inactive', last_seen_utc=? WHERE agent=?",
                (now, agent))
            self.conn.execute(
                "UPDATE activity SET phase='inactive', activity='session ended', "
                "blocker='', wait_reason='', updated_utc=? WHERE agent=?",
                (now, agent))
        for lease in leases:
            self.event("coord.lease.released", agent, lease=lease["lease_id"],
                       path=lease["path"], reason="session_end", session=agent)
        self.event("coord.session.ended", agent, session=agent)
        return {"agent": agent, "released": len(leases)}

    def sync(self, agent: str, *, mark_read: bool = True) -> dict:
        self.require_agent(agent)
        self._sweep(agent)
        unread = self.conn.execute(
            "SELECT * FROM messages WHERE read_at='' AND "
            "(recipient=? OR recipient IN ('all','any')) ORDER BY id",
            (agent,)).fetchall()
        if mark_read and unread:
            now = utc_now()
            with self.conn:
                for row in unread:
                    self.conn.execute(
                        "UPDATE messages SET read_at=? WHERE id=?", (now, row["id"]))
        row = self._agent_row(agent)
        cursor = int(row["cursor"]) if row else 0
        events = self.conn.execute(
            "SELECT seq, event, agent, fields, created_utc FROM events "
            "WHERE seq>? ORDER BY seq", (cursor,)).fetchall()
        if events:
            with self.conn:
                self.conn.execute(
                    "UPDATE agents SET cursor=? WHERE agent=?",
                    (int(events[-1]["seq"]), agent))
        unread_context = self.user_context_list(
            agent, limit=20, unread_only=True)
        latest_context = self.user_context_list(limit=10)
        if mark_read and unread_context:
            self.user_context_ack(agent)
        return {
            "agent": agent,
            "messages": [dict(item) for item in unread],
            "events": [
                {"seq": item["seq"], "event": item["event"],
                 "agent": item["agent"],
                 "fields": json.loads(item["fields"]), "created_utc": item["created_utc"]}
                for item in events
            ],
            "leases": [dict(item) for item in self.conn.execute(
                "SELECT * FROM leases WHERE state='active' ORDER BY path")],
            "shared": [dict(item) for item in self.conn.execute(
                "SELECT * FROM shared_files WHERE state='accepted' ORDER BY path")],
            "conflicts": [dict(item) for item in self.conn.execute(
                "SELECT * FROM conflicts WHERE state='open' ORDER BY id")],
            "decisions": [dict(item) for item in self.conn.execute(
                "SELECT * FROM decisions WHERE status IN ('active','stale') "
                "ORDER BY id DESC LIMIT 10")],
            "halt": dict(self._halted()) if self._halted() else None,
            "freezes": [dict(item) for item in self.conn.execute(
                "SELECT * FROM freezes WHERE released_utc='' ORDER BY path")],
            "activity": [dict(item) for item in self.conn.execute(
                "SELECT * FROM activity ORDER BY agent")],
            "issues": [dict(item) for item in self.conn.execute(
                "SELECT id,title,kind,state,severity,priority,owner,summary,"
                "deferred_reason,deferred_until,updated_utc FROM issues "
                "WHERE state NOT IN ('done','cancelled') ORDER BY updated_utc DESC")],
            "user_context": unread_context,
            "user_context_latest": latest_context,
            "brain_check": self.brain_check(agent),
        }

    def brain_check(self, agent: str) -> dict:
        """Return a bounded issue snapshot after every agent message send."""
        self.require_agent(agent)
        activity = self.conn.execute(
            "SELECT issue_id FROM activity WHERE agent=?", (agent,)).fetchone()
        current_issue_id = str(activity["issue_id"] or "") if activity else ""
        current_issue = None
        if current_issue_id:
            row = self.conn.execute(
                "SELECT id,title,kind,state,priority,owner,summary FROM issues "
                "WHERE id=?", (current_issue_id,)).fetchone()
            current_issue = dict(row) if row else None
        claimable = [dict(row) for row in self.conn.execute(
            "SELECT id,title,kind,state,priority,summary FROM issues "
            "WHERE owner='' AND state NOT IN "
            "('done','cancelled','deferred','needs_clarification','blocked') "
            "ORDER BY CASE priority WHEN 'high' THEN 0 WHEN 'normal' THEN 1 "
            "WHEN 'low' THEN 2 ELSE 1 END, updated_utc LIMIT 5")]
        work = self.next_work(agent, claim=False)
        next_issue = work.get("issue")
        latest_context = self.user_context_list(limit=5)
        unread_context = self.user_context_list(
            agent, limit=5, unread_only=True)
        return {
            "current_issue": current_issue,
            "latest_user_context": latest_context,
            "unread_user_context": unread_context,
            "next_work": ({
                "issue": {key: next_issue.get(key) for key in
                          ("id", "title", "kind", "state", "priority", "owner")},
                "task": work.get("task"),
                "next_step": work.get("next_step"),
            } if next_issue else None),
            "claimable_issues": claimable,
            "instructions": (
                "Claim work only when taking responsibility with `next --claim`. "
                "A message that names one existing issue is linked automatically; "
                "pass `--issue <id>` when it is not named or when multiple issues "
                "are discussed. Use `issue suggest` for a separate proposal on an issue."
            ),
        }

    # ── messages ─────────────────────────────────────────────────────────
    def send(self, sender: str, recipient: str, subject: str, body: str,
             *, kind: str = "message", paths: list[str] | None = None,
             requires_response: bool = False, due_minutes: float = 0,
             reply_to: str = "", thread: str = "",
             issue_id: str = "") -> dict:
        self.require_agent(sender)
        issue_id = issue_id.strip()
        if not issue_id:
            candidates = set(re.findall(
                r"\bissue-[A-Za-z0-9_-]+\b", f"{subject}\n{body}", re.IGNORECASE))
            existing = [candidate for candidate in candidates
                        if self.conn.execute(
                            "SELECT 1 FROM issues WHERE id=?", (candidate,)
                        ).fetchone()]
            if len(existing) == 1:
                issue_id = existing[0]
        if issue_id:
            self._issue_row(issue_id)
            if not body.strip():
                raise CoordError("issue-linked message body is required", 2)
        brain = self.brain_check(sender)
        now = utc_now()
        msg_id = new_id("msg")
        thread_id = thread or new_id("thread")
        if reply_to:
            parent = self.conn.execute(
                "SELECT thread FROM messages WHERE msg_id=?", (reply_to,)).fetchone()
            if parent:
                thread_id = parent["thread"]
        due = utc_after(due_minutes) if due_minutes > 0 else ""
        with self.conn:
            self.conn.execute(
                "INSERT INTO messages(msg_id, thread, sender, recipient, kind, "
                "subject, body, paths, requires_response, due_utc, created_utc, "
                "reply_to) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)",
                (msg_id, thread_id, sender, recipient, kind, subject, body,
                 json.dumps(paths or []), 1 if requires_response else 0,
                 due, now, reply_to))
        self.event("coord.message.sent", sender, message=msg_id, thread=thread_id,
                   recipient=recipient, kind=kind, session=sender)
        result = {"message": msg_id, "thread": thread_id, "due_utc": due,
                  "brain_check": brain}
        if issue_id:
            self.issue_discussion(sender, issue_id, body, message_id=msg_id,
                                  subject=subject, kind=kind)
            result["issue_discussion_appended"] = issue_id
        return result

    def reply(self, agent: str, message_id: str, body: str,
              issue_id: str = "") -> dict:
        parent = self.conn.execute(
            "SELECT * FROM messages WHERE msg_id=?", (message_id,)).fetchone()
        if parent is None:
            raise CoordError(f"unknown message {message_id}", 2)
        result = self.send(agent, parent["sender"], "re: " + parent["subject"],
                           body, kind="answer", reply_to=message_id,
                           thread=parent["thread"], issue_id=issue_id)
        now = utc_now()
        with self.conn:
            self.conn.execute(
                "UPDATE messages SET answered_at=?, read_at=CASE WHEN read_at='' "
                "THEN ? ELSE read_at END WHERE msg_id=?",
                (now, now, message_id))
        self.event("coord.message.answered", agent, message=message_id,
                   answer=result["message"], session=agent)
        return result

    def wait_for_answer(self, agent: str, message_id: str,
                        timeout_seconds: float) -> dict | None:
        deadline = time.monotonic() + max(0.0, timeout_seconds)
        while time.monotonic() < deadline:
            row = self.conn.execute(
                "SELECT * FROM messages WHERE msg_id=?", (message_id,)).fetchone()
            if row is None:
                return None
            if row["answered_at"]:
                answer = self.conn.execute(
                    "SELECT * FROM messages WHERE reply_to=? ORDER BY id DESC LIMIT 1",
                    (message_id,)).fetchone()
                return dict(answer) if answer else dict(row)
            time.sleep(0.25)
        return None

    def inbox(self, agent: str, *, unread_only: bool = True) -> list[dict]:
        self.require_agent(agent)
        where = "read_at=''" if unread_only else "1=1"
        rows = self.conn.execute(
            f"SELECT * FROM messages WHERE {where} AND "
            "(recipient=? OR recipient IN ('all','any')) ORDER BY id",
            (agent,)).fetchall()
        if rows and unread_only:
            now = utc_now()
            with self.conn:
                for row in rows:
                    self.conn.execute(
                        "UPDATE messages SET read_at=? WHERE id=?", (now, row["id"]))
        return [dict(item) for item in rows]

    # ── tasks, directives, notes ─────────────────────────────────────────
    def task_add(self, agent: str, task_id: str, title: str = "",
                 owner: str = "", *, issue_id: str = "",
                 priority: str = "normal", acceptance: str = "") -> dict:
        self.require_agent(agent)
        now = utc_now()
        with self.conn:
            self.conn.execute(
                "INSERT INTO tasks(id,title,state,owner,method,issue_id,priority,"
                "acceptance,created_utc,updated_utc) "
                "VALUES (?,?, 'open', ?, '', ?, ?, ?, ?, ?) ON CONFLICT(id) DO UPDATE SET "
                "title=excluded.title, owner=CASE WHEN excluded.owner<>'' THEN "
                "excluded.owner ELSE tasks.owner END, issue_id=CASE WHEN "
                "excluded.issue_id<>'' THEN excluded.issue_id ELSE tasks.issue_id END, "
                "priority=excluded.priority, updated_utc=excluded.updated_utc",
                (task_id, title, owner or agent, issue_id, priority, acceptance,
                 now, now))
            if issue_id:
                self.conn.execute(
                    "INSERT OR IGNORE INTO issue_tasks(issue_id,task_id,step_order) "
                    "VALUES (?,?,0)", (issue_id, task_id))
        self.event("coord.task.changed", agent, task=task_id, change="add",
                   owner=owner or agent, issue=issue_id)
        return {"task": task_id, "owner": owner or agent, "issue": issue_id}

    def task_owner(self, agent: str, task_id: str, owner: str) -> dict:
        self.require_agent(agent)
        with self.conn:
            self.conn.execute(
                "UPDATE tasks SET owner=?, updated_utc=? WHERE id=?",
                (owner, utc_now(), task_id))
        self.event("coord.task.changed", agent, task=task_id, change="owner",
                   owner=owner)
        return {"task": task_id, "owner": owner}

    def task_dep(self, agent: str, task_id: str, before_id: str) -> dict:
        self.require_agent(agent)
        if task_id == before_id or self._dep_reaches(before_id, task_id):
            raise CoordError(f"dependency cycle: {task_id} -> {before_id}", 2)
        with self.conn:
            self.conn.execute(
                "INSERT OR IGNORE INTO task_deps(task_id, before_id) VALUES (?,?)",
                (task_id, before_id))
        self.event("coord.task.changed", agent, task=task_id, change="dependency",
                   before=before_id)
        return {"task": task_id, "before": before_id}

    def _dep_reaches(self, start: str, target: str) -> bool:
        seen: set[str] = set()
        stack = [start]
        while stack:
            current = stack.pop()
            if current == target:
                return True
            if current in seen:
                continue
            seen.add(current)
            stack.extend(row["before_id"] for row in self.conn.execute(
                "SELECT before_id FROM task_deps WHERE task_id=?", (current,)))
        return False

    def task_method(self, agent: str, task_id: str, method: str) -> dict:
        self.require_agent(agent)
        with self.conn:
            self.conn.execute(
                "UPDATE tasks SET method=?, updated_utc=? WHERE id=?",
                (method, utc_now(), task_id))
        self.event("coord.task.changed", agent, task=task_id, change="method",
                   method=method)
        return {"task": task_id, "method": method}

    def _proof_path_hash(self, relative: str) -> str:
        path = Path(relative)
        if not path.is_absolute():
            path = ROOT / path
        return sha256_file(path)

    def proof_add(self, agent: str, task_id: str, *, paths=None,
                  status: str = "runtime_proven", build_id: str = "",
                  game_generation: int = 0, run_id: str = "",
                  journal_path: str = "", event: str = "",
                  field_predicate: str = "") -> dict:
        self.require_agent(agent)
        proof_id = new_id("task-proof")
        path_list = [str(item) for item in (paths or [])]
        hashes = {path: self._proof_path_hash(path) for path in path_list}
        with self.conn:
            self.conn.execute(
                "INSERT INTO task_proofs(proof_id,task_id,agent,status,build_id,"
                "game_generation,run_id,journal_path,event,field_predicate,paths,"
                "source_hashes,created_utc) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)",
                (proof_id, task_id, agent, status, build_id,
                 int(game_generation or 0), run_id, journal_path, event,
                 field_predicate, json.dumps(path_list), json.dumps(hashes),
                 utc_now()))
        self.event("coord.task-proof.added", agent, proof=proof_id,
                   task=task_id, build=build_id,
                   generation=game_generation, run=run_id, session=agent)
        return {"id": proof_id, "task": task_id, "result": "unverified"}

    def proof_verify(self, agent: str, proof_id: str) -> dict:
        self.require_agent(agent)
        row = self.conn.execute(
            "SELECT * FROM task_proofs WHERE proof_id=?", (proof_id,)).fetchone()
        if row is None:
            raise CoordError(f"unknown task proof {proof_id!r}", 2)
        reasons: list[str] = []
        matched = False
        if row["journal_path"]:
            try:
                journal = self._artifact_path(str(row["journal_path"]))
                text = journal.read_text(encoding="utf-8", errors="replace")
                for line in text.splitlines():
                    try:
                        value = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    if row["event"] and value.get("event") != row["event"]:
                        continue
                    fields = value.get("fields") or {}
                    if row["field_predicate"]:
                        key, _, expected = str(row["field_predicate"]).partition("=")
                        actual = fields.get(key)
                        if str(actual) != expected:
                            continue
                    matched = True
                    break
                if not matched:
                    reasons.append("journal event predicate not found")
            except (OSError, RuntimeError) as error:
                reasons.append(f"journal unavailable: {error}")
        elif row["event"]:
            reasons.append("journal path missing")
        result = "proven" if not reasons else "unproven"
        with self.conn:
            self.conn.execute(
                "UPDATE task_proofs SET result=?, reason=?, verified_utc=? "
                "WHERE proof_id=?",
                (result, "; ".join(reasons), utc_now(), proof_id))
        self.event("coord.task-proof.verified", agent, proof=proof_id,
                   task=row["task_id"], result=result,
                   reason="; ".join(reasons), session=agent)
        return {"id": proof_id, "task": row["task_id"], "result": result,
                "reason": "; ".join(reasons)}

    def proof_status(self) -> dict:
        stale = 0
        proven = 0
        exempt = 0
        rows = []
        for row in self.conn.execute("SELECT * FROM task_proofs ORDER BY id"):
            item = dict(row)
            if item.get("result") == "exempt":
                exempt += 1
            if item.get("result") == "proven":
                hashes = json.loads(item.get("source_hashes") or "{}")
                changed = [path for path, expected in hashes.items()
                           if self._proof_path_hash(path) != expected]
                if changed:
                    stale += 1
                    item["result"] = "stale"
                    item["reason"] = "source changed: " + ", ".join(changed)
                    with self.conn:
                        self.conn.execute(
                            "UPDATE task_proofs SET result='stale', reason=? "
                            "WHERE proof_id=?", (item["reason"], item["id"]))
                else:
                    proven += 1
            rows.append(item)
        return {"stale": stale, "proven": proven, "exempt": exempt,
                "proofs": rows}

    def proof_exempt(self, agent: str, task_id: str, reason: str,
                     paths=None) -> dict:
        self.require_agent(agent)
        reason = reason.strip()
        if not reason:
            raise CoordError("proof exemption requires a reason", 2)
        path_list = [str(item) for item in (paths or [])]
        hashes = {path: self._proof_path_hash(path) for path in path_list}
        proof_id = new_id("task-proof")
        now = utc_now()
        with self.conn:
            self.conn.execute(
                "UPDATE tasks SET proof_exempt_reason=?, proof_exempt_paths=?, "
                "updated_utc=? WHERE id=?",
                (reason, json.dumps(path_list), now, task_id))
            self.conn.execute(
                "INSERT INTO task_proofs(proof_id,task_id,agent,status,result,"
                "paths,source_hashes,reason,verified_utc,created_utc) "
                "VALUES (?,?,?,'exempt','exempt',?,?,?,?,?)",
                (proof_id, task_id, agent, json.dumps(path_list),
                 json.dumps(hashes), reason, now, now))
        self.event("coord.task-proof.exempt", agent, task=task_id,
                   proof=proof_id, reason=reason, session=agent)
        return {"task": task_id, "exempt": True, "reason": reason,
                "id": proof_id}

    def task_status(self, agent: str, task_id: str, state: str) -> dict:
        self.require_agent(agent)
        if state == "done":
            task = self.conn.execute(
                "SELECT * FROM tasks WHERE id=?", (task_id,)).fetchone()
            exempt = bool(task and task["proof_exempt_reason"]) if task else False
            proven = self.conn.execute(
                "SELECT 1 FROM task_proofs WHERE task_id=? AND result='proven' "
                "LIMIT 1", (task_id,)).fetchone()
            if not exempt and proven is None:
                raise CoordError(
                    "task done requires verified proof or an explicit exemption",
                    5)
        with self.conn:
            self.conn.execute(
                "UPDATE tasks SET state=?, updated_utc=? WHERE id=?",
                (state, utc_now(), task_id))
        self.event("coord.task.changed", agent, task=task_id, change="state",
                   state=state)
        return {"task": task_id, "state": state}

    def directive_add(self, agent: str, text: str, *, task_id: str = "",
                      method: str = "", source: str = "agent",
                      via_agent: str = "", priority: str = "normal") -> dict:
        self.require_agent(agent)
        now = utc_now()
        with self.conn:
            cursor = self.conn.execute(
                "INSERT INTO directives(agent,source,via_agent,task_id,text,method,"
                "priority,created_utc) VALUES (?,?,?,?,?,?,?,?)",
                (agent, source, via_agent or agent, task_id, text, method,
                 priority, now))
            directive_id = int(cursor.lastrowid or 0)
        self.event("coord.directive.recorded", agent, directive=directive_id,
                   task=task_id, method=method, source=source, session=agent)
        self._mark_decisions_stale(agent, task_id, directive_id)
        self._detect_method_conflict(agent, task_id)
        return {"directive": directive_id}

    def _mark_decisions_stale(self, agent: str, task_id: str,
                              directive_id: int) -> None:
        if not task_id:
            return
        rows = self.conn.execute(
            "SELECT decision_id FROM decisions WHERE task_id=? AND status='active'",
            (task_id,)).fetchall()
        for row in rows:
            with self.conn:
                self.conn.execute(
                    "UPDATE decisions SET status='stale' WHERE decision_id=?",
                    (row["decision_id"],))
            self.event("coord.decision.stale", agent,
                       decision=row["decision_id"], task=task_id,
                       directive=directive_id)

    def _detect_method_conflict(self, agent: str, task_id: str) -> None:
        if not task_id:
            return
        rows = self.conn.execute(
            "SELECT id, method FROM directives WHERE task_id=? AND status='active' "
            "AND method<>''", (task_id,)).fetchall()
        methods = {row["method"] for row in rows}
        if len(methods) < 2:
            return
        existing = self.conn.execute(
            "SELECT conflict_id FROM conflicts WHERE task_id=? AND kind='method' "
            "AND state='open'", (task_id,)).fetchone()
        if existing:
            return
        conflict_id = new_id("conflict")
        with self.conn:
            self.conn.execute(
                "INSERT INTO conflicts(conflict_id,task_id,kind,summary,created_utc) "
                "VALUES (?,?,?,?,?)",
                (conflict_id, task_id, "method",
                 "conflicting task methods: " + ", ".join(sorted(methods)),
                 utc_now()))
        self.event("coord.conflict.opened", agent, conflict=conflict_id,
                   task=task_id, kind="method", options=sorted(methods))

    def note_add(self, agent: str, body: str, *, task_id: str = "",
                 tags: str = "") -> dict:
        self.require_agent(agent)
        with self.conn:
            cursor = self.conn.execute(
                "INSERT INTO notes(agent,task_id,body,tags,created_utc) "
                "VALUES (?,?,?,?,?)", (agent, task_id, body, tags, utc_now()))
            note_id = int(cursor.lastrowid or 0)
        self.event("coord.note.appended", agent, note=note_id, task=task_id)
        return {"note": note_id}

    def note_read(self, since: int = 0, task_id: str = "",
                  limit: int = 50) -> list[dict]:
        query = "SELECT * FROM notes WHERE id>?"
        args: list[object] = [since]
        if task_id:
            query += " AND task_id=?"
            args.append(task_id)
        query += " ORDER BY id LIMIT ?"
        args.append(limit)
        return [dict(row) for row in self.conn.execute(query, args)]

    def note_search(self, text: str, limit: int = 50) -> list[dict]:
        return [dict(row) for row in self.conn.execute(
            "SELECT * FROM notes WHERE body LIKE ? ORDER BY id DESC LIMIT ?",
            (f"%{text}%", limit))]

    # ── issues, activity, board ──────────────────────────────────────────
    def _issue_row(self, issue_id: str) -> sqlite3.Row:
        row = self.conn.execute(
            "SELECT * FROM issues WHERE id=?", (issue_id,)).fetchone()
        if row is None:
            raise CoordError(f"unknown issue {issue_id!r}", 2)
        return row

    @staticmethod
    def _slug(text: str, limit: int = 48) -> str:
        value = "".join(ch if ch.isalnum() else "-" for ch in text.lower())
        while "--" in value:
            value = value.replace("--", "-")
        return value.strip("-")[:limit] or "issue"

    def _issue_doc_path(self, issue_id: str, title: str, kind: str,
                        created_utc: str) -> Path:
        stamp = created_utc[:10].replace("-", "")
        domain = self._slug(kind, 24)
        return (self.issue_docs_root / domain /
                f"{stamp}-{self._slug(title)}-{issue_id}.md")

    def _doc_display_path(self, path: Path) -> str:
        try:
            return path.resolve().relative_to(ROOT).as_posix()
        except ValueError:
            return str(path.resolve())

    def _append_issue_doc(self, issue_id: str, section: str,
                          body: str) -> str:
        if not self.publish_docs:
            return ""
        row = self._issue_row(issue_id)
        raw_path = str(row["doc_path"] or "").strip()
        path = Path(raw_path) if raw_path else self._issue_doc_path(
            issue_id, str(row["title"]), str(row["kind"]),
            str(row["created_utc"]))
        if not path.is_absolute():
            path = ROOT / path
        path.parent.mkdir(parents=True, exist_ok=True)
        append = path.is_file()
        with path.open("a", encoding="utf-8", newline="\n") as handle:
            if not append:
                handle.write(f"# {row['id']}: {row['title']}\n\n")
                handle.write("## Human report (verbatim, immutable)\n\n")
                quote = str(row["human_quote"] or "").splitlines() or [""]
                for line in quote:
                    handle.write(f"> {line}\n")
                handle.write("\n")
            if section:
                handle.write(f"\n## {section}\n\n{body.rstrip()}\n")
        display = self._doc_display_path(path)
        if str(row["doc_path"] or "") != display:
            with self.conn:
                self.conn.execute(
                    "UPDATE issues SET doc_path=?, updated_utc=? WHERE id=?",
                    (display, utc_now(), issue_id))
        return display

    def _write_issue_index(self) -> None:
        if not self.publish_docs:
            return
        root = self.issue_docs_root
        root.mkdir(parents=True, exist_ok=True)
        rows = self.conn.execute(
            "SELECT * FROM issues ORDER BY "
            "CASE state WHEN 'needs_clarification' THEN 0 WHEN 'new' THEN 1 "
            "WHEN 'triaged' THEN 2 WHEN 'assigned' THEN 3 WHEN 'active' THEN 4 "
            "WHEN 'in_review' THEN 5 WHEN 'deferred' THEN 6 ELSE 7 END, updated_utc DESC"
        ).fetchall()
        lines = [
            "# Issues",
            "",
            "Generated from `.dev/agent-coord/brain.db`. Do not hand-edit the table.",
            "Detail files are append-only; the original human report is preserved verbatim.",
            "",
            "| ID | State | Kind | Severity | Owner | Summary | Detail |",
            "|---|---|---|---|---|---|---|",
        ]
        for row in rows:
            doc = str(row["doc_path"] or "")
            if doc:
                path = Path(doc)
                if path.is_absolute():
                    try:
                        doc = path.resolve().relative_to(root.resolve()).as_posix()
                    except ValueError:
                        pass
                elif doc.startswith("docs/issues/"):
                    doc = doc[len("docs/issues/"):]
            summary = str(row["summary"] or row["title"] or "").replace("|", "\\|")
            lines.append(
                f"| {row['id']} | {row['state']} | {row['kind']} | "
                f"{row['severity']} | {row['owner'] or '-'} | {summary} | "
                f"{doc or '-'} |")
        (root / "README.md").write_text("\n".join(lines) + "\n",
                                        encoding="utf-8", newline="\n")

    def issue_add(self, agent: str, title: str, *, quote: str = "",
                  summary: str = "", kind: str = "bug", source: str = "agent",
                  via_agent: str = "", severity: str = "normal",
                  priority: str = "normal", owner: str = "",
                  evidence: object = None, issue_id: str = "",
                  reproduction: str = "", expected_behavior: str = "",
                  acceptance: str = "", paths: list[str] | None = None,
                  dependencies: list[str] | None = None, risk: str = "",
                  proof_expectation: str = "", context_id: str = "") -> dict:
        self.require_agent(agent)
        title = " ".join(str(title).split())
        paths = [str(item) for item in (paths or []) if str(item)]
        dependencies = [str(item) for item in (dependencies or []) if str(item)]
        evidence_value = evidence if isinstance(evidence, dict) else {}
        required = {
            "title": bool(title),
            "quote": bool(str(quote or "").strip()),
            "summary": bool(str(summary or "").strip()),
            "reproduction": bool(str(reproduction or "").strip()),
            "expected_behavior": bool(str(expected_behavior or "").strip()),
            "acceptance": bool(str(acceptance or "").strip()),
            "paths": bool(paths),
            "evidence": bool(evidence_value),
            "proof_expectation": bool(str(proof_expectation or "").strip()),
        }
        missing = [name for name, present in required.items() if not present]
        if missing:
            raise CoordError(
                "issue missing required fields: " + ", ".join(missing) +
                "; use devscripts/agent-issue.py or fill them with issue context",
                2)
        issue_id = issue_id.strip() or new_id("issue")
        now = utc_now()
        payload = json.dumps(evidence_value, sort_keys=True)
        with self.conn:
            self.conn.execute(
                "INSERT INTO issues(id,title,kind,state,severity,priority,source,"
                "reporter,via_agent,owner,human_quote,summary,evidence,"
                "reproduction,expected_behavior,acceptance,paths,dependencies,"
                "risk,proof_expectation,created_utc,updated_utc) "
                "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                (issue_id, title, kind, "new", severity, priority, source,
                 agent, via_agent or agent, owner, quote, summary, payload,
                 reproduction, expected_behavior, acceptance,
                 json.dumps(paths), json.dumps(dependencies), risk,
                 proof_expectation, now, now))
        self.event("coord.issue.opened", agent, issue=issue_id, title=title,
                   kind=kind, source=source, severity=severity,
                   priority=priority, paths=paths,
                   quote_hash=hashlib.sha256(
                       str(quote or "").encode("utf-8")).hexdigest())
        self._append_issue_doc(issue_id, "", "")
        self._write_issue_index()
        if context_id:
            self.user_context_link(agent, context_id, issue_id,
                                   reason="issue intake linked context")
        elif source == "user" and str(quote or "").strip():
            context = self.user_context_add(
                agent, quote, source="user", priority=priority, issue_id=issue_id)
            context_id = str(context["context_id"])
        result = self.issue_show(issue_id)
        result["context_id"] = context_id
        return result

    def issue_show(self, issue_id: str) -> dict:
        row = dict(self._issue_row(issue_id))
        row["tasks"] = [dict(item) for item in self.conn.execute(
            "SELECT t.*, it.step_order FROM issue_tasks it "
            "JOIN tasks t ON t.id=it.task_id WHERE it.issue_id=? "
            "ORDER BY it.step_order, t.id", (issue_id,))]
        row["reviews"] = [dict(item) for item in self.conn.execute(
            "SELECT * FROM issue_reviews WHERE issue_id=? ORDER BY id",
            (issue_id,))]
        row["quotes"] = [dict(item) for item in self.conn.execute(
            "SELECT * FROM issue_quotes WHERE issue_id=? ORDER BY id",
            (issue_id,))]
        row["readiness"] = self.issue_readiness(issue_id)
        occurrences = []
        for item in self.conn.execute(
                "SELECT seq, agent, fields, created_utc FROM events "
                "WHERE event='coord.issue.updated' ORDER BY seq DESC LIMIT 500"):
            try:
                fields = json.loads(item["fields"] or "{}")
            except json.JSONDecodeError:
                continue
            if fields.get("issue") != issue_id or fields.get("change") != "occurrence":
                continue
            occurrences.append({
                "seq": item["seq"], "agent": item["agent"],
                "created_utc": item["created_utc"],
                "body": fields.get("occurrence", ""),
                "evidence": fields.get("evidence", {}),
            })
        row["occurrences"] = list(reversed(occurrences))
        row["user_context"] = self.user_context_for_issue(issue_id)
        return row

    def issue_brief(self, issue_id: str) -> dict:
        issue = self.issue_show(issue_id)
        try:
            paths = [str(item) for item in json.loads(issue.get("paths") or "[]")]
        except json.JSONDecodeError:
            paths = []
        tasks = []
        for task in issue.get("tasks", []):
            proof = self.conn.execute(
                "SELECT * FROM task_proofs WHERE task_id=? "
                "ORDER BY id DESC LIMIT 1", (task["id"],)).fetchone()
            tasks.append({**task, "latest_proof": dict(proof) if proof else None})
        task_ids = [task["id"] for task in tasks]
        leases = []
        if paths:
            placeholders = ",".join("?" for _ in paths)
            leases = [dict(row) for row in self.conn.execute(
                "SELECT * FROM leases WHERE state='active' AND path IN (" +
                placeholders + ") ORDER BY path", paths)]
        builds = [dict(row) for row in self.conn.execute(
            "SELECT * FROM build_requests WHERE issue_id=? ORDER BY id DESC",
            (issue_id,))]
        runtimes = [dict(row) for row in self.conn.execute(
            "SELECT * FROM runtime_registry WHERE issue_id=? "
            "ORDER BY updated_utc DESC", (issue_id,))]
        decisions = []
        if task_ids:
            placeholders = ",".join("?" for _ in task_ids)
            decisions = [dict(row) for row in self.conn.execute(
                "SELECT * FROM decisions WHERE task_id IN (" + placeholders +
                ") ORDER BY id", task_ids)]
        blockers = [{"task": task["id"], "state": task.get("state"),
                     "owner": task.get("owner")}
                    for task in tasks if task.get("state") == "blocked"]
        next_task = next(
            (task for task in tasks
             if task.get("state") != "done" and
             (not task.get("latest_proof") or
              task["latest_proof"].get("result") != "proven")),
            None)
        causality = []
        for item in self.conn.execute(
                "SELECT seq, event, agent, fields, created_utc FROM events "
                "ORDER BY seq DESC LIMIT 500"):
            try:
                fields = json.loads(item["fields"] or "{}")
            except json.JSONDecodeError:
                continue
            if issue_id in json.dumps(fields, sort_keys=True):
                causality.append({"seq": item["seq"], "event": item["event"],
                                  "agent": item["agent"],
                                  "fields": fields,
                                  "created_utc": item["created_utc"]})
            if len(causality) >= 20:
                break
        missing = [name for name in ("title", "summary", "source")
                   if not issue.get(name)]
        return {
            "issue": issue,
            "paths": paths,
            "tasks": tasks,
            "leases": leases,
            "builds": builds,
            "runs": runtimes,
            "decisions": decisions,
            "blockers": blockers,
            "next": ({"action": "claim_task", "task": next_task["id"]}
                     if next_task else
                     {"action": "claim_issue" if issue.get("state") == "new"
                      else "wait", "reason": issue.get("deferred_reason") or ""}),
            "causality": list(reversed(causality)),
            "missing": missing,
        }

    def issue_list(self, *, state: str = "", kind: str = "",
                   owner: str = "", limit: int = 100) -> list[dict]:
        clauses = []
        args: list[object] = []
        if state:
            clauses.append("state=?")
            args.append(state)
        if kind:
            clauses.append("kind=?")
            args.append(kind)
        if owner:
            clauses.append("owner=?")
            args.append(owner)
        where = (" WHERE " + " AND ".join(clauses)) if clauses else ""
        args.append(max(1, int(limit)))
        return [dict(row) for row in self.conn.execute(
            "SELECT * FROM issues" + where +
            " ORDER BY updated_utc DESC LIMIT ?", args)]

    def _issue_append(self, agent: str, issue_id: str, section: str,
                      body: str, **fields) -> dict:
        self._issue_row(issue_id)
        self._append_issue_doc(issue_id, section,
                               f"### {utc_now()} — {agent}\n\n{body}")
        with self.conn:
            self.conn.execute(
                "UPDATE issues SET updated_utc=? WHERE id=?",
                (utc_now(), issue_id))
        self._write_issue_index()
        self.event("coord.issue.updated", agent, issue=issue_id,
                   change=section.lower().replace(" ", "_"), **fields)
        return self.issue_show(issue_id)

    def issue_triage(self, agent: str, issue_id: str, *, summary: str = "",
                     owner: str = "", priority: str = "") -> dict:
        self.require_agent(agent)
        self._issue_row(issue_id)
        with self.conn:
            self.conn.execute(
                "UPDATE issues SET state='triaged', summary=CASE WHEN ?<>'' "
                "THEN ? ELSE summary END, owner=CASE WHEN ?<>'' THEN ? ELSE owner "
                "END, priority=CASE WHEN ?<>'' THEN ? ELSE priority END, "
                "updated_utc=? WHERE id=?",
                (summary, summary, owner, owner, priority, priority,
                 utc_now(), issue_id))
        return self._issue_append(
            agent, issue_id, "Triage",
            f"Summary: {summary or '(unchanged)'}\n"
            f"Owner: {owner or '(unchanged)'}\n"
            f"Priority: {priority or '(unchanged)'}")

    def issue_assign(self, agent: str, issue_id: str, owner: str) -> dict:
        self.require_agent(agent)
        if not owner.strip():
            raise CoordError("owner is required", 2)
        with self.conn:
            self.conn.execute(
                "UPDATE issues SET owner=?, state=CASE WHEN state='new' OR "
                "state='triaged' THEN 'assigned' ELSE state END, updated_utc=? "
                "WHERE id=?", (owner, utc_now(), issue_id))
        return self._issue_append(agent, issue_id, "Assignment",
                                  f"Owner: {owner}")

    def issue_defer(self, agent: str, issue_id: str, reason: str,
                    until: str = "") -> dict:
        self.require_agent(agent)
        with self.conn:
            self.conn.execute(
                "UPDATE issues SET state='deferred', deferred_reason=?, "
                "deferred_until=?, updated_utc=? WHERE id=?",
                (reason, until, utc_now(), issue_id))
        return self._issue_append(
            agent, issue_id, "Deferred",
            f"Reason: {reason or '(not provided)'}\n"
            f"Until: {until or '(not set)'}")

    def issue_clarify(self, agent: str, issue_id: str, question: str) -> dict:
        self.require_agent(agent)
        if not question.strip():
            raise CoordError("clarification question is required", 2)
        with self.conn:
            self.conn.execute(
                "UPDATE issues SET state='needs_clarification', clarification=?, "
                "updated_utc=? WHERE id=?", (question, utc_now(), issue_id))
        return self._issue_append(
            agent, issue_id, "Clarification requested",
            f"Question for the user:\n\n{question}")

    def _artifact_path(self, raw: str) -> Path:
        path = Path(raw)
        if not path.is_absolute():
            path = ROOT / path
        return resolve_within(ROOT, path, label="evidence path")

    def evidence_record(self, agent: str, issue_id: str, *,
                        proof_kind: str = "integration",
                        build_generation: int = 0,
                        runtime_id: str = "", run_id: str = "",
                        server_journal: str = "", client_journal: str = "",
                        required_events: list[str] | None = None,
                        dll_path: str = "") -> dict:
        self.require_agent(agent)
        self._issue_row(issue_id)
        evidence_id = new_id("evidence")
        required = [str(item) for item in (required_events or []) if str(item)]
        derived_dll = dll_path
        if not derived_dll and build_generation > 0:
            derived_dll = str(
                ROOT / "build" / "game" / f"generation-{build_generation}"
                / f"game-g{build_generation}.dll")
        with self.conn:
            self.conn.execute(
                "INSERT INTO issue_evidence(evidence_id,issue_id,proof_kind,"
                "build_generation,dll_path,runtime_id,run_id,server_journal,"
                "client_journal,required_events,captured_by,created_utc) "
                "VALUES (?,?,?,?,?,?,?,?,?,?,?,?)",
                (evidence_id, issue_id, proof_kind, int(build_generation),
                 derived_dll, runtime_id, run_id, server_journal, client_journal,
                 json.dumps(required), agent, utc_now()))
        self.event("coord.evidence.recorded", agent, evidence=evidence_id,
                   issue=issue_id, kind=proof_kind, generation=build_generation,
                   session=agent)
        return self.evidence_verify(agent, evidence_id)

    def evidence_verify(self, agent: str, evidence_id: str) -> dict:
        self.require_agent(agent)
        row = self.conn.execute(
            "SELECT * FROM issue_evidence WHERE evidence_id=?",
            (evidence_id,)).fetchone()
        if row is None:
            raise CoordError(f"unknown evidence {evidence_id!r}", 2)
        required = json.loads(row["required_events"] or "[]")
        matched: list[str] = []
        scenario_passed = False
        reasons: list[str] = []
        dll_sha = ""
        dll = Path(row["dll_path"]) if row["dll_path"] else None
        if dll is not None:
            try:
                dll = self._artifact_path(str(dll))
                if dll.is_file():
                    dll_sha = sha256_file(dll)
                else:
                    reasons.append("DLL missing")
            except (OSError, RuntimeError) as error:
                reasons.append(f"DLL path invalid: {error}")
        if int(row["build_generation"] or 0) > 0:
            generation = int(row["build_generation"])
            generation_file = (ROOT / "build" / "game"
                               / f"generation-{generation}" / "generation.json")
            if not generation_file.is_file():
                reasons.append("build generation missing")
        if row["proof_kind"] == "integration":
            if not required:
                reasons.append("integration proof requires events")
        elif row["proof_kind"] != "unit":
            reasons.append("unknown proof kind")
        for journal_value in (row["server_journal"], row["client_journal"]):
            if not journal_value:
                continue
            try:
                journal = self._artifact_path(str(journal_value))
            except (OSError, RuntimeError) as error:
                reasons.append(f"journal path invalid: {error}")
                continue
            if not journal.is_file():
                reasons.append(f"journal missing: {journal_value}")
                continue
            try:
                for line in journal.read_text(
                        encoding="utf-8", errors="replace").splitlines():
                    try:
                        event = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    name = str(event.get("event") or "")
                    if name in required and name not in matched:
                        matched.append(name)
                    if name == "scenario.finished":
                        fields = event.get("fields") or {}
                        scenario_passed = bool(fields.get("passed"))
            except OSError as error:
                reasons.append(f"journal read failed: {error}")
        if "scenario.finished" in required and not scenario_passed:
            reasons.append("required scenario.finished did not pass")
        missing = [name for name in required if name not in matched]
        if missing:
            reasons.append("missing events: " + ", ".join(missing))
        result = "proven" if not reasons else "unproven"
        with self.conn:
            self.conn.execute(
                "UPDATE issue_evidence SET dll_sha256=?, matched_events=?, "
                "scenario_passed=?, result=?, reason=?, verified_utc=? "
                "WHERE evidence_id=?",
                (dll_sha, json.dumps(matched), 1 if scenario_passed else 0,
                 result, "; ".join(reasons), utc_now(), evidence_id))
            self.conn.execute(
                "UPDATE issues SET evidence=?, updated_utc=? WHERE id=?",
                (json.dumps({"evidence": evidence_id, "result": result,
                             "reason": "; ".join(reasons)}),
                 utc_now(), row["issue_id"]))
        self.event("coord.evidence.verified", agent, evidence=evidence_id,
                   issue=row["issue_id"], result=result,
                   reason="; ".join(reasons), generation=row["build_generation"],
                   session=agent)
        return {
            "evidence": evidence_id, "issue": row["issue_id"],
            "result": result, "reason": "; ".join(reasons),
            "matched_events": matched, "required_events": required,
            "scenario_passed": scenario_passed, "dll_sha256": dll_sha,
        }

    def evidence_list(self, issue_id: str = "") -> list[dict]:
        if issue_id:
            rows = self.conn.execute(
                "SELECT * FROM issue_evidence WHERE issue_id=? ORDER BY id",
                (issue_id,))
        else:
            rows = self.conn.execute(
                "SELECT * FROM issue_evidence ORDER BY id DESC LIMIT 100")
        return [dict(row) for row in rows]

    def issue_trace(self, issue_id: str) -> dict:
        return {"issue": self.issue_show(issue_id),
                "evidence": self.evidence_list(issue_id)}

    # ── user context bus ────────────────────────────────────────────────
    def _context_row(self, context_id: str) -> sqlite3.Row:
        row = self.conn.execute(
            "SELECT * FROM user_context WHERE context_id=?",
            (context_id,)).fetchone()
        if row is None:
            raise CoordError(f"unknown user context {context_id!r}", 2)
        return row

    def _context_links(self, context_id: str) -> list[dict]:
        return [dict(row) for row in self.conn.execute(
            "SELECT l.*, i.title FROM user_context_links l "
            "LEFT JOIN issues i ON i.id=l.issue_id "
            "WHERE l.context_id=? ORDER BY l.id", (context_id,))]

    def _context_view(self, row: sqlite3.Row) -> dict:
        value = dict(row)
        try:
            value["tags"] = json.loads(value.get("tags") or "[]")
        except json.JSONDecodeError:
            value["tags"] = []
        value["links"] = self._context_links(str(row["context_id"]))
        return value

    def user_context_add(self, agent: str, quote: str, *,
                         source: str = "user", session: str = "",
                         source_ref: str = "", priority: str = "normal",
                         tags: list[str] | None = None,
                         issue_id: str = "") -> dict:
        self.ensure_agent(agent, "user context relay")
        quote = str(quote or "").strip()
        if not quote:
            raise CoordError("user context quote is required", 2)
        digest = hashlib.sha256(quote.encode("utf-8")).hexdigest()
        reference = str(source_ref or f"quote:{digest}")
        now = utc_now()
        existing = self.conn.execute(
            "SELECT * FROM user_context WHERE source_ref=? OR quote_hash=? "
            "ORDER BY created_utc LIMIT 1", (reference, digest)).fetchone()
        created = False
        if existing is None:
            context_id = new_id("ctx")
            with self.conn:
                self.conn.execute(
                    "INSERT INTO user_context(context_id,quote,quote_hash,source,"
                    "source_agent,source_session,source_ref,priority,tags,status,"
                    "canonical_issue_id,created_utc,updated_utc) "
                    "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)",
                    (context_id, quote, digest, source, agent, session,
                     reference, priority, json.dumps(tags or []), "new", "",
                     now, now))
            existing = self._context_row(context_id)
            created = True
            self.event("coord.user_context.added", agent, context=context_id,
                       quote_hash=digest, source=source, priority=priority,
                       session=session, issue=issue_id, tags=tags or [])
            self._broadcast_user_context(agent, context_id, quote, priority)
            if priority == "high":
                self._queue_high_context_wake(agent, context_id, quote)
        context_id = str(existing["context_id"])
        if issue_id:
            self.user_context_link(agent, context_id, issue_id,
                                   reason="created/linked by issue intake")
        result = self._context_view(self._context_row(context_id))
        result["created"] = created
        return result

    def _queue_high_context_wake(self, source_agent: str, context_id: str,
                                 quote: str) -> None:
        if self.root.resolve() != (ROOT / ".dev" / "agent-coord").resolve():
            return
        script = ROOT / "devscripts" / "agent-wake.py"
        if not script.is_file():
            return
        agents = [str(row["agent"]) for row in self.conn.execute(
            "SELECT agent FROM agents WHERE status='active' AND agent<>?",
            (source_agent,))]
        for target in agents[:3]:
            prompt = (f"New high-priority user context {context_id}.\n\n"
                      f"{quote}\n\nRun `agent-coord context list --high "
                      f"--agent {target}` and review its linked issue. "
                      "Acknowledge it after routing or repairing ownership.")
            adapter = "opencode" if shutil.which("opencode") else "codex"
            command = [sys.executable, str(script), "add", "--agent", target,
                       "--when", "manual", "--adapter", adapter,
                       "--prompt", prompt]
            try:
                subprocess.run(command, cwd=ROOT, timeout=8,
                               capture_output=True, check=False)
            except (OSError, subprocess.SubprocessError):
                continue

    def _broadcast_user_context(self, agent: str, context_id: str, quote: str,
                                priority: str) -> None:
        msg_id = new_id("msg")
        thread_id = new_id("thread")
        with self.conn:
            self.conn.execute(
                "INSERT INTO messages(msg_id,thread,sender,recipient,kind,"
                "subject,body,paths,requires_response,due_utc,created_utc,"
                "reply_to) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)",
                (msg_id, thread_id, agent, "all", "user-context",
                 f"user context {context_id}", quote, "[]", 0, "",
                 utc_now(), ""))
        self.event("coord.message.sent", agent, message=msg_id,
                   thread=thread_id, recipient="all", kind="user-context",
                   context=context_id, priority=priority, session=agent)

    def user_context_list(self, agent: str = "", *, limit: int = 20,
                          unread_only: bool = False,
                          high_priority_only: bool = False) -> list[dict]:
        clauses: list[str] = []
        args: list[object] = []
        if unread_only and agent:
            clauses.append(
                "context_id NOT IN (SELECT context_id FROM user_context_acks "
                "WHERE agent=?)")
            args.append(agent)
        if high_priority_only:
            clauses.append("priority='high'")
        where = (" WHERE " + " AND ".join(clauses)) if clauses else ""
        args.append(max(1, int(limit)))
        rows = self.conn.execute(
            "SELECT * FROM user_context" + where +
            " ORDER BY created_utc DESC LIMIT ?", args).fetchall()
        return [self._context_view(row) for row in rows]

    def user_context_ack(self, agent: str, context_id: str = "") -> dict:
        self.require_agent(agent)
        now = utc_now()
        if context_id:
            self._context_row(context_id)
            ids = [context_id]
        else:
            ids = [str(row["context_id"]) for row in self.conn.execute(
                "SELECT context_id FROM user_context WHERE context_id NOT IN "
                "(SELECT context_id FROM user_context_acks WHERE agent=?)",
                (agent,))]
        with self.conn:
            for item in ids:
                self.conn.execute(
                    "INSERT OR REPLACE INTO user_context_acks"
                    "(context_id,agent,read_utc) VALUES (?,?,?)",
                    (item, agent, now))
        return {"agent": agent, "acknowledged": len(ids), "contexts": ids}

    def user_context_link(self, agent: str, context_id: str, issue_id: str,
                          *, relation: str = "primary",
                          reason: str = "") -> dict:
        self.require_agent(agent)
        context = self._context_row(context_id)
        self._issue_row(issue_id)
        if relation not in ("primary", "related"):
            raise CoordError("context relation must be primary or related", 2)
        now = utc_now()
        with self.conn:
            if relation == "primary":
                self.conn.execute(
                    "UPDATE user_context_links SET active=0 "
                    "WHERE context_id=? AND relation='primary' AND active=1",
                    (context_id,))
            self.conn.execute(
                "INSERT INTO user_context_links(context_id,issue_id,relation,"
                "linked_by,reason,active,created_utc) VALUES (?,?,?,?,?,1,?)",
                (context_id, issue_id, relation, agent, reason, now))
            canonical = issue_id if relation == "primary" else str(
                context["canonical_issue_id"] or "")
            self.conn.execute(
                "UPDATE user_context SET status=?, canonical_issue_id=?, "
                "updated_utc=? WHERE context_id=?",
                ("linked", canonical, now, context_id))
        self.event("coord.user_context.linked", agent, context=context_id,
                   issue=issue_id, relation=relation, reason=reason)
        return self._context_view(self._context_row(context_id))

    def user_context_move(self, agent: str, context_id: str, issue_id: str,
                          reason: str) -> dict:
        self.require_agent(agent)
        if not str(reason or "").strip():
            raise CoordError("context move reason is required", 2)
        self._context_row(context_id)
        self._issue_row(issue_id)
        previous = self.conn.execute(
            "SELECT issue_id FROM user_context_links WHERE context_id=? "
            "AND relation='primary' AND active=1 ORDER BY id DESC LIMIT 1",
            (context_id,)).fetchone()
        from_issue = str(previous["issue_id"] if previous else "")
        now = utc_now()
        with self.conn:
            self.conn.execute(
                "UPDATE user_context_links SET active=0 WHERE context_id=? "
                "AND relation='primary' AND active=1", (context_id,))
            self.conn.execute(
                "INSERT INTO user_context_links(context_id,issue_id,relation,"
                "linked_by,reason,active,created_utc) VALUES (?,?,?,?,?,1,?)",
                (context_id, issue_id, "primary", agent, reason, now))
            self.conn.execute(
                "INSERT INTO user_context_corrections(context_id,from_issue,"
                "to_issue,agent,reason,created_utc) VALUES (?,?,?,?,?,?)",
                (context_id, from_issue, issue_id, agent, reason, now))
            self.conn.execute(
                "UPDATE user_context SET status='linked',canonical_issue_id=?,"
                "updated_utc=? WHERE context_id=?",
                (issue_id, now, context_id))
        self.event("coord.user_context.moved", agent, context=context_id,
                   from_issue=from_issue, to_issue=issue_id, reason=reason)
        return self._context_view(self._context_row(context_id))

    def user_context_lint(self) -> dict:
        rows = [self._context_view(row) for row in self.conn.execute(
            "SELECT * FROM user_context ORDER BY created_utc")]
        unlinked = [row for row in rows
                    if not any(link["relation"] == "primary" and
                               link["active"] for link in row["links"])]
        multiple = []
        stale = []
        for row in rows:
            primary = [link for link in row["links"]
                       if link["relation"] == "primary" and link["active"]]
            if len(primary) > 1:
                multiple.append(row)
            if row["canonical_issue_id"] and not any(
                    link["issue_id"] == row["canonical_issue_id"] and
                    link["active"] for link in row["links"]):
                stale.append(row)
        return {"unlinked": unlinked, "multiple_primary": multiple,
                "stale_canonical": stale, "total": len(rows)}

    def user_context_for_issue(self, issue_id: str) -> list[dict]:
        rows = self.conn.execute(
            "SELECT DISTINCT c.* FROM user_context c JOIN user_context_links l "
            "ON l.context_id=c.context_id WHERE l.issue_id=? AND l.active=1 "
            "ORDER BY c.created_utc", (issue_id,)).fetchall()
        return [self._context_view(row) for row in rows]

    def next_work(self, agent: str, *, claim: bool = False) -> dict:
        self.require_agent(agent)
        priority = ("high", "normal", "low")
        rows = [dict(row) for row in self.conn.execute(
            "SELECT * FROM issues WHERE state NOT IN ('done','cancelled') "
            "AND (owner='' OR owner=?) ORDER BY updated_utc", (agent,))]
        rows.sort(key=lambda row: (
            priority.index(str(row.get("priority") or "normal"))
            if str(row.get("priority") or "normal") in priority else 1,
            0 if row.get("owner") == agent else 1,
            str(row.get("updated_utc") or "")))
        blocked = [row for row in rows
                   if row.get("state") in ("needs_clarification", "deferred")]
        actionable = [row for row in rows
                      if row.get("state") not in ("needs_clarification", "deferred")]
        if not actionable:
            return {"agent": agent, "issue": None,
                    "blocked": blocked, "next_step": ""}
        issue = actionable[0]
        if claim and not issue.get("owner"):
            self.issue_assign(agent, issue["id"], agent)
            issue["owner"] = agent
        tasks = [dict(row) for row in self.conn.execute(
            "SELECT t.* FROM issue_tasks it JOIN tasks t ON t.id=it.task_id "
            "WHERE it.issue_id=? AND t.state NOT IN ('done','cancelled') "
            "ORDER BY it.step_order, t.id", (issue["id"],))]
        task = tasks[0] if tasks else None
        next_step = ""
        if task:
            next_step = str(task.get("acceptance") or "")
            if not next_step:
                next_step = f"Advance task {task.get('id')}: {task.get('title') or ''}".strip()
        if not next_step:
            next_step = str(issue.get("summary") or issue.get("title") or "")
        return {
            "agent": agent,
            "issue": issue,
            "task": task,
            "next_step": next_step,
            "blocked": blocked,
        }

    def waiting_work(self, agent: str) -> dict:
        """Return missing-context, missing-evidence, or stale-proof work.

        This is deliberately read-only: a waiting agent chooses whether to
        claim the work after reviewing it.
        """
        candidates: list[dict] = []
        priority_rank = {"high": 0, "normal": 1, "low": 2}
        for issue in self.issue_list(limit=500):
            if issue.get("state") in {"done", "cancelled",
                                      "needs_clarification", "deferred"}:
                continue
            readiness = self.issue_readiness(str(issue["id"]))
            rank = priority_rank.get(str(issue.get("priority") or "normal"), 1)
            if not readiness.get("ready"):
                missing = ", ".join(readiness.get("missing") or [])
                candidates.append({
                    "kind": "missing_context",
                    "rank": (0, rank, str(issue.get("updated_utc") or "")),
                    "issue": issue,
                    "task": None,
                    "next_step": f"Fill missing issue fields: {missing}",
                })
                continue
            tasks = [dict(row) for row in self.conn.execute(
                "SELECT t.* FROM issue_tasks it JOIN tasks t ON t.id=it.task_id "
                "WHERE it.issue_id=? AND t.state NOT IN ('done','cancelled') "
                "ORDER BY it.step_order, t.id", (issue["id"],))]
            for task in tasks:
                proof = self.conn.execute(
                    "SELECT result, reason, verified_utc FROM task_proofs "
                    "WHERE task_id=? ORDER BY id DESC LIMIT 1",
                    (task["id"],)).fetchone()
                result = str(proof["result"] or "") if proof else ""
                if result not in {"proven", "exempt"}:
                    stale = result == "stale"
                    candidates.append({
                        "kind": "stale_proof" if stale else "missing_evidence",
                        "rank": (1 if stale else 2, rank,
                                 str(issue.get("updated_utc") or "")),
                        "issue": issue,
                        "task": task,
                        "next_step": (
                            "Re-verify stale proof and append the new evidence."
                            if stale else
                            "Produce the missing proof/evidence or a reasoned "
                            "exemption; do not claim completion without it."),
                    })
                    break
        for row in self.conn.execute(
                "SELECT p.*, t.issue_id, t.title, t.owner, t.state AS task_state "
                "FROM task_proofs p JOIN tasks t ON t.id=p.task_id "
                "WHERE p.result='stale' AND t.state NOT IN ('done','cancelled') "
                "ORDER BY p.id DESC LIMIT 50"):
            item = dict(row)
            issue_id = str(item.get("issue_id") or "")
            if not issue_id or any(c["issue"].get("id") == issue_id
                                   for c in candidates):
                continue
            issue_row = self._issue_row(issue_id)
            candidates.append({
                "kind": "stale_proof",
                "rank": (1, priority_rank.get(
                    str(issue_row["priority"] or "normal"), 1),
                    str(issue_row["updated_utc"] or "")),
                "issue": dict(issue_row),
                "task": item,
                "next_step": "Re-verify stale task proof and append new evidence.",
            })
        if not candidates:
            return {"agent": agent, "mode": "waiting", "issue": None,
                    "task": None, "next_step": "",
                    "reason": "no missing-context or missing-evidence work"}
        chosen = min(candidates, key=lambda item: item["rank"])
        return {"agent": agent, "mode": "waiting",
                "work_kind": chosen["kind"],
                "issue": chosen["issue"], "task": chosen["task"],
                "next_step": chosen["next_step"],
                "reason": chosen["kind"]}

    def wake_check(self, agent: str) -> dict:
        high_context = self.user_context_list(
            agent, limit=1, unread_only=True, high_priority_only=True)
        work = self.next_work(agent)
        issue = work.get("issue")
        if high_context:
            return {
                "wake": True,
                "exit_code": 0,
                "agent": agent,
                "reason": "high-priority user context",
                "user_context": high_context[0],
                "issue": issue,
                "task": work.get("task"),
                "next_step": ("Read the new user context, acknowledge it, and "
                              "route or repair its issue ownership."),
                "blocked": work.get("blocked") or [],
            }
        if not issue:
            blocked = work.get("blocked") or []
            return {
                "wake": False,
                "exit_code": 2 if blocked else 10,
                "agent": agent,
                "reason": "blocked issues only" if blocked else "no actionable work",
                "blocked": blocked,
            }
        context = self.observe_issue(issue["id"], agent)
        return {
            "wake": True,
            "exit_code": 0,
            "agent": agent,
            "issue": issue,
            "task": work.get("task"),
            "next_step": work.get("next_step"),
            "blocked": work.get("blocked") or [],
            "context": context,
        }

    def observe_issue(self, issue_id: str, agent: str = "") -> dict:
        issue = self.issue_show(issue_id)
        evidence = self.evidence_list(issue_id)
        activity = [dict(row) for row in self.conn.execute(
            "SELECT * FROM activity WHERE issue_id=? ORDER BY agent",
            (issue_id,))]
        messages = [dict(row) for row in self.conn.execute(
            "SELECT msg_id,thread,sender,recipient,kind,subject,created_utc "
            "FROM messages ORDER BY id DESC LIMIT 15")]
        build = {}
        for path in (ROOT / "build" / "game" / "ready.json",
                     ROOT / "build" / "game" / "current.json"):
            try:
                value = json.loads(path.read_text(encoding="utf-8"))
                if isinstance(value, dict):
                    build[path.name] = value
            except (OSError, json.JSONDecodeError):
                continue
        runtimes = []
        runtime_root = ROOT / ".dev" / "agent-runtimes"
        if runtime_root.is_dir():
            for path in sorted(runtime_root.glob("*.json")):
                try:
                    value = json.loads(path.read_text(encoding="utf-8"))
                except (OSError, json.JSONDecodeError):
                    continue
                if not isinstance(value, dict):
                    continue
                if agent and value.get("agent") not in ("", agent):
                    continue
                runtimes.append(value)
            runtimes = runtimes[-10:]
        return {
            "issue": issue,
            "evidence": evidence,
            "activity": activity,
            "recent_messages": messages,
            "build": build,
            "runtimes": runtimes,
        }

    def issue_close(self, agent: str, issue_id: str, resolution: str,
                    evidence: str = "") -> dict:
        self.require_agent(agent)
        issue = self._issue_row(issue_id)
        proven = self.conn.execute(
            "SELECT * FROM issue_evidence WHERE issue_id=? AND result='proven' "
            "AND proof_kind IN ('integration','unit') ORDER BY id DESC LIMIT 1",
            (issue_id,)).fetchone()
        exempt = str(issue["proof_exempt_reason"] or "").strip()
        if proven is None and not exempt:
            raise CoordError(
                "issue close requires proven integration/unit evidence or an "
                "explicit issue proof exemption with a reason", 5)
        with self.conn:
            self.conn.execute(
                "UPDATE issues SET state='done', resolution=?, deferred_reason='', "
                "deferred_until='', updated_utc=? WHERE id=?",
                (resolution, utc_now(), issue_id))
        if proven is None:
            proof_text = f"Proof exemption: {exempt}"
        else:
            proof_text = (f"Evidence: {evidence or proven['evidence_id']} "
                          f"({proven['proof_kind']}, {proven['result']})")
        return self._issue_append(
            agent, issue_id, "Resolution",
            f"{resolution}\n\n{proof_text}")

    def issue_exempt(self, agent: str, issue_id: str, reason: str,
                     paths=None) -> dict:
        self.require_agent(agent)
        self._issue_row(issue_id)
        reason = reason.strip()
        if not reason:
            raise CoordError("issue proof exemption requires a reason", 2)
        with self.conn:
            self.conn.execute(
                "UPDATE issues SET proof_exempt_reason=?, proof_exempt_paths=?, "
                "updated_utc=? WHERE id=?",
                (reason, json.dumps([str(item) for item in (paths or [])]),
                 utc_now(), issue_id))
        self.event("coord.issue-proof.exempt", agent, issue=issue_id,
                   reason=reason, session=agent)
        return self._issue_append(
            agent, issue_id, "Proof exemption",
            f"Reason: {reason}\n\nPaths: {', '.join(paths or []) or '(not specified)'}")

    def issue_occurrence(self, agent: str, issue_id: str, body: str,
                         evidence: object = None) -> dict:
        self.require_agent(agent)
        return self._issue_append(
            agent, issue_id, "Occurrence",
            f"{body}\n\nEvidence: {json.dumps(evidence or {}, sort_keys=True)}",
            occurrence=body, evidence=evidence or {})

    def issue_discussion(self, agent: str, issue_id: str, body: str, *,
                         message_id: str, subject: str, kind: str) -> dict:
        self.require_agent(agent)
        self._issue_row(issue_id)
        if not body.strip():
            raise CoordError("issue-linked message body is required", 2)
        return self._issue_append(
            agent, issue_id, "Agent discussion and suggestions",
            f"Message: `{message_id}` ({kind})\n\n"
            f"Subject: {subject or '(no subject)'}\n\n{body.strip()}")

    def issue_suggest(self, agent: str, issue_id: str, body: str) -> dict:
        self.require_agent(agent)
        self._issue_row(issue_id)
        if not body.strip():
            raise CoordError("issue suggestion is required", 2)
        return self._issue_append(agent, issue_id, "Agent suggestion", body.strip())

    def issue_quote(self, agent: str, issue_id: str, quote: str,
                    supports: str = "") -> dict:
        self.require_agent(agent)
        self._issue_row(issue_id)
        if not quote.strip():
            raise CoordError("quote is required", 2)
        with self.conn:
            self.conn.execute(
                "INSERT INTO issue_quotes(issue_id,agent,quote,supports,"
                "created_utc) VALUES (?,?,?,?,?)",
                (issue_id, agent, quote, supports, utc_now()))
        return self._issue_append(
            agent, issue_id, "User quote",
            f"Via {agent}; supports {supports or 'unspecified'}:\n\n> {quote}")

    def issue_review(self, agent: str, issue_id: str, verdict: str,
                     body: str = "") -> dict:
        self.require_agent(agent)
        row = self._issue_row(issue_id)
        reviewers = []
        try:
            reviewers = json.loads(row["reviewers"] or "[]")
        except (TypeError, json.JSONDecodeError):
            reviewers = []
        if agent not in reviewers:
            reviewers.append(agent)
        with self.conn:
            self.conn.execute(
                "INSERT INTO issue_reviews(issue_id,agent,verdict,body,created_utc) "
                "VALUES (?,?,?,?,?)", (issue_id, agent, verdict, body, utc_now()))
            self.conn.execute(
                "UPDATE issues SET state='in_review', reviewers=?, updated_utc=? "
                "WHERE id=?", (json.dumps(reviewers), utc_now(), issue_id))
        return self._issue_append(
            agent, issue_id, f"Review ({verdict})",
            f"Reviewer: {agent}\n\n{body or '(no body)'}")

    def issue_link_task(self, agent: str, issue_id: str, task_id: str,
                        step_order: int = 0) -> dict:
        self.require_agent(agent)
        self._issue_row(issue_id)
        task = self.conn.execute(
            "SELECT id FROM tasks WHERE id=?", (task_id,)).fetchone()
        if task is None:
            raise CoordError(f"unknown task {task_id!r}", 2)
        with self.conn:
            self.conn.execute(
                "INSERT OR REPLACE INTO issue_tasks(issue_id,task_id,step_order) "
                "VALUES (?,?,?)", (issue_id, task_id, int(step_order)))
            self.conn.execute(
                "UPDATE tasks SET issue_id=? WHERE id=?", (issue_id, task_id))
        return self._issue_append(agent, issue_id, "Task linked",
                                  f"Task: {task_id} (step {int(step_order)})")

    def issue_context(self, agent: str, issue_id: str, *,
                      reproduction: str | None = None,
                      expected_behavior: str | None = None,
                      acceptance: str | None = None,
                      paths: list[str] | None = None,
                      related_build: str | None = None,
                      related_run: str | None = None,
                      evidence: object | None = None,
                      dependencies: list[str] | None = None,
                      risk: str | None = None,
                      proof_expectation: str | None = None,
                      context_id: str | None = None) -> dict:
        self.require_agent(agent)
        self._issue_row(issue_id)
        updates: dict[str, object] = {}
        fields: dict[str, object] = {}
        if reproduction is not None:
            updates["reproduction"] = reproduction
            fields["reproduction"] = reproduction
        if expected_behavior is not None:
            updates["expected_behavior"] = expected_behavior
            fields["expected_behavior"] = expected_behavior
        if acceptance is not None:
            updates["acceptance"] = acceptance
            fields["acceptance"] = acceptance
        if paths is not None:
            updates["paths"] = json.dumps(paths)
            fields["paths"] = paths
        if related_build is not None:
            updates["related_build"] = related_build
            fields["related_build"] = related_build
        if related_run is not None:
            updates["related_run"] = related_run
            fields["related_run"] = related_run
        if evidence is not None:
            updates["evidence"] = json.dumps(evidence, sort_keys=True)
            fields["evidence"] = evidence
        if dependencies is not None:
            updates["dependencies"] = json.dumps(dependencies)
            fields["dependencies"] = dependencies
        if risk is not None:
            updates["risk"] = risk
            fields["risk"] = risk
        if proof_expectation is not None:
            updates["proof_expectation"] = proof_expectation
            fields["proof_expectation"] = proof_expectation
        if context_id is not None:
            self.user_context_link(agent, context_id, issue_id,
                                   reason="context attached by issue context")
            fields["context_id"] = context_id
        if not updates:
            return self.issue_show(issue_id)
        now = utc_now()
        with self.conn:
            assignments = ", ".join(f"{key}=?" for key in updates)
            self.conn.execute(
                f"UPDATE issues SET {assignments}, updated_utc=? WHERE id=?",
                [*updates.values(), now, issue_id])
        return self._issue_append(
            agent, issue_id, "Context",
            "\n".join(f"{key}: {value}" for key, value in fields.items()))

    def issue_lint(self, limit: int = 200) -> list[dict]:
        rows = self.conn.execute(
            "SELECT id,title,state,owner FROM issues "
            "ORDER BY updated_utc DESC LIMIT ?", (max(1, int(limit)),)).fetchall()
        result = []
        for row in rows:
            readiness = self.issue_readiness(str(row["id"]))
            if not readiness["ready"]:
                result.append({
                    "id": row["id"], "title": row["title"],
                    "state": row["state"], "owner": row["owner"],
                    "missing": readiness["missing"],
                })
        return result

    def issue_readiness(self, issue_id: str) -> dict:
        row = self._issue_row(issue_id)
        quotes = self.conn.execute(
            "SELECT COUNT(*) FROM issue_quotes WHERE issue_id=?",
            (issue_id,)).fetchone()[0]
        tasks = self.conn.execute(
            "SELECT COUNT(*) FROM issue_tasks WHERE issue_id=?",
            (issue_id,)).fetchone()[0]
        paths = json.loads(row["paths"] or "[]")
        evidence = json.loads(row["evidence"] or "{}")
        required = {
            "human_quote": bool(str(row["human_quote"] or "").strip() or quotes),
            "reproduction": bool(str(row["reproduction"] or "").strip()),
            "expected_behavior": bool(str(row["expected_behavior"] or "").strip()),
            "acceptance": bool(str(row["acceptance"] or "").strip()),
            "paths": bool(paths),
            "evidence": bool(evidence) or bool(row["related_build"]) or bool(row["related_run"]),
            "proof_expectation": bool(str(row["proof_expectation"] or "").strip()),
            "risk": bool(str(row["risk"] or "").strip()),
        }
        missing = [name for name, present in required.items() if not present]
        return {
            "issue": issue_id,
            "ready": not missing,
            "score": len(required) - len(missing),
            "possible": len(required),
            "missing": missing,
            "tasks": int(tasks),
            "state": row["state"],
        }

    _WAIT_PHASES = frozenset({"build_wait", "test_wait", "waiting", "blocked"})

    @staticmethod
    def _work_ms(start: str, end: str = "") -> int:
        try:
            finish = (datetime.datetime.fromisoformat(end.replace("Z", "+00:00"))
                      if end else datetime.datetime.now(datetime.timezone.utc))
            begin = datetime.datetime.fromisoformat(
                str(start).replace("Z", "+00:00"))
            return max(0, int((finish - begin).total_seconds() * 1000))
        except (TypeError, ValueError):
            return 0

    def work_start(self, agent: str, *, task_id: str = "", issue_id: str = "",
                   message_id: str = "", notes: str = "") -> dict:
        self.require_agent(agent)
        active = self._active_work_session(agent)
        if active is not None:
            with self.conn:
                self.conn.execute(
                    "UPDATE work_sessions SET "
                    "task_id=CASE WHEN ?<>'' THEN ? ELSE task_id END, "
                    "issue_id=CASE WHEN ?<>'' THEN ? ELSE issue_id END, "
                    "received_message_id=CASE WHEN ?<>'' THEN ? "
                    "ELSE received_message_id END, "
                    "notes=CASE WHEN ?<>'' THEN ? ELSE notes END "
                    "WHERE session_id=?",
                    (task_id, task_id, issue_id, issue_id, message_id,
                     message_id, notes, notes, active["session_id"]))
            return self.work_status(str(active["session_id"]))
        session_id = new_id("work")
        now = utc_now()
        with self.conn:
            self.conn.execute(
                "INSERT INTO work_sessions(session_id,agent,task_id,issue_id,"
                "received_message_id,state,phase,started_utc,notes) "
                "VALUES (?,?,?,?,?,'active','received',?,?)",
                (session_id, agent, task_id, issue_id, message_id, now, notes))
            self.conn.execute(
                "INSERT INTO work_phases(session_id,name,started_utc,state,notes) "
                "VALUES (?,?,?,'active',?)", (session_id, "received", now, notes))
        self.activity_set(
            agent, task_id=task_id, issue_id=issue_id, phase="received",
            activity="work session started", next_step="planning")
        self.event("coord.work.started", agent, session=session_id,
                   task=task_id, issue=issue_id, message=message_id)
        return self.work_status(session_id)

    def _work_session_row(self, session_id: str) -> sqlite3.Row:
        row = self.conn.execute(
            "SELECT * FROM work_sessions WHERE session_id=?", (session_id,)).fetchone()
        if row is None:
            raise CoordError(f"unknown work session {session_id!r}", 2)
        return row

    def _active_work_session(self, agent: str) -> sqlite3.Row | None:
        return self.conn.execute(
            "SELECT * FROM work_sessions WHERE agent=? AND state='active' "
            "ORDER BY started_utc DESC LIMIT 1", (agent,)).fetchone()

    def _close_active_work_phase(self, session_id: str, now: str) -> None:
        phase = self.conn.execute(
            "SELECT * FROM work_phases WHERE session_id=? AND state='active' "
            "ORDER BY id DESC LIMIT 1", (session_id,)).fetchone()
        if phase is None:
            return
        try:
            elapsed = max(0, int((datetime.datetime.fromisoformat(
                now.replace("Z", "+00:00")) - datetime.datetime.fromisoformat(
                str(phase["started_utc"]).replace("Z", "+00:00"))).total_seconds() * 1000))
        except ValueError:
            elapsed = 0
        self.conn.execute(
            "UPDATE work_phases SET ended_utc=?, elapsed_ms=?, state='done' "
            "WHERE id=?", (now, elapsed, phase["id"]))

    def work_phase(self, agent: str, session_id: str, name: str, *,
                   wait_reason: str = "", build_id: str = "",
                   run_id: str = "", notes: str = "") -> dict:
        self.require_agent(agent)
        row = self._work_session_row(session_id)
        if row["agent"] != agent:
            raise CoordError("work session belongs to another agent", 2)
        if row["state"] != "active":
            raise CoordError("work session is not active", 2)
        name = name.strip()
        if not name:
            raise CoordError("work phase name is required", 2)
        old_phase = str(row["phase"] or "")
        resume_phase = str(row["resume_phase"] or "")
        if name in self._WAIT_PHASES and old_phase not in self._WAIT_PHASES:
            resume_phase = old_phase
        elif name not in self._WAIT_PHASES:
            resume_phase = ""
        now = utc_now()
        with self.conn:
            self._close_active_work_phase(session_id, now)
            self.conn.execute(
                "INSERT INTO work_phases(session_id,name,started_utc,state,"
                "wait_reason,build_id,run_id,notes) VALUES (?,?,?,'active',?,?,?,?)",
                (session_id, name, now, wait_reason, build_id, run_id, notes))
            self.conn.execute(
                "UPDATE work_sessions SET phase=?, resume_phase=?, "
                "notes=CASE WHEN ?<>'' THEN ? ELSE notes END WHERE session_id=?",
                (name, resume_phase, notes, notes, session_id))
        self.activity_set(
            agent, task_id=str(row["task_id"]), issue_id=str(row["issue_id"]),
            phase=name, activity=name, wait_reason=wait_reason,
            build_id=build_id, run_id=run_id,
            blocker=wait_reason, next_step=notes)
        self.event("coord.work.phase", agent, session=session_id, phase=name,
                   wait_reason=wait_reason, build=build_id, run=run_id)
        return self.work_status(session_id)

    def work_wait(self, agent: str, phase: str = "build_wait", *,
                  wait_reason: str = "", build_id: str = "",
                  run_id: str = "", notes: str = "",
                  auto_start: bool = True) -> dict:
        self.require_agent(agent)
        if phase not in self._WAIT_PHASES:
            raise CoordError(f"not a wait phase: {phase!r}", 2)
        session = self._active_work_session(agent)
        if session is None and auto_start:
            started = self.work_start(
                agent, message_id="auto:dev-loop",
                notes="automatic wait tracking")
            session = self._work_session_row(started["session_id"])
        if session is None:
            return {"agent": agent, "session_id": "", "phase": phase,
                    "state": "no-active-session"}
        if str(session["phase"]) == phase:
            return self.work_status(str(session["session_id"]))
        return self.work_phase(
            agent, str(session["session_id"]), phase,
            wait_reason=wait_reason, build_id=build_id, run_id=run_id,
            notes=notes or "automatic wait tracking")

    def work_resume(self, agent: str, *, notes: str = "") -> dict:
        self.require_agent(agent)
        session = self._active_work_session(agent)
        if session is None:
            return {"agent": agent, "session_id": "", "state": "no-active-session"}
        phase = str(session["phase"] or "")
        if phase not in self._WAIT_PHASES:
            return self.work_status(str(session["session_id"]))
        target = str(session["resume_phase"] or "editing")
        return self.work_phase(
            agent, str(session["session_id"]), target,
            notes=notes or f"automatic resume after {phase}")

    def work_end(self, agent: str, session_id: str, *, state: str = "done",
                 notes: str = "") -> dict:
        self.require_agent(agent)
        row = self._work_session_row(session_id)
        if row["agent"] != agent:
            raise CoordError("work session belongs to another agent", 2)
        now = utc_now()
        with self.conn:
            self._close_active_work_phase(session_id, now)
            try:
                elapsed = max(0, int((datetime.datetime.fromisoformat(
                    now.replace("Z", "+00:00")) - datetime.datetime.fromisoformat(
                    str(row["started_utc"]).replace("Z", "+00:00"))).total_seconds() * 1000))
            except ValueError:
                elapsed = 0
            self.conn.execute(
                "UPDATE work_sessions SET state=?, phase='done', ended_utc=?, "
                "elapsed_ms=?, notes=CASE WHEN ?<>'' THEN ? ELSE notes END "
                "WHERE session_id=?",
                (state, now, elapsed, notes, notes, session_id))
        self.activity_set(
            agent, task_id=str(row["task_id"]), issue_id=str(row["issue_id"]),
            phase="done", activity="work session ended", next_step=notes)
        self.event("coord.work.ended", agent, session=session_id, state=state,
                   elapsed_ms=elapsed)
        return self.work_status(session_id)

    def work_status(self, session_id: str) -> dict:
        row = dict(self._work_session_row(session_id))
        phases = [dict(item) for item in self.conn.execute(
            "SELECT * FROM work_phases WHERE session_id=? ORDER BY id",
            (session_id,))]
        if row["state"] == "active":
            row["elapsed_ms"] = self._work_ms(str(row["started_utc"]))
            for phase in phases:
                if phase["state"] == "active":
                    phase["elapsed_ms"] = self._work_ms(
                        str(phase["started_utc"]))
        row["phases"] = phases
        return row

    def work_list(self, agent: str = "", *, active_only: bool = False) -> list[dict]:
        clauses = []
        args: list[object] = []
        if agent:
            clauses.append("agent=?")
            args.append(agent)
        if active_only:
            clauses.append("state='active'")
        where = (" WHERE " + " AND ".join(clauses)) if clauses else ""
        rows = []
        for item in self.conn.execute(
                "SELECT * FROM work_sessions" + where +
                " ORDER BY started_utc DESC", args):
            rows.append(self.work_status(str(item["session_id"])))
        return rows

    def work_summary(self, agent: str = "", issue_id: str = "") -> dict:
        clauses = []
        args: list[object] = []
        if agent:
            clauses.append("agent=?")
            args.append(agent)
        if issue_id:
            clauses.append("issue_id=?")
            args.append(issue_id)
        where = (" WHERE " + " AND ".join(clauses)) if clauses else ""
        sessions = [dict(row) for row in self.conn.execute(
            "SELECT * FROM work_sessions" + where, args)]
        for session in sessions:
            session["elapsed_ms"] = self._work_ms(
                str(session["started_utc"]), str(session["ended_utc"] or ""))
        session_ids = [str(row["session_id"]) for row in sessions]
        phase_totals: dict[str, int] = {}
        if session_ids:
            placeholders = ",".join("?" for _ in session_ids)
            for row in self.conn.execute(
                    f"SELECT * FROM work_phases WHERE session_id IN "
                    f"({placeholders})", session_ids):
                name = str(row["name"])
                milliseconds = (int(row["elapsed_ms"] or 0)
                                if row["state"] != "active"
                                else self._work_ms(str(row["started_utc"])))
                phase_totals[name] = phase_totals.get(name, 0) + milliseconds
        return {
            "sessions": len(sessions),
            "active": sum(1 for row in sessions if row["state"] == "active"),
            "total_ms": sum(int(row["elapsed_ms"] or 0) for row in sessions),
            "phases_ms": phase_totals,
            "sessions_detail": sessions,
        }

    def activity_set(self, agent: str, *, task_id: str = "", issue_id: str = "",
                     phase: str = "", activity: str = "",
                     paths: list[str] | None = None, build_id: str = "",
                     game_generation: int = 0, run_id: str = "",
                     blocker: str = "", next_step: str = "",
                     wait_reason: str = "") -> dict:
        self.require_agent(agent)
        payload = json.dumps(paths or [])
        now = utc_now()
        with self.conn:
            self.conn.execute(
                "INSERT INTO activity(agent,task_id,issue_id,phase,activity,paths,"
                "build_id,game_generation,run_id,blocker,next_step,wait_reason,"
                "updated_utc) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?) "
                "ON CONFLICT(agent) DO UPDATE SET task_id=excluded.task_id, "
                "issue_id=excluded.issue_id, phase=excluded.phase, "
                "activity=excluded.activity, paths=excluded.paths, "
                "build_id=excluded.build_id, game_generation=excluded.game_generation, "
                "run_id=excluded.run_id, blocker=excluded.blocker, "
                "next_step=excluded.next_step, wait_reason=excluded.wait_reason, "
                "updated_utc=excluded.updated_utc",
                (agent, task_id, issue_id, phase, activity, payload, build_id,
                 int(game_generation or 0), run_id, blocker, next_step,
                 wait_reason, now))
        self.event("coord.activity.set", agent, phase=phase, task=task_id,
                   issue=issue_id, build=build_id, game_generation=game_generation,
                   run=run_id, wait_reason=wait_reason, session=agent)
        return self.board(agent=agent)

    def build_request(self, agent: str, *, kind: str = "integration",
                      issue_id: str = "", base_generation: int = 0,
                      paths=None, overlay_paths=None,
                      priority: str = "normal", cohort: str = "") -> dict:
        self.require_agent(agent)
        if kind not in ("integration", "preview"):
            raise CoordError("build kind must be integration or preview", 2)
        request_id = new_id("build-request")
        now = utc_now()
        with self.conn:
            self.conn.execute(
                "INSERT INTO build_requests(request_id,agent,cohort,issue_id,kind,"
                "base_generation,paths,overlay_paths,priority,created_utc,"
                "updated_utc) VALUES (?,?,?,?,?,?,?,?,?,?,?)",
                (request_id, agent, cohort, issue_id, kind, int(base_generation),
                 json.dumps([str(item) for item in (paths or [])]),
                 json.dumps([str(item) for item in (overlay_paths or [])]),
                 priority, now, now))
        self.event("coord.build.requested", agent, request=request_id,
                   kind=kind, issue=issue_id, base_generation=base_generation,
                   priority=priority, session=agent)
        return {"request": request_id, "agent": agent, "kind": kind,
                "status": "proposed"}

    def build_owner(self, agent: str = "") -> dict:
        if agent:
            with self.conn:
                self.conn.execute(
                    "INSERT INTO build_owner(id,agent,updated_utc) VALUES (1,?,?) "
                    "ON CONFLICT(id) DO UPDATE SET agent=excluded.agent, "
                    "updated_utc=excluded.updated_utc", (agent, utc_now()))
            self.event("coord.build.owner", agent, owner=agent, session=agent)
        row = self.conn.execute("SELECT * FROM build_owner WHERE id=1").fetchone()
        return dict(row) if row else {"agent": ""}

    def build_list(self, *, status: str = "") -> list[dict]:
        if status:
            rows = self.conn.execute(
                "SELECT * FROM build_requests WHERE status=? ORDER BY id",
                (status,))
        else:
            rows = self.conn.execute(
                "SELECT * FROM build_requests ORDER BY id DESC LIMIT 100")
        return [dict(row) for row in rows]

    def build_update(self, agent: str, request_id: str, status: str,
                     *, generation: int = 0, dll_path: str = "",
                     dll_sha256: str = "", wait_reason: str = "") -> dict:
        self.require_agent(agent)
        if status not in ("approved", "queued", "building", "published",
                          "failed", "rejected", "superseded", "excluded"):
            raise CoordError("invalid build status", 2)
        with self.conn:
            self.conn.execute(
                "UPDATE build_requests SET status=?, generation=COALESCE(NULLIF(?,0),"
                "generation), dll_path=CASE WHEN ?<>'' THEN ? ELSE dll_path END, "
                "dll_sha256=CASE WHEN ?<>'' THEN ? ELSE dll_sha256 END, "
                "wait_reason=?, updated_utc=? WHERE request_id=?",
                (status, int(generation), dll_path, dll_path, dll_sha256,
                 dll_sha256, wait_reason, utc_now(), request_id))
        self.event("coord.build.updated", agent, request=request_id,
                   status=status, generation=generation, wait_reason=wait_reason,
                   session=agent)
        return {"request": request_id, "status": status}

    def build_wait(self, agent: str, request_id: str, reason: str,
                   seconds: float = 0) -> dict:
        self.require_agent(agent)
        with self.conn:
            self.conn.execute(
                "UPDATE build_requests SET wait_reason=?, updated_utc=? "
                "WHERE request_id=?", (reason, utc_now(), request_id))
        self.event("coord.build.wait", agent, request=request_id,
                   reason=reason, seconds=seconds, session=agent)
        return {"request": request_id, "wait_reason": reason,
                "seconds": seconds}

    def runtime_register(self, agent: str, runtime_id: str, *,
                         cohort: str = "", issue_id: str = "",
                         generation: int = 0, dll_path: str = "",
                         dll_sha256: str = "", run_id: str = "",
                         server_pid: int = 0, client_pids=None,
                         server_journal: str = "", client_journals=None,
                         state: str = "starting") -> dict:
        self.require_agent(agent)
        now = utc_now()
        with self.conn:
            self.conn.execute(
                "INSERT INTO runtime_registry(runtime_id,agent,cohort,issue_id,"
                "generation,dll_path,dll_sha256,run_id,server_pid,client_pids,"
                "server_journal,client_journals,state,created_utc,updated_utc) "
                "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
                "ON CONFLICT(runtime_id) DO UPDATE SET agent=excluded.agent, "
                "cohort=excluded.cohort, issue_id=excluded.issue_id, "
                "generation=excluded.generation, dll_path=excluded.dll_path, "
                "dll_sha256=excluded.dll_sha256, run_id=excluded.run_id, "
                "server_pid=excluded.server_pid, client_pids=excluded.client_pids, "
                "server_journal=excluded.server_journal, "
                "client_journals=excluded.client_journals, state=excluded.state, "
                "updated_utc=excluded.updated_utc",
                (runtime_id, agent, cohort, issue_id, int(generation), dll_path,
                 dll_sha256, run_id, int(server_pid),
                 json.dumps(client_pids or []), server_journal,
                 json.dumps(client_journals or []), state, now, now))
        self.event("coord.runtime.registered", agent, runtime=runtime_id,
                   generation=generation, run=run_id, state=state, session=agent)
        return {"runtime": runtime_id, "state": state}

    def runtime_list(self, *, agent: str = "", state: str = "") -> list[dict]:
        clauses = []
        args: list[object] = []
        if agent:
            clauses.append("(agent=? OR cohort=?)")
            args.extend([agent, agent])
        if state:
            clauses.append("state=?")
            args.append(state)
        where = (" WHERE " + " AND ".join(clauses)) if clauses else ""
        return [dict(row) for row in self.conn.execute(
            "SELECT * FROM runtime_registry" + where + " ORDER BY updated_utc DESC",
            args)]

    def runtime_update(self, agent: str, runtime_id: str, state: str) -> dict:
        self.require_agent(agent)
        if state not in ("starting", "running", "stalled", "stopping",
                         "stopped", "failed"):
            raise CoordError("invalid runtime state", 2)
        with self.conn:
            self.conn.execute(
                "UPDATE runtime_registry SET state=?, updated_utc=? "
                "WHERE runtime_id=?", (state, utc_now(), runtime_id))
        self.event("coord.runtime.updated", agent, runtime=runtime_id,
                   state=state, session=agent)
        return {"runtime": runtime_id, "state": state}

    def board(self, *, agent: str = "") -> dict:
        self._sweep(agent)
        activity_rows = {row["agent"]: dict(row)
                         for row in self.conn.execute("SELECT * FROM activity")}
        agents = []
        for row in self.conn.execute(
                "SELECT agent,task,status,pid,last_seen_utc,parent,cohort,capabilities FROM agents "
                "ORDER BY agent"):
            item = dict(row)
            item["activity"] = activity_rows.get(item["agent"], {})
            agents.append(item)
        issues = [dict(row) for row in self.conn.execute(
            "SELECT * FROM issues WHERE state NOT IN ('done','cancelled') "
            "ORDER BY updated_utc DESC")]
        tasks = [dict(row) for row in self.conn.execute(
            "SELECT * FROM tasks WHERE state NOT IN ('done','cancelled') "
            "ORDER BY updated_utc DESC LIMIT 200")]
        return {
            "generated_utc": utc_now(),
            "agents": agents,
            "issues": issues,
            "tasks": tasks,
            "proof": self.proof_status(),
            "build_requests": self.build_list(),
            "build_owner": self.build_owner(),
            "runtimes": self.runtime_list(),
            "work": self.work_list(active_only=True),
            "leases": [dict(row) for row in self.conn.execute(
                "SELECT * FROM leases WHERE state='active' ORDER BY path")],
            "unread": int(self.conn.execute(
                "SELECT COUNT(*) FROM messages WHERE read_at='' AND "
                "(recipient=? OR recipient IN ('all','any'))",
                (agent or "",)).fetchone()[0]) if agent else 0,
        }

    def coordination_snapshot(self, agent: str = "", *,
                              limit_agents: int = 50,
                              limit_messages: int = 80,
                              limit_issues: int = 100,
                              limit_notes: int = 30,
                              limit_decisions: int = 15) -> dict:
        """Bounded read-only data for the dev-loop coordination panel."""
        activity_rows = {row["agent"]: dict(row)
                         for row in self.conn.execute("SELECT * FROM activity")}
        agents = []
        for row in self.conn.execute(
                "SELECT agent,task,status,pid,last_seen_utc,parent,cohort,capabilities FROM agents "
                "ORDER BY agent LIMIT ?", (max(1, int(limit_agents)),)):
            item = dict(row)
            item["activity"] = activity_rows.get(item["agent"], {})
            agents.append(item)
        agents.sort(key=lambda item: (
            0 if item.get("status") == "active" else
            1 if item.get("status") == "stale" else 2,
            str(item.get("agent") or "")))
        issues = [dict(row) for row in self.conn.execute(
            "SELECT * FROM issues WHERE state NOT IN ('done','cancelled') "
            "ORDER BY updated_utc DESC LIMIT ?", (max(1, int(limit_issues)),))]
        tasks = [dict(row) for row in self.conn.execute(
            "SELECT * FROM tasks WHERE state NOT IN ('done','cancelled') "
            "ORDER BY updated_utc DESC LIMIT 200")]
        message_rows = [dict(row) for row in self.conn.execute(
            "SELECT id,msg_id,thread,sender,recipient,kind,subject,body,paths,"
            "requires_response,created_utc,read_at,answered_at,reply_to "
            "FROM messages ORDER BY id DESC LIMIT ?",
            (max(1, int(limit_messages)),))]
        message_rows.reverse()
        for item in message_rows:
            sender = str(item.get("sender") or "")
            item["sender_kind"] = ("human"
                                   if sender in {"human-dashboard", "human"}
                                   else "agent")
        notes = [dict(row) for row in self.conn.execute(
            "SELECT id,agent,task_id,body,tags,created_utc FROM notes "
            "ORDER BY id DESC LIMIT ?", (max(1, int(limit_notes)),))]
        notes.reverse()
        decisions = [dict(row) for row in self.conn.execute(
            "SELECT decision_id,task_id,chosen,rationale,status,created_utc "
            "FROM decisions ORDER BY id DESC LIMIT ?",
            (max(1, int(limit_decisions)),))]
        decisions.reverse()
        leases = [dict(row) for row in self.conn.execute(
            "SELECT * FROM leases WHERE state='active' ORDER BY path")]
        unread = int(self.conn.execute(
            "SELECT COUNT(*) FROM messages WHERE read_at=''").fetchone()[0])
        focus = issues[0] if issues else {}
        return {
            "generated_utc": utc_now(),
            "focus": focus,
            "agents": agents,
            "issues": issues,
            "tasks": tasks,
            "messages": message_rows,
            "notes": notes,
            "decisions": decisions,
            "leases": leases,
            "build_requests": self.build_list(),
            "build_owner": self.build_owner(),
            "runtimes": self.runtime_list(),
            "work": self.work_list(active_only=True),
            "user_context": self.user_context_list(limit=10),
            "unread": unread,
        }

    def send_human(self, recipient: str, subject: str, body: str, *,
                   kind: str = "message", thread: str = "") -> dict:
        """Write a human-dashboard message without registering an AI agent."""
        recipient = str(recipient or "").strip()
        body = str(body or "").strip()
        if not recipient:
            raise CoordError("recipient is required", 2)
        if not body:
            raise CoordError("message body is required", 2)
        if recipient not in {"all", "any"}:
            if self._agent_row(recipient) is None:
                raise CoordError(f"unknown recipient {recipient!r}", 2)
        now = utc_now()
        msg_id = new_id("msg")
        thread_id = thread.strip() or new_id("thread")
        with self.conn:
            self.conn.execute(
                "INSERT INTO messages(msg_id,thread,sender,recipient,kind,"
                "subject,body,paths,requires_response,due_utc,created_utc,"
                "reply_to) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)",
                (msg_id, thread_id, "human-dashboard", recipient, kind,
                 str(subject or "").strip(), body, "[]", 0, "", now, ""))
        self.event("coord.message.sent", "human-dashboard", message=msg_id,
                   thread=thread_id, recipient=recipient, kind=kind,
                   human=True)
        return {"message": msg_id, "thread": thread_id,
                "recipient": recipient, "sender": "human-dashboard",
                "created_utc": now}

    # ── conflicts, options, votes, decisions ─────────────────────────────
    def conflict_list(self, *, open_only: bool = True) -> list[dict]:
        where = "WHERE state='open'" if open_only else ""
        return [dict(row) for row in self.conn.execute(
            f"SELECT * FROM conflicts {where} ORDER BY id")]

    def option_add(self, agent: str, conflict_id: str, title: str,
                   tradeoffs: str = "", patch: str = "") -> dict:
        self.require_agent(agent)
        option_id = new_id("option")
        with self.conn:
            self.conn.execute(
                "INSERT INTO options(option_id,conflict_id,title,tradeoffs,patch,"
                "created_utc) VALUES (?,?,?,?,?,?)",
                (option_id, conflict_id, title, tradeoffs, patch, utc_now()))
        self.event("coord.option.added", agent, conflict=conflict_id,
                   option=option_id, title=title)
        return {"option": option_id}

    def vote(self, agent: str, conflict_id: str, option_id: str,
             rationale: str = "") -> dict:
        self.require_agent(agent)
        with self.conn:
            self.conn.execute(
                "INSERT INTO votes(conflict_id,option_id,agent,rationale,created_utc) "
                "VALUES (?,?,?,?,?) ON CONFLICT(conflict_id,agent) DO UPDATE SET "
                "option_id=excluded.option_id, rationale=excluded.rationale, "
                "created_utc=excluded.created_utc",
                (conflict_id, option_id, agent, rationale, utc_now()))
        self.event("coord.vote.cast", agent, conflict=conflict_id,
                   option=option_id, rationale=rationale)
        return {"conflict": conflict_id, "option": option_id}

    def decision_record(self, agent: str, task_id: str, chosen: str, *,
                        conflict_id: str = "", rationale: str = "",
                        tradeoffs: str = "", rejected: list[str] | None = None,
                        affected_paths: list[str] | None = None,
                        dissent: list[str] | None = None,
                        supersedes: str = "") -> dict:
        self.require_agent(agent)
        task = self.conn.execute("SELECT * FROM tasks WHERE id=?", (task_id,)).fetchone()
        if task and task["owner"] and task["owner"] != agent:
            owner = self._agent_row(task["owner"])
            if owner and owner["status"] == "active":
                raise CoordError(
                    f"task {task_id} is owned by {task['owner']}; transfer ownership first",
                    2)
        inputs = [row["id"] for row in self.conn.execute(
            "SELECT id FROM directives WHERE task_id=? AND status='active'",
            (task_id,))]
        decision_id = new_id("decision")
        now = utc_now()
        document = self._write_decision_doc(
            decision_id, agent, task_id, conflict_id, inputs, chosen,
            rationale, tradeoffs, rejected or [], affected_paths or [],
            dissent or [])
        with self.conn:
            self.conn.execute(
                "INSERT INTO decisions(decision_id,task_id,conflict_id,inputs,chosen,"
                "rejected,rationale,tradeoffs,affected_paths,dissent,status,document,"
                "created_utc,supersedes) VALUES (?,?,?,?,?,?,?,?,?,?, 'active',?,?,?)",
                (decision_id, task_id, conflict_id, json.dumps(inputs), chosen,
                 json.dumps(rejected or []), rationale, tradeoffs,
                 json.dumps(affected_paths or []), json.dumps(dissent or []),
                 document, now, supersedes))
            self.conn.execute(
                "UPDATE decisions SET status='superseded' WHERE task_id=? AND "
                "decision_id<>? AND status IN ('active','stale')", (task_id, decision_id))
            if conflict_id:
                self.conn.execute(
                    "UPDATE conflicts SET state='resolved', resolved_decision=? "
                    "WHERE conflict_id=?", (decision_id, conflict_id))
        self.event("coord.decision.recorded", agent, decision=decision_id,
                   task=task_id, conflict=conflict_id, document=document,
                   chosen=chosen, session=agent)
        self._write_review(conflict_id) if conflict_id else None
        return {"decision": decision_id, "task": task_id, "document": document}

    def decisions(self, limit: int = 20) -> list[dict]:
        return [dict(row) for row in self.conn.execute(
            "SELECT * FROM decisions ORDER BY id DESC LIMIT ?", (limit,))]

    def _write_decision_doc(self, decision_id: str, agent: str, task_id: str,
                            conflict_id: str, inputs: list[int], chosen: str,
                            rationale: str, tradeoffs: str,
                            rejected: list[str], affected_paths: list[str],
                            dissent: list[str]) -> str:
        if not self.publish_docs:
            return ""
        self.docs_root.mkdir(parents=True, exist_ok=True)
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%d_%H%M%S")
        path = self.docs_root / f"{stamp}-{task_id or 'task'}-{decision_id}.md"
        directive_rows = self.conn.execute(
            "SELECT * FROM directives WHERE task_id=? AND status='active' "
            "ORDER BY id", (task_id,)).fetchall()
        lines = [
            f"# Coordination decision: {task_id or '(no task)'}",
            "",
            f"- Decision: `{decision_id}`",
            f"- Made by: `{agent}`",
            f"- Created UTC: `{utc_now()}`",
            f"- Conflict: `{conflict_id or '-'}`",
            f"- Inputs: `{', '.join(str(value) for value in inputs) or '-'}`",
            "",
            "## Chosen approach",
            "",
            chosen,
            "",
            "## Rationale",
            "",
            rationale or "-",
            "",
            "## Tradeoffs",
            "",
            tradeoffs or "-",
            "",
            "## User directives considered",
            "",
        ]
        if directive_rows:
            for row in directive_rows:
                lines.extend([
                    f"- `{row['id']}` from `{row['source']}` via `{row['via_agent']}` "
                    f"task `{row['task_id']}` method `{row['method'] or '-'}`: {row['text']}",
                ])
        else:
            lines.append("- none recorded")
        lines.extend([
            "",
            "## Rejected alternatives",
            "",
            *([f"- {item}" for item in rejected] or ["- none"]),
            "",
            "## Affected paths",
            "",
            *([f"- `{item}`" for item in affected_paths] or ["- none"]),
            "",
            "## Dissent",
            "",
            *([f"- {item}" for item in dissent] or ["- none"]),
            "",
            "## User override",
            "",
            "Add a new user directive for the affected task. The decision becomes stale",
            "automatically and the task owner records a replacement decision.",
            "",
        ])
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        return path.relative_to(ROOT).as_posix()

    def _write_review(self, conflict_id: str) -> None:
        path = self.root / "reviews" / f"{conflict_id}.md"
        conflict = self.conn.execute(
            "SELECT * FROM conflicts WHERE conflict_id=?", (conflict_id,)).fetchone()
        if conflict is None:
            return
        options = self.conn.execute(
            "SELECT * FROM options WHERE conflict_id=? ORDER BY id",
            (conflict_id,)).fetchall()
        votes = self.conn.execute(
            "SELECT * FROM votes WHERE conflict_id=? ORDER BY id",
            (conflict_id,)).fetchall()
        lines = [
            f"# Conflict review: {conflict_id}",
            "",
            f"- Task: `{conflict['task_id'] or '-'}`",
            f"- Kind: `{conflict['kind']}`",
            f"- State: `{conflict['state']}`",
            f"- Summary: {conflict['summary']}",
            "",
            "## Options",
            "",
        ]
        if options:
            for row in options:
                lines.extend([
                    f"### `{row['option_id']}` {row['title']}",
                    "",
                    row["tradeoffs"] or "-",
                    "",
                    f"Patch: `{row['patch'] or '-'}`",
                    "",
                ])
        else:
            lines.append("- none")
        lines.extend(["", "## Votes", ""])
        if votes:
            for row in votes:
                lines.append(
                    f"- `{row['agent']}` -> `{row['option_id']}`: {row['rationale'] or '-'}")
        else:
            lines.append("- none")
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    def review_text(self, conflict_id: str) -> str:
        self._write_review(conflict_id)
        return (self.root / "reviews" / f"{conflict_id}.md").read_text(
            encoding="utf-8")

    # ── leases and shared files ─────────────────────────────────────────
    def shared_begin(self, agent: str, path: str, with_agents: list[str],
                     regions: dict[str, list[str]]) -> dict:
        self.require_agent(agent)
        relative = self._resolve_path(path)
        participants = sorted(set([agent, *with_agents]))
        used: set[str] = set()
        for participant in participants:
            names = regions.get(participant, [])
            if not names:
                raise CoordError(f"regions missing for participant {participant}", 2)
            for name in names:
                if name in used:
                    raise CoordError(f"overlapping shared region: {name}", 2)
                used.add(name)
        shared_id = new_id("shared")
        with self.conn:
            self.conn.execute(
                "INSERT INTO shared_files(shared_id,path,participants,regions,"
                "created_utc) VALUES (?,?,?,?,?)",
                (shared_id, relative, json.dumps(participants),
                 json.dumps(regions), utc_now()))
        self.event("coord.shared.requested", agent, shared=shared_id,
                   path=relative, participants=participants, session=agent)
        return {"shared": shared_id, "path": relative, "participants": participants}

    def shared_accept(self, agent: str, shared_id: str) -> dict:
        self.require_agent(agent)
        row = self.conn.execute(
            "SELECT * FROM shared_files WHERE shared_id=?", (shared_id,)).fetchone()
        if row is None:
            raise CoordError(f"unknown shared file {shared_id}", 2)
        if agent not in json.loads(row["participants"]):
            raise CoordError(f"{agent} is not a participant", 2)
        with self.conn:
            self.conn.execute(
                "UPDATE shared_files SET state='accepted' WHERE shared_id=?",
                (shared_id,))
        self.event("coord.shared.accepted", agent, shared=shared_id,
                   path=row["path"], session=agent)
        return {"shared": shared_id, "state": "accepted"}

    def shared_end(self, agent: str, shared_id: str) -> dict:
        self.require_agent(agent)
        with self.conn:
            self.conn.execute(
                "UPDATE shared_files SET state='ended', ended_utc=? "
                "WHERE shared_id=?", (utc_now(), shared_id))
        self.event("coord.shared.ended", agent, shared=shared_id, session=agent)
        return {"shared": shared_id, "state": "ended"}

    def edit_begin(self, agent: str, path: str, *, region: str = "",
                   known_hash: str = "", minutes: float = DEFAULT_TTL_MINUTES,
                   task_id: str = "") -> dict:
        self.require_agent(agent)
        relative = self._resolve_path(path)
        self._ensure_writable(relative)
        active = self._active_lease(relative)
        if active and active["holder"] != agent:
            raise CoordError(
                f"{relative} is leased by {active['holder']} until "
                f"{active['expires_utc']}; direct writes to one shared file "
                "stay serialized because a full-file save can overwrite "
                "another region's changes. Prepare a disjoint patch and notify "
                "the lease owner with: edit propose <path> --region <name> "
                "--patch <repo-local-patch> --summary <intent>", 2)
        shared = self._shared_for_path(relative)
        if shared and agent not in json.loads(shared["participants"]):
            raise CoordError(
                f"{relative} is a shared file for {shared['participants']}", 2)
        now = utc_now()
        expires = utc_after(minutes)
        current_hash = self._file_hash(relative)
        if known_hash and current_hash and known_hash != current_hash:
            raise CoordError(
                f"{relative} changed since the agreed hash; sync and re-read", 2)
        if active and active["holder"] == agent:
            with self.conn:
                self.conn.execute(
                    "UPDATE leases SET expires_utc=?, region=?, base_hash=? "
                    "WHERE id=?", (expires, region, current_hash, active["id"]))
            lease_id = active["lease_id"]
            self.event("coord.lease.renewed", agent, lease=lease_id,
                       path=relative, region=region, expires=expires, session=agent)
            return {"lease": lease_id, "path": relative, "expires_utc": expires}
        lease_id = new_id("lease")
        participants = json.loads(shared["participants"]) if shared else [agent]
        with self.conn:
            self.conn.execute(
                "INSERT INTO leases(lease_id,path,region,holder,participants,"
                "base_hash,expires_utc,created_utc) VALUES (?,?,?,?,?,?,?,?)",
                (lease_id, relative, region, agent, json.dumps(participants),
                 current_hash, expires, now))
        self.event("coord.lease.acquired", agent, lease=lease_id, path=relative,
                   region=region, base_hash=current_hash, expires=expires,
                   task=task_id, session=agent)
        return {"lease": lease_id, "path": relative, "expires_utc": expires,
                "base_hash": current_hash}

    def edit_propose(self, agent: str, path: str, region: str,
                     patch: str, summary: str) -> dict:
        """Prepare a same-file patch and notify the active lease owner."""
        self.require_agent(agent)
        relative = self._resolve_path(path)
        region = region.strip()
        summary = summary.strip()
        if not region or not summary:
            raise CoordError("edit proposal requires a region and summary", 2)
        active = self._active_lease(relative)
        if active is None:
            raise CoordError(
                f"{relative} has no active lease; use edit begin for a direct edit", 2)
        if active["holder"] == agent:
            raise CoordError(
                f"{agent} already holds the lease for {relative}; apply the "
                "patch within that lease instead of proposing to yourself", 2)
        holder = str(active["holder"])
        proposal = self.proposal_create(agent, holder, relative, patch)
        message = self.send(
            agent, holder,
            f"Same-file patch request: {relative} [{region}]",
            f"{summary}\n\nProposal {proposal['proposal']} contains a patch for "
            f"region '{region}'. It has not been applied. Please review it and, "
            "after your file lease is released, apply it if it still fits; the "
            "patch apply check will reject conflicting context.",
            kind="shared.request", paths=[relative], requires_response=True,
            due_minutes=DEFAULT_QUESTION_MINUTES)
        return {
            "proposal": proposal["proposal"],
            "message": message["message"],
            "recipient": holder,
            "path": relative,
            "region": region,
            "applied": False,
        }

    def edit_commit(self, agent: str, path: str, *, new_hash: str = "") -> dict:
        self.require_agent(agent)
        relative = self._resolve_path(path)
        active = self._active_lease(relative)
        if active is None or active["holder"] != agent:
            raise CoordError(f"{agent} does not hold a lease for {relative}", 2)
        current_hash = self._file_hash(relative)
        supplied = new_hash.strip()
        mismatch = bool(supplied and current_hash and supplied != current_hash)
        now = utc_now()
        with self.conn:
            self.conn.execute(
                "UPDATE leases SET state='released', released_utc=?, "
                "released_reason='commit' WHERE id=?", (now, active["id"]))
        self.event("coord.lease.released", agent, lease=active["lease_id"],
                   path=relative, reason="commit", hash=current_hash, session=agent)
        if mismatch:
            conflict_id = new_id("conflict")
            with self.conn:
                self.conn.execute(
                    "INSERT INTO conflicts(conflict_id,task_id,kind,summary,created_utc) "
                    "VALUES (?,?,?,?,?)",
                    (conflict_id, "", "file-change",
                     f"{relative} changed during edit; reconcile before writing",
                     now))
            self.event("coord.conflict.opened", agent, conflict=conflict_id,
                       path=relative, kind="file-change")
            raise CoordError(
                f"commit released {relative}, but the file hash changed; "
                f"conflict {conflict_id} recorded", 3)
        return {"released": active["lease_id"], "path": relative,
                "hash": current_hash}

    def edit_abort(self, agent: str, path: str) -> dict:
        self.require_agent(agent)
        relative = self._resolve_path(path)
        active = self._active_lease(relative)
        if active is None or active["holder"] != agent:
            raise CoordError(f"{agent} does not hold a lease for {relative}", 2)
        with self.conn:
            self.conn.execute(
                "UPDATE leases SET state='released', released_utc=?, "
                "released_reason='abort' WHERE id=?", (utc_now(), active["id"]))
        self.event("coord.lease.released", agent, lease=active["lease_id"],
                   path=relative, reason="abort", session=agent)
        return {"released": active["lease_id"], "path": relative}

    # ── proposals ────────────────────────────────────────────────────────
    def proposal_create(self, agent: str, recipient: str, path: str,
                        patch: str) -> dict:
        self.require_agent(agent)
        relative = self._resolve_path(path)
        patch_path = Path(patch)
        if not patch_path.is_absolute():
            patch_path = ROOT / patch_path
        patch_path = resolve_within(ROOT, patch_path, label="proposal patch",
                                    must_exist=True)
        data = patch_path.read_bytes()
        proposal_id = new_id("proposal")
        stored = self.root / "proposals" / f"{proposal_id}.patch"
        stored.write_bytes(data)
        digest = hashlib.sha256(data).hexdigest()
        with self.conn:
            self.conn.execute(
                "INSERT INTO proposals(proposal_id,path,sender,recipient,patch_path,"
                "patch_sha,created_utc) VALUES (?,?,?,?,?,?,?)",
                (proposal_id, relative, agent, recipient, str(stored), digest, utc_now()))
        self.event("coord.proposal.created", agent, proposal=proposal_id,
                   path=relative, recipient=recipient, sha=digest, session=agent)
        return {"proposal": proposal_id, "path": relative, "sha": digest}

    def proposal_show(self, proposal_id: str) -> dict:
        row = self.conn.execute(
            "SELECT * FROM proposals WHERE proposal_id=?", (proposal_id,)).fetchone()
        if row is None:
            raise CoordError(f"unknown proposal {proposal_id}", 2)
        result = dict(row)
        try:
            result["patch_text"] = Path(row["patch_path"]).read_text(
                encoding="utf-8", errors="replace")
        except OSError:
            result["patch_text"] = ""
        return result

    def proposal_apply(self, agent: str, proposal_id: str) -> dict:
        self.require_agent(agent)
        row = self.conn.execute(
            "SELECT * FROM proposals WHERE proposal_id=?", (proposal_id,)).fetchone()
        if row is None:
            raise CoordError(f"unknown proposal {proposal_id}", 2)
        if row["recipient"] != agent:
            raise CoordError(f"proposal is for {row['recipient']}", 2)
        relative = row["path"]
        active = self._active_lease(relative)
        if active:
            raise CoordError(f"{relative} is leased by {active['holder']}", 2)
        check = subprocess.run(
            ["git", "apply", "--check", row["patch_path"]], cwd=ROOT,
            text=True, capture_output=True)
        if check.returncode != 0:
            raise CoordError("patch does not apply: " + (check.stderr or check.stdout), 3)
        apply = subprocess.run(
            ["git", "apply", row["patch_path"]], cwd=ROOT, text=True,
            capture_output=True)
        if apply.returncode != 0:
            raise CoordError("patch apply failed: " + (apply.stderr or apply.stdout), 3)
        with self.conn:
            self.conn.execute(
                "UPDATE proposals SET state='applied', applied_utc=? "
                "WHERE proposal_id=?", (utc_now(), proposal_id))
        self.event("coord.proposal.applied", agent, proposal=proposal_id,
                   path=relative, session=agent)
        return {"proposal": proposal_id, "path": relative, "state": "applied"}

    def proposal_reject(self, agent: str, proposal_id: str, reason: str = "") -> dict:
        self.require_agent(agent)
        with self.conn:
            self.conn.execute(
                "UPDATE proposals SET state='rejected' WHERE proposal_id=?",
                (proposal_id,))
        self.event("coord.proposal.rejected", agent, proposal=proposal_id,
                   reason=reason, session=agent)
        return {"proposal": proposal_id, "state": "rejected"}

    # ── halt / freeze / guard ───────────────────────────────────────────
    def halt(self, agent: str, reason: str) -> dict:
        with self.conn:
            self.conn.execute(
                "UPDATE halt SET active=1, reason=?, by_agent=?, created_utc=?, "
                "released_utc='' WHERE id=1", (reason, agent, utc_now()))
        self.event("coord.halt.set", agent, reason=reason, session=agent)
        return {"halt": True, "reason": reason}

    def resume(self, agent: str) -> dict:
        with self.conn:
            self.conn.execute(
                "UPDATE halt SET active=0, released_utc=? WHERE id=1", (utc_now(),))
        self.event("coord.halt.cleared", agent, session=agent)
        return {"halt": False}

    def freeze(self, agent: str, path: str, reason: str) -> dict:
        relative = self._resolve_path(path)
        with self.conn:
            self.conn.execute(
                "INSERT INTO freezes(path,reason,created_utc) VALUES (?,?,?) "
                "ON CONFLICT(path) DO UPDATE SET reason=excluded.reason, "
                "created_utc=excluded.created_utc, released_utc=''",
                (relative, reason, utc_now()))
        self.event("coord.freeze.set", agent, path=relative, reason=reason,
                   session=agent)
        return {"path": relative, "frozen": True}

    def unfreeze(self, agent: str, path: str) -> dict:
        relative = self._resolve_path(path)
        with self.conn:
            self.conn.execute(
                "UPDATE freezes SET released_utc=? WHERE path=?",
                (utc_now(), relative))
        self.event("coord.freeze.cleared", agent, path=relative, session=agent)
        return {"path": relative, "frozen": False}

    def guard(self, agent: str, staged: bool = False,
              paths: list[str] | None = None) -> dict:
        targets = list(paths or [])
        if staged:
            result = subprocess.run(
                ["git", "diff", "--cached", "--name-only", "-z"], cwd=ROOT,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if result.returncode != 0:
                raise CoordError("cannot read staged files", 4)
            targets.extend(item for item in result.stdout.decode(
                "utf-8", errors="replace").split("\0") if item)
        blocked: list[dict] = []
        halted = self._halted()
        if halted:
            blocked.append({"kind": "halt", "reason": halted["reason"]})
        for raw in targets:
            relative = self._resolve_path(raw)
            frozen = self._frozen(relative)
            if frozen:
                blocked.append({"kind": "freeze", "path": relative,
                                "reason": frozen["reason"]})
            lease = self._active_lease(relative)
            if lease and lease["holder"] != agent:
                blocked.append({"kind": "lease", "path": relative,
                                "holder": lease["holder"],
                                "expires_utc": lease["expires_utc"]})
        return {"ok": not blocked, "blocked": blocked, "checked": targets}

    def status(self, agent: str = "") -> dict:
        self._sweep(agent)
        halt = self._halted()
        unread = 0
        if agent:
            unread = int(self.conn.execute(
                "SELECT COUNT(*) FROM messages WHERE read_at='' AND "
                "(recipient=? OR recipient IN ('all','any'))", (agent,)).fetchone()[0])
        return {
            "halt": dict(halt) if halt else None,
            "freezes": [dict(row) for row in self.conn.execute(
                "SELECT * FROM freezes WHERE released_utc='' ORDER BY path")],
            "agents": [dict(row) for row in self.conn.execute(
                "SELECT agent,task,status,pid,last_seen_utc,parent,cohort,capabilities FROM agents ORDER BY agent")],
            "leases": [dict(row) for row in self.conn.execute(
                "SELECT * FROM leases WHERE state='active' ORDER BY path")],
            "shared": [dict(row) for row in self.conn.execute(
                "SELECT * FROM shared_files WHERE state IN ('pending','accepted') "
                "ORDER BY path")],
            "conflicts": [dict(row) for row in self.conn.execute(
                "SELECT * FROM conflicts WHERE state='open' ORDER BY id")],
            "decisions": self.decisions(10),
            "activity": [dict(row) for row in self.conn.execute(
                "SELECT * FROM activity ORDER BY agent")],
            "issues": [dict(row) for row in self.conn.execute(
                "SELECT id,title,kind,state,severity,priority,owner,summary,"
                "deferred_reason,deferred_until,updated_utc FROM issues "
                "WHERE state NOT IN ('done','cancelled') ORDER BY updated_utc DESC")],
            "proof": self.proof_status(),
            "build_requests": self.build_list(),
            "build_owner": self.build_owner(),
            "runtimes": self.runtime_list(),
            "work": self.work_list(active_only=True),
            "user_context": self.user_context_list(limit=10),
            "unread": unread,
        }

    def message_view(self, message_id: str) -> dict:
        row = self.conn.execute(
            "SELECT * FROM messages WHERE msg_id=?", (message_id,)).fetchone()
        if row is None:
            raise CoordError(f"unknown message {message_id}", 2)
        return dict(row)


def coordination_brief(agent: str = "") -> dict:
    """Read-only summary used by the dev-loop status hook."""
    configured = os.environ.get("MIMITA_COORD_ROOT", "").strip()
    root = (Path(configured) if configured else ROOT / ".dev" / "agent-coord")
    root = resolve_within(ROOT, root, label="coordination root")
    if not (root / "brain.db").is_file():
        return {}
    store = CoordStore(root, publish_docs=False)
    try:
        data = store.status(agent)
        data["latest_message"] = None
        rows = store.conn.execute(
            "SELECT msg_id, sender, subject, created_utc FROM messages "
            "ORDER BY id DESC LIMIT 1").fetchone()
        if rows:
            data["latest_message"] = dict(rows)
        return data
    finally:
        store.close()


def coordination_message(message_id: str) -> dict:
    store = CoordStore(publish_docs=False)
    try:
        return store.message_view(message_id)
    finally:
        store.close()


def format_brief(brief: dict) -> str:
    if not brief:
        return "[COORD] no coordination state"
    halt = brief.get("halt") or {}
    conflicts = brief.get("conflicts") or []
    leases = brief.get("leases") or []
    decisions = brief.get("decisions") or []
    activity = {row.get("agent"): row for row in (brief.get("activity") or [])}
    issues = brief.get("issues") or []
    agents = brief.get("agents") or []
    proof = brief.get("proof") or {}
    proofs = proof.get("proofs") or []
    unproven = sum(1 for row in proofs
                   if row.get("result") not in {"proven", "exempt"})
    contexts = brief.get("user_context") or []
    line = (f"[COORD] unread={brief.get('unread', 0)} "
            f"contexts={len(contexts)} "
            f"conflicts={len(conflicts)} leases={len(leases)} "
            f"agents={len(agents)} open_issues={len(issues)} "
            f"unproven={unproven} stale={proof.get('stale', 0)} "
            f"halt={'yes' if halt.get('active') else 'no'}")
    if decisions:
        current = decisions[0]
        line += f" decision={current.get('decision_id')} task={current.get('task_id') or '-'}"
    lines = [line]
    for agent in agents[:8]:
        act = activity.get(agent.get("agent"), {})
        parts = [f"agent={agent.get('agent')}", f"status={agent.get('status')}"]
        if act.get("phase"):
            parts.append(f"phase={act['phase']}")
        if act.get("build_id"):
            parts.append(f"build={act['build_id']}")
        if act.get("run_id"):
            parts.append(f"run={act['run_id']}")
        if act.get("blocker"):
            parts.append(f"blocker={act['blocker']}")
        if act.get("wait_reason"):
            parts.append(f"wait={act['wait_reason']}")
        lines.append("[BOARD] " + " ".join(parts))
    for context in contexts[:2]:
        lines.append(f"[USER] {context.get('context_id')} "
                     f"priority={context.get('priority')} "
                     f"issue={context.get('canonical_issue_id') or '-'} "
                     f"{context.get('quote', '')[:140]}")
    for issue in issues[:5]:
        lines.append(f"[ISSUE] {issue.get('id')} {issue.get('state')} "
                     f"kind={issue.get('kind')} owner={issue.get('owner') or '-'} "
                     f"{issue.get('summary') or issue.get('title') or ''}")
    for row in proofs:
        if row.get("result") in {"proven", "exempt"}:
            continue
        lines.append(f"[PROOF] task={row.get('task_id')} "
                     f"result={row.get('result')} "
                     f"reason={row.get('reason') or '-'}")
    return "\n".join(lines)


# ── CLI ──────────────────────────────────────────────────────────────────
def _agent_arg(args: argparse.Namespace, required: bool = True) -> str:
    agent = (getattr(args, "agent", "") or os.environ.get("MIMITA_AGENT_ID", "")).strip()
    if required and not agent:
        raise CoordError("--agent or MIMITA_AGENT_ID is required", 2)
    return agent


def _print(value: object, as_json: bool = False) -> None:
    if as_json:
        print(json.dumps(value, indent=2, sort_keys=True))
    elif isinstance(value, str):
        print(value)
    else:
        print(json.dumps(value, indent=2, sort_keys=True))


def _paths(value: list[str] | None) -> list[str]:
    return list(value or [])


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="MiMITA shared agent coordination brain")
    parser.add_argument("--root", default="", help="coordination state root")
    parser.add_argument("--json", action="store_true", help="print JSON")
    parser.add_argument("--agent", default="", help="agent name; defaults to MIMITA_AGENT_ID")
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("begin")
    p.add_argument("--task", default="")
    p.add_argument("--parent", default="")
    p.add_argument("--cohort", default="")
    p.add_argument("--capability", action="append", default=[])
    p = sub.add_parser("end")
    p = sub.add_parser("sync")
    p.add_argument("--peek", action="store_true")
    p = sub.add_parser("status")
    p = sub.add_parser("inbox")
    p.add_argument("--all", action="store_true")

    p = sub.add_parser("announce")
    p.add_argument("--purpose", default="")
    p.add_argument("--paths", nargs="*", default=[])
    p.add_argument("--minutes", type=float, default=60.0)

    p = sub.add_parser("send")
    p.add_argument("--to", required=True)
    p.add_argument("--subject", default="")
    p.add_argument("body")
    p.add_argument("--kind", default="message")
    p.add_argument("--paths", nargs="*", default=[])
    p.add_argument("--requires-response", action="store_true")
    p.add_argument("--minutes", type=float, default=0.0)
    p.add_argument("--reply-to", default="")
    p.add_argument("--thread", default="")
    p.add_argument("--issue", default="",
                   help="append this message to the issue discussion")

    p = sub.add_parser("ask")
    p.add_argument("--to", required=True)
    p.add_argument("--subject", default="")
    p.add_argument("body")
    p.add_argument("--wait", type=float, default=0.0,
                   help="bounded seconds; 0 returns immediately")
    p.add_argument("--minutes", type=float, default=DEFAULT_QUESTION_MINUTES)
    p.add_argument("--issue", default="",
                   help="append this question to the issue discussion")

    p = sub.add_parser("reply")
    p.add_argument("message_id")
    p.add_argument("body")
    p.add_argument("--issue", default="",
                   help="append this reply to the issue discussion")

    p = sub.add_parser("wait")
    p.add_argument("--timeout", type=float, default=300.0)

    p = sub.add_parser("note")
    nsub = p.add_subparsers(dest="note_command", required=True)
    n = nsub.add_parser("add")
    n.add_argument("body")
    n.add_argument("--task", default="")
    n.add_argument("--tags", default="")
    n = nsub.add_parser("read")
    n.add_argument("--since", type=int, default=0)
    n.add_argument("--task", default="")
    n = nsub.add_parser("search")
    n.add_argument("text")

    p = sub.add_parser("directive")
    dsub = p.add_subparsers(dest="directive_command", required=True)
    d = dsub.add_parser("add")
    d.add_argument("text")
    d.add_argument("--task", default="")
    d.add_argument("--method", default="")
    d.add_argument("--source", default="agent")
    d.add_argument("--via", default="")
    d.add_argument("--priority", default="normal")

    p = sub.add_parser("task")
    tsub = p.add_subparsers(dest="task_command", required=True)
    t = tsub.add_parser("add")
    t.add_argument("task_id")
    t.add_argument("--title", default="")
    t.add_argument("--owner", default="")
    t.add_argument("--issue", default="")
    t.add_argument("--priority", default="normal")
    t.add_argument("--acceptance", default="")
    t = tsub.add_parser("owner")
    t.add_argument("task_id")
    t.add_argument("owner")
    t = tsub.add_parser("dep")
    t.add_argument("task_id")
    t.add_argument("before_id")
    t = tsub.add_parser("method")
    t.add_argument("task_id")
    t.add_argument("method")
    t = tsub.add_parser("status")
    t.add_argument("task_id")
    t.add_argument("state")

    p = sub.add_parser("context")
    csub = p.add_subparsers(dest="context_command", required=True)
    c = csub.add_parser("add")
    c.add_argument("quote")
    c.add_argument("--source", default="user")
    c.add_argument("--session", default="")
    c.add_argument("--source-ref", default="")
    c.add_argument("--priority", default="normal")
    c.add_argument("--tags", nargs="*", default=[])
    c.add_argument("--issue", default="")
    c = csub.add_parser("list")
    c.add_argument("--limit", type=int, default=20)
    c.add_argument("--unread", action="store_true")
    c.add_argument("--high", action="store_true")
    c = csub.add_parser("ack")
    c.add_argument("context_id", nargs="?", default="")
    c = csub.add_parser("link")
    c.add_argument("context_id")
    c.add_argument("issue_id")
    c.add_argument("--related", action="store_true")
    c.add_argument("--reason", default="")
    c = csub.add_parser("move")
    c.add_argument("context_id")
    c.add_argument("issue_id")
    c.add_argument("--reason", required=True)
    c = csub.add_parser("lint")

    p = sub.add_parser("issue")
    isub = p.add_subparsers(dest="issue_command", required=True)
    i = isub.add_parser("add")
    i.add_argument("title")
    i.add_argument("--quote", default="")
    i.add_argument("--summary", default="")
    i.add_argument("--kind", default="bug")
    i.add_argument("--source", default="agent")
    i.add_argument("--via", default="")
    i.add_argument("--severity", default="normal")
    i.add_argument("--priority", default="normal")
    i.add_argument("--owner", default="")
    i.add_argument("--evidence", default="")
    i.add_argument("--id", default="")
    i.add_argument("--reproduction", default="")
    i.add_argument("--expected-behavior", default="")
    i.add_argument("--acceptance", default="")
    i.add_argument("--paths", nargs="*", default=[])
    i.add_argument("--dependencies", nargs="*", default=[])
    i.add_argument("--risk", default="")
    i.add_argument("--proof-expectation", default="")
    i.add_argument("--context", default="")
    i = isub.add_parser("list")
    i.add_argument("--state", default="")
    i.add_argument("--kind", default="")
    i.add_argument("--owner", default="")
    i.add_argument("--limit", type=int, default=100)
    i = isub.add_parser("show")
    i.add_argument("issue_id")
    i = isub.add_parser("brief")
    i.add_argument("issue_id")
    i = isub.add_parser("triage")
    i.add_argument("issue_id")
    i.add_argument("--summary", default="")
    i.add_argument("--owner", default="")
    i.add_argument("--priority", default="")
    i = isub.add_parser("assign")
    i.add_argument("issue_id")
    i.add_argument("owner")
    i = isub.add_parser("defer")
    i.add_argument("issue_id")
    i.add_argument("--reason", required=True)
    i.add_argument("--until", default="")
    i = isub.add_parser("clarify")
    i.add_argument("issue_id")
    i.add_argument("--question", required=True)
    i = isub.add_parser("close")
    i.add_argument("issue_id")
    i.add_argument("--resolution", required=True)
    i.add_argument("--evidence", default="")
    i = isub.add_parser("exempt")
    i.add_argument("issue_id")
    i.add_argument("--reason", required=True)
    i.add_argument("--paths", nargs="*", default=[])
    i = isub.add_parser("quote")
    i.add_argument("issue_id")
    i.add_argument("quote")
    i.add_argument("--supports", default="")
    i = isub.add_parser("suggest")
    i.add_argument("issue_id")
    i.add_argument("body")
    i = isub.add_parser("review")
    i.add_argument("issue_id")
    i.add_argument("--verdict", required=True)
    i.add_argument("--body", default="")
    i = isub.add_parser("link-task")
    i.add_argument("issue_id")
    i.add_argument("task_id")
    i.add_argument("--step", type=int, default=0)
    i = isub.add_parser("context")
    i.add_argument("issue_id")
    i.add_argument("--reproduction", default=None)
    i.add_argument("--expected-behavior", default=None)
    i.add_argument("--acceptance", default=None)
    i.add_argument("--paths", nargs="*", default=None)
    i.add_argument("--build", default=None)
    i.add_argument("--run", default=None)
    i.add_argument("--evidence", default=None)
    i.add_argument("--dependencies", nargs="*", default=None)
    i.add_argument("--risk", default=None)
    i.add_argument("--proof-expectation", default=None)
    i.add_argument("--user-context", default=None)
    i = isub.add_parser("lint")
    i.add_argument("--limit", type=int, default=200)
    i = isub.add_parser("readiness")
    i.add_argument("issue_id")
    i = isub.add_parser("needs-info")
    i.add_argument("--limit", type=int, default=100)

    p = sub.add_parser("proof")
    psub = p.add_subparsers(dest="proof_command", required=True)
    q = psub.add_parser("record")
    q.add_argument("issue_id")
    q.add_argument("--kind", choices=("integration", "unit"), default="integration")
    q.add_argument("--generation", type=int, default=0)
    q.add_argument("--runtime", default="")
    q.add_argument("--run-id", default="")
    q.add_argument("--server-journal", default="")
    q.add_argument("--client-journal", default="")
    q.add_argument("--event", action="append", default=[])
    q.add_argument("--dll", default="")
    q = psub.add_parser("verify")
    q.add_argument("evidence_id")
    q = psub.add_parser("list")
    q.add_argument("--issue", default="")
    q = psub.add_parser("task-add")
    q.add_argument("task_id")
    q.add_argument("--paths", nargs="*", default=[])
    q.add_argument("--build-id", default="")
    q.add_argument("--generation", type=int, default=0)
    q.add_argument("--run-id", default="")
    q.add_argument("--journal", default="")
    q.add_argument("--event", default="")
    q.add_argument("--field", default="")
    q = psub.add_parser("task-verify")
    q.add_argument("proof_id")
    q = psub.add_parser("exempt")
    q.add_argument("task_id")
    q.add_argument("--reason", required=True)
    q.add_argument("--paths", nargs="*", default=[])
    q = psub.add_parser("status")
    p = sub.add_parser("trace")
    p.add_argument("issue_id")

    p = sub.add_parser("work")
    wsub = p.add_subparsers(dest="work_command", required=True)
    w = wsub.add_parser("start")
    w.add_argument("--task", default="")
    w.add_argument("--issue", default="")
    w.add_argument("--message", default="")
    w.add_argument("--notes", default="")
    w = wsub.add_parser("phase")
    w.add_argument("session_id")
    w.add_argument("name")
    w.add_argument("--wait-reason", default="")
    w.add_argument("--build-id", default="")
    w.add_argument("--run-id", default="")
    w.add_argument("--notes", default="")
    w = wsub.add_parser("wait")
    w.add_argument("--phase", default="build_wait")
    w.add_argument("--wait-reason", required=True)
    w.add_argument("--build-id", default="")
    w.add_argument("--run-id", default="")
    w.add_argument("--notes", default="")
    w = wsub.add_parser("resume")
    w.add_argument("--notes", default="")
    w = wsub.add_parser("end")
    w.add_argument("session_id")
    w.add_argument("--state", default="done")
    w.add_argument("--notes", default="")
    w = wsub.add_parser("status")
    w.add_argument("session_id")
    w = wsub.add_parser("list")
    w.add_argument("--active", action="store_true")
    w = wsub.add_parser("summary")
    w.add_argument("--issue", default="")

    p = sub.add_parser("activity")
    asub = p.add_subparsers(dest="activity_command", required=True)
    a = asub.add_parser("set")
    a.add_argument("--task", default="")
    a.add_argument("--issue", default="")
    a.add_argument("--phase", default="")
    a.add_argument("--activity", default="")
    a.add_argument("--paths", nargs="*", default=[])
    a.add_argument("--build-id", default="")
    a.add_argument("--game-generation", type=int, default=0)
    a.add_argument("--run-id", default="")
    a.add_argument("--blocker", default="")
    a.add_argument("--next-step", default="")
    a.add_argument("--wait-reason", default="")
    p = sub.add_parser("board")
    p = sub.add_parser("next")
    p.add_argument("--claim", action="store_true")
    p.add_argument("--mode", choices=("work", "waiting"), default="work",
                   help="waiting returns proof/context repair work")
    p = sub.add_parser("observe")
    p.add_argument("issue_id")
    p = sub.add_parser("wake-check")
    p = sub.add_parser("build")
    bsub = p.add_subparsers(dest="build_command", required=True)
    b = bsub.add_parser("request")
    b.add_argument("--kind", choices=("integration", "preview"),
                   default="integration")
    b.add_argument("--issue", default="")
    b.add_argument("--base-generation", type=int, default=0)
    b.add_argument("--paths", nargs="*", default=[])
    b.add_argument("--overlay", nargs="*", default=[])
    b.add_argument("--priority", default="normal")
    b.add_argument("--cohort", default="")
    b = bsub.add_parser("list")
    b.add_argument("--status", default="")
    b = bsub.add_parser("owner")
    b.add_argument("--set", default="")
    b = bsub.add_parser("approve")
    b.add_argument("request_id")
    b = bsub.add_parser("reject")
    b.add_argument("request_id")
    b.add_argument("--reason", default="")
    b = bsub.add_parser("wait")
    b.add_argument("request_id")
    b.add_argument("--reason", required=True)
    b.add_argument("--seconds", type=float, default=0.0)
    p = sub.add_parser("runtime")
    rsub = p.add_subparsers(dest="runtime_command", required=True)
    r = rsub.add_parser("register")
    r.add_argument("runtime_id")
    r.add_argument("--cohort", default="")
    r.add_argument("--issue", default="")
    r.add_argument("--generation", type=int, default=0)
    r.add_argument("--dll", default="")
    r.add_argument("--dll-sha", default="")
    r.add_argument("--run-id", default="")
    r.add_argument("--server-pid", type=int, default=0)
    r.add_argument("--client-pids", nargs="*", type=int, default=[])
    r.add_argument("--server-journal", default="")
    r.add_argument("--client-journals", nargs="*", default=[])
    r.add_argument("--state", default="starting")
    r = rsub.add_parser("list")
    r.add_argument("--state", default="")
    r = rsub.add_parser("update")
    r.add_argument("runtime_id")
    r.add_argument("state")

    p = sub.add_parser("conflict")
    csub = p.add_subparsers(dest="conflict_command", required=True)
    c = csub.add_parser("list")
    c.add_argument("--all", action="store_true")
    c = csub.add_parser("review")
    c.add_argument("conflict_id")

    p = sub.add_parser("option")
    osub = p.add_subparsers(dest="option_command", required=True)
    o = osub.add_parser("add")
    o.add_argument("conflict_id")
    o.add_argument("title")
    o.add_argument("--tradeoffs", default="")
    o.add_argument("--patch", default="")

    p = sub.add_parser("vote")
    p.add_argument("conflict_id")
    p.add_argument("option_id")
    p.add_argument("--rationale", default="")

    p = sub.add_parser("decision")
    dsub = p.add_subparsers(dest="decision_command", required=True)
    d = dsub.add_parser("record")
    d.add_argument("task_id")
    d.add_argument("chosen")
    d.add_argument("--conflict", default="")
    d.add_argument("--rationale", default="")
    d.add_argument("--tradeoffs", default="")
    d.add_argument("--rejected", nargs="*", default=[])
    d.add_argument("--paths", nargs="*", default=[])
    d.add_argument("--dissent", nargs="*", default=[])
    d.add_argument("--supersedes", default="")
    d = dsub.add_parser("list")
    d.add_argument("--limit", type=int, default=20)

    p = sub.add_parser("shared")
    ssub = p.add_subparsers(dest="shared_command", required=True)
    s = ssub.add_parser("begin")
    s.add_argument("path")
    s.add_argument("--with", dest="with_agents", action="append", default=[])
    s.add_argument("--regions", required=True,
                   help="JSON object mapping participant to region names, or a file")
    s = ssub.add_parser("accept")
    s.add_argument("shared_id")
    s = ssub.add_parser("end")
    s.add_argument("shared_id")

    p = sub.add_parser("edit")
    esub = p.add_subparsers(dest="edit_command", required=True)
    e = esub.add_parser("begin")
    e.add_argument("path")
    e.add_argument("--region", default="")
    e.add_argument("--hash", default="")
    e.add_argument("--minutes", type=float, default=DEFAULT_TTL_MINUTES)
    e.add_argument("--task", default="")
    e = esub.add_parser("propose")
    e.add_argument("path")
    e.add_argument("--region", required=True)
    e.add_argument("--patch", required=True,
                   help="repository-local patch file prepared outside the leased source file")
    e.add_argument("--summary", required=True)
    e = esub.add_parser("commit")
    e.add_argument("path")
    e.add_argument("--hash", default="")
    e = esub.add_parser("abort")
    e.add_argument("path")

    p = sub.add_parser("proposal")
    psub = p.add_subparsers(dest="proposal_command", required=True)
    q = psub.add_parser("create")
    q.add_argument("path")
    q.add_argument("--to", required=True)
    q.add_argument("--patch", required=True)
    q = psub.add_parser("show")
    q.add_argument("proposal_id")
    q = psub.add_parser("apply")
    q.add_argument("proposal_id")
    q = psub.add_parser("reject")
    q.add_argument("proposal_id")
    q.add_argument("--reason", default="")

    p = sub.add_parser("halt")
    p.add_argument("--reason", default="")
    p = sub.add_parser("resume")
    p = sub.add_parser("freeze")
    p.add_argument("path")
    p.add_argument("--reason", default="")
    p = sub.add_parser("unfreeze")
    p.add_argument("path")
    p = sub.add_parser("guard")
    p.add_argument("--staged", action="store_true")
    p.add_argument("--paths", nargs="*", default=[])
    return parser


def _load_regions(value: str) -> dict[str, list[str]]:
    text = value
    path = Path(value)
    if not path.is_absolute():
        path = ROOT / path
    if path.is_file():
        text = path.read_text(encoding="utf-8")
    data = json.loads(text)
    if not isinstance(data, dict):
        raise CoordError("--regions must be a JSON object", 2)
    return {str(key): [str(item) for item in (value or [])]
            for key, value in data.items()}


def run(args: argparse.Namespace) -> object:
    needs_agent = args.command not in {"status", "conflict", "guard", "board",
                                      "trace", "observe"}
    if args.command == "decision" and args.decision_command == "list":
        needs_agent = False
    if args.command == "proof" and args.proof_command in {"list", "status"}:
        needs_agent = False
    if args.command == "issue" and args.issue_command in {
            "list", "show", "readiness", "needs-info", "brief", "lint"}:
        needs_agent = False
    if args.command == "context" and args.context_command in {"list", "lint"}:
        needs_agent = False
    if args.command == "work" and args.work_command in {
            "status", "list", "summary"}:
        needs_agent = False
    if args.command == "build" and (
            args.build_command == "list" or
            (args.build_command == "owner" and not args.set)):
        needs_agent = False
    if args.command == "runtime" and args.runtime_command == "list":
        needs_agent = False
    agent = _agent_arg(args, required=needs_agent)
    store = CoordStore(Path(args.root) if args.root else None)
    try:
        command = args.command
        if command == "begin":
            return store.begin(agent, args.task, parent=args.parent,
                               cohort=args.cohort,
                               capabilities=args.capability)
        if command == "end":
            return store.end(agent)
        if command == "sync":
            return store.sync(agent, mark_read=not args.peek)
        if command == "status":
            return store.status(agent)
        if command == "inbox":
            return store.inbox(agent, unread_only=not args.all)
        if command == "context":
            if args.context_command == "add":
                return store.user_context_add(
                    agent, args.quote, source=args.source,
                    session=args.session, source_ref=args.source_ref,
                    priority=args.priority, tags=args.tags, issue_id=args.issue)
            if args.context_command == "list":
                return store.user_context_list(
                    agent, limit=args.limit, unread_only=args.unread,
                    high_priority_only=args.high)
            if args.context_command == "ack":
                return store.user_context_ack(agent, args.context_id)
            if args.context_command == "link":
                return store.user_context_link(
                    agent, args.context_id, args.issue_id,
                    relation="related" if args.related else "primary",
                    reason=args.reason)
            if args.context_command == "move":
                return store.user_context_move(
                    agent, args.context_id, args.issue_id, args.reason)
            return store.user_context_lint()
        if command == "announce":
            return store.send(agent, "all", "intent: " + (args.purpose or "working"),
                              args.purpose or "working", kind="announce",
                              paths=_paths(args.paths))
        if command == "send":
            return store.send(agent, args.to, args.subject, args.body,
                              kind=args.kind, paths=_paths(args.paths),
                              requires_response=args.requires_response,
                              due_minutes=args.minutes, reply_to=args.reply_to,
                              thread=args.thread, issue_id=args.issue)
        if command == "ask":
            sent = store.send(agent, args.to, args.subject, args.body,
                              kind="question", requires_response=True,
                              due_minutes=args.minutes, issue_id=args.issue)
            if args.wait > 0:
                answer = store.wait_for_answer(agent, sent["message"], args.wait)
                return {"sent": sent, "answer": answer, "timed_out": answer is None}
            return sent
        if command == "reply":
            return store.reply(agent, args.message_id, args.body,
                               issue_id=args.issue)
        if command == "wait":
            deadline = time.monotonic() + max(0.0, args.timeout)
            while time.monotonic() < deadline:
                messages = store.inbox(agent, unread_only=True)
                if messages:
                    return {"messages": messages}
                time.sleep(0.25)
            return {"messages": [], "timed_out": True}
        if command == "note":
            if args.note_command == "add":
                return store.note_add(agent, args.body, task_id=args.task,
                                      tags=args.tags)
            if args.note_command == "read":
                return store.note_read(args.since, args.task)
            return store.note_search(args.text)
        if command == "directive":
            return store.directive_add(agent, args.text, task_id=args.task,
                                       method=args.method, source=args.source,
                                       via_agent=args.via, priority=args.priority)
        if command == "task":
            if args.task_command == "add":
                return store.task_add(
                    agent, args.task_id, args.title, args.owner,
                    issue_id=args.issue, priority=args.priority,
                    acceptance=args.acceptance)
            if args.task_command == "owner":
                return store.task_owner(agent, args.task_id, args.owner)
            if args.task_command == "dep":
                return store.task_dep(agent, args.task_id, args.before_id)
            if args.task_command == "method":
                return store.task_method(agent, args.task_id, args.method)
            return store.task_status(agent, args.task_id, args.state)
        if command == "issue":
            if args.issue_command == "add":
                evidence: object = {}
                if args.evidence:
                    try:
                        evidence = json.loads(args.evidence)
                    except json.JSONDecodeError:
                        evidence = {"text": args.evidence}
                return store.issue_add(
                    agent, args.title, quote=args.quote, summary=args.summary,
                    kind=args.kind, source=args.source, via_agent=args.via,
                    severity=args.severity, priority=args.priority,
                    owner=args.owner, evidence=evidence, issue_id=args.id,
                    reproduction=args.reproduction,
                    expected_behavior=args.expected_behavior,
                    acceptance=args.acceptance, paths=args.paths,
                    dependencies=args.dependencies, risk=args.risk,
                    proof_expectation=args.proof_expectation,
                    context_id=args.context)
            if args.issue_command == "list":
                return store.issue_list(
                    state=args.state, kind=args.kind, owner=args.owner,
                    limit=args.limit)
            if args.issue_command == "show":
                return store.issue_show(args.issue_id)
            if args.issue_command == "brief":
                return store.issue_brief(args.issue_id)
            if args.issue_command == "triage":
                return store.issue_triage(
                    agent, args.issue_id, summary=args.summary,
                    owner=args.owner, priority=args.priority)
            if args.issue_command == "assign":
                return store.issue_assign(agent, args.issue_id, args.owner)
            if args.issue_command == "defer":
                return store.issue_defer(
                    agent, args.issue_id, args.reason, args.until)
            if args.issue_command == "clarify":
                return store.issue_clarify(
                    agent, args.issue_id, args.question)
            if args.issue_command == "close":
                return store.issue_close(
                    agent, args.issue_id, args.resolution, args.evidence)
            if args.issue_command == "exempt":
                return store.issue_exempt(
                    agent, args.issue_id, args.reason, args.paths)
            if args.issue_command == "quote":
                return store.issue_quote(
                    agent, args.issue_id, args.quote, args.supports)
            if args.issue_command == "suggest":
                return store.issue_suggest(agent, args.issue_id, args.body)
            if args.issue_command == "review":
                return store.issue_review(
                    agent, args.issue_id, args.verdict, args.body)
            if args.issue_command == "context":
                return store.issue_context(
                    agent, args.issue_id,
                    reproduction=args.reproduction,
                    expected_behavior=args.expected_behavior,
                    acceptance=args.acceptance, paths=args.paths,
                    related_build=args.build, related_run=args.run,
                    dependencies=args.dependencies, risk=args.risk,
                    proof_expectation=args.proof_expectation,
                    context_id=args.user_context,
                    evidence=(json.loads(args.evidence)
                              if args.evidence and args.evidence.lstrip().startswith(("{", "["))
                              else ({"text": args.evidence} if args.evidence else None)))
            if args.issue_command == "lint":
                return store.issue_lint(args.limit)
            if args.issue_command == "readiness":
                return store.issue_readiness(args.issue_id)
            if args.issue_command == "needs-info":
                return [
                    store.issue_readiness(row["id"])
                    for row in store.issue_list(limit=args.limit)
                    if not store.issue_readiness(row["id"])["ready"]]
            return store.issue_link_task(
                agent, args.issue_id, args.task_id, args.step)
        if command == "proof":
            if args.proof_command == "record":
                return store.evidence_record(
                    agent, args.issue_id, proof_kind=args.kind,
                    build_generation=args.generation, runtime_id=args.runtime,
                    run_id=args.run_id, server_journal=args.server_journal,
                    client_journal=args.client_journal,
                    required_events=args.event, dll_path=args.dll)
            if args.proof_command == "verify":
                return store.evidence_verify(agent, args.evidence_id)
            if args.proof_command == "task-add":
                return store.proof_add(
                    agent, args.task_id, paths=args.paths,
                    build_id=args.build_id, game_generation=args.generation,
                    run_id=args.run_id, journal_path=args.journal,
                    event=args.event, field_predicate=args.field)
            if args.proof_command == "task-verify":
                return store.proof_verify(agent, args.proof_id)
            if args.proof_command == "exempt":
                return store.proof_exempt(
                    agent, args.task_id, args.reason, args.paths)
            if args.proof_command == "status":
                return store.proof_status()
            return store.evidence_list(args.issue)
        if command == "trace":
            return store.issue_trace(args.issue_id)
        if command == "activity":
            return store.activity_set(
                agent, task_id=args.task, issue_id=args.issue,
                phase=args.phase, activity=args.activity, paths=args.paths,
                build_id=args.build_id,
                game_generation=args.game_generation, run_id=args.run_id,
                blocker=args.blocker, next_step=args.next_step,
                wait_reason=args.wait_reason)
        if command == "work":
            if args.work_command == "start":
                return store.work_start(
                    agent, task_id=args.task, issue_id=args.issue,
                    message_id=args.message, notes=args.notes)
            if args.work_command == "phase":
                return store.work_phase(
                    agent, args.session_id, args.name,
                    wait_reason=args.wait_reason, build_id=args.build_id,
                    run_id=args.run_id, notes=args.notes)
            if args.work_command == "wait":
                return store.work_wait(
                    agent, args.phase, wait_reason=args.wait_reason,
                    build_id=args.build_id, run_id=args.run_id,
                    notes=args.notes)
            if args.work_command == "resume":
                return store.work_resume(agent, notes=args.notes)
            if args.work_command == "end":
                return store.work_end(
                    agent, args.session_id, state=args.state, notes=args.notes)
            if args.work_command == "status":
                return store.work_status(args.session_id)
            if args.work_command == "list":
                return store.work_list(agent=agent, active_only=args.active)
            return store.work_summary(agent=agent, issue_id=args.issue)
        if command == "board":
            return store.board(agent=agent)
        if command == "next":
            if args.mode == "waiting":
                return store.waiting_work(agent)
            return store.next_work(agent, claim=args.claim)
        if command == "observe":
            return store.observe_issue(args.issue_id, agent)
        if command == "wake-check":
            return store.wake_check(agent)
        if command == "build":
            if args.build_command == "request":
                return store.build_request(
                    agent, kind=args.kind, issue_id=args.issue,
                    base_generation=args.base_generation, paths=args.paths,
                    overlay_paths=args.overlay, priority=args.priority,
                    cohort=args.cohort)
            if args.build_command == "list":
                return store.build_list(status=args.status)
            if args.build_command == "owner":
                return store.build_owner(args.set)
            if args.build_command == "approve":
                return store.build_update(agent, args.request_id, "approved")
            if args.build_command == "reject":
                return store.build_update(
                    agent, args.request_id, "rejected",
                    wait_reason=args.reason)
            return store.build_wait(
                agent, args.request_id, args.reason, args.seconds)
        if command == "runtime":
            if args.runtime_command == "register":
                return store.runtime_register(
                    agent, args.runtime_id, cohort=args.cohort,
                    issue_id=args.issue, generation=args.generation,
                    dll_path=args.dll, dll_sha256=args.dll_sha,
                    run_id=args.run_id, server_pid=args.server_pid,
                    client_pids=args.client_pids,
                    server_journal=args.server_journal,
                    client_journals=args.client_journals, state=args.state)
            if args.runtime_command == "list":
                return store.runtime_list(agent=agent, state=args.state)
            return store.runtime_update(agent, args.runtime_id, args.state)
        if command == "conflict":
            if args.conflict_command == "list":
                return store.conflict_list(open_only=not args.all)
            return store.review_text(args.conflict_id)
        if command == "option":
            return store.option_add(agent, args.conflict_id, args.title,
                                    args.tradeoffs, args.patch)
        if command == "vote":
            return store.vote(agent, args.conflict_id, args.option_id,
                              args.rationale)
        if command == "decision":
            if args.decision_command == "list":
                return store.decisions(args.limit)
            return store.decision_record(
                agent, args.task_id, args.chosen, conflict_id=args.conflict,
                rationale=args.rationale, tradeoffs=args.tradeoffs,
                rejected=args.rejected, affected_paths=args.paths,
                dissent=args.dissent, supersedes=args.supersedes)
        if command == "shared":
            if args.shared_command == "begin":
                regions = _load_regions(args.regions)
                return store.shared_begin(agent, args.path, args.with_agents, regions)
            if args.shared_command == "accept":
                return store.shared_accept(agent, args.shared_id)
            return store.shared_end(agent, args.shared_id)
        if command == "edit":
            if args.edit_command == "begin":
                return store.edit_begin(
                    agent, args.path, region=args.region, known_hash=args.hash,
                    minutes=args.minutes, task_id=args.task)
            if args.edit_command == "propose":
                return store.edit_propose(
                    agent, args.path, args.region, args.patch, args.summary)
            if args.edit_command == "commit":
                return store.edit_commit(agent, args.path, new_hash=args.hash)
            return store.edit_abort(agent, args.path)
        if command == "proposal":
            if args.proposal_command == "create":
                return store.proposal_create(agent, args.to, args.path, args.patch)
            if args.proposal_command == "show":
                return store.proposal_show(args.proposal_id)
            if args.proposal_command == "apply":
                return store.proposal_apply(agent, args.proposal_id)
            return store.proposal_reject(agent, args.proposal_id, args.reason)
        if command == "halt":
            return store.halt(agent, args.reason)
        if command == "resume":
            return store.resume(agent)
        if command == "freeze":
            return store.freeze(agent, args.path, args.reason)
        if command == "unfreeze":
            return store.unfreeze(agent, args.path)
        if command == "guard":
            result = store.guard(agent, args.staged, args.paths)
            if not result["ok"]:
                raise CoordError("coordination guard blocked: " +
                                 json.dumps(result["blocked"]), 2)
            return result
        raise CoordError(f"unknown command {command}", 2)
    finally:
        store.close()


def _normalize_global_options(argv: list[str]) -> list[str]:
    """Allow --agent/--json/--root after a subcommand without model-specific syntax."""
    values = list(argv)
    hoisted: list[str] = []
    remaining: list[str] = []
    index = 0
    while index < len(values):
        token = values[index]
        if token in ("--agent", "--root"):
            if index + 1 >= len(values):
                remaining.append(token)
                break
            hoisted.extend([token, values[index + 1]])
            index += 2
            continue
        if token == "--json":
            hoisted.append(token)
            index += 1
            continue
        remaining.append(token)
        index += 1
    return hoisted + remaining


def main(argv: list[str] | None = None) -> int:
    parser = create_parser()
    arguments = _normalize_global_options(
        list(sys.argv[1:] if argv is None else argv))
    args = parser.parse_args(arguments)
    try:
        result = run(args)
        _print(result, args.json)
        if args.command == "wake-check" and isinstance(result, dict):
            return int(result.get("exit_code") or 0)
        return 0
    except CoordError as error:
        print(f"[AGENT COORD] {error}", file=sys.stderr)
        return error.code
    except (OSError, ValueError, sqlite3.Error) as error:
        print(f"[AGENT COORD] {error}", file=sys.stderr)
        return 4


if __name__ == "__main__":
    raise SystemExit(main())

