#!/usr/bin/env python3
"""Create and operate isolated MiMITA agent worktree lanes.

Run lane-management commands from the coordinator checkout. Each lane has its
own Git worktree, dev-loop daemon/state, build outputs, executable artifacts,
and runtime journals. Cleanup stops that lane's dev-loop but keeps its files
and evidence for later review.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import datetime
import time
import uuid
from pathlib import Path
from typing import Any

from dev_loop.source_identity import source_identity
from path_containment import (
    apply_temp_environment,
    resolve_within,
)


ROOT = Path(__file__).resolve().parents[1]
DEV_ROOT = ROOT / ".dev"
LANE_ROOT = DEV_ROOT / "agent-lanes"
REGISTRY = DEV_ROOT / "agent-lanes"
LANE_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_-]{0,47}$")


def git(args: list[str], cwd: Path = ROOT, *, check: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(["git", *args], cwd=cwd, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if check and result.returncode:
        raise RuntimeError(result.stderr.strip() or f"git {' '.join(args)} failed")
    return result


def lane_path(lane_id: str) -> Path:
    validate_lane_id(lane_id)
    return resolve_within(ROOT, LANE_ROOT / lane_id, label="lane worktree")


def validate_lane_id(lane_id: str) -> None:
    if not LANE_ID_RE.fullmatch(lane_id):
        raise ValueError("lane ID must be 1-48 letters, digits, '_' or '-' and start with a letter or digit")


def metadata_path(lane_id: str) -> Path:
    validate_lane_id(lane_id)
    return resolve_within(ROOT, REGISTRY / f"{lane_id}.json", label="lane metadata")


def working_copy_snapshot() -> str:
    """Create an immutable snapshot commit without changing the user's index."""
    DEV_ROOT.mkdir(parents=True, exist_ok=True)
    snapshot_index = resolve_within(
        ROOT, DEV_ROOT / f"agent-lane-snapshot-{uuid.uuid4().hex}.index",
        label="lane snapshot index")
    env = os.environ.copy()
    env["GIT_INDEX_FILE"] = str(snapshot_index)
    try:
        snapshot_index.unlink(missing_ok=True)
        subprocess.run(["git", "read-tree", "HEAD"], cwd=ROOT, env=env, check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        subprocess.run(["git", "add", "-A"], cwd=ROOT, env=env, check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        tree = subprocess.run(["git", "write-tree"], cwd=ROOT, env=env, check=True,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True).stdout.strip()
        parent = git(["rev-parse", "HEAD"]).stdout.strip()
        commit = subprocess.run(["git", "commit-tree", tree, "-p", parent,
                                 "-m", "Agent lane baseline snapshot"], cwd=ROOT,
                                env=env, check=True, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, text=True).stdout.strip()
        return commit
    finally:
        snapshot_index.unlink(missing_ok=True)


def read_metadata(lane_id: str) -> dict[str, Any]:
    path = metadata_path(lane_id)
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as error:
        raise RuntimeError(f"unknown lane {lane_id!r}; create it first") from error
    if not isinstance(data, dict) or data.get("lane_id") != lane_id:
        raise RuntimeError(f"invalid lane metadata: {path}")
    worktree = resolve_within(
        ROOT, Path(str(data.get("worktree", ""))), label="lane worktree metadata")
    if worktree != lane_path(lane_id):
        raise RuntimeError(f"lane path mismatch in {path}; refusing to operate")
    return data


def write_metadata(data: dict[str, Any]) -> None:
    path = metadata_path(str(data["lane_id"]))
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def prepare_submodules(path: Path) -> None:
    for relative in ("external/libjuice", "external/manifold", "external/recastnavigation"):
        lane_submodule = path / relative
        if lane_submodule.is_dir() and any(lane_submodule.iterdir()):
            continue
        source_submodule = ROOT / relative
        expected_line = git(["ls-tree", "HEAD", "--", relative], path).stdout.strip()
        expected = expected_line.split()[2] if expected_line else ""
        actual = git(["rev-parse", "HEAD"], source_submodule,
                     check=False).stdout.strip() if source_submodule.is_dir() else ""
        if expected and expected == actual:
            # These are build inputs only. Copy them without their .git pointer
            # so agents/builds cannot change another checkout's dependency tree.
            shutil.copytree(source_submodule, lane_submodule,
                            ignore=shutil.ignore_patterns(".git"), dirs_exist_ok=True)
            continue
        initialized = git(["submodule", "update", "--init", "--", relative], path,
                          check=False)
        if initialized.returncode:
            raise RuntimeError(f"could not prepare {relative} at {path}: "
                               f"{initialized.stderr.strip()}")


def run(command: list[str], cwd: Path, *, timeout: float | None = None,
        capture: bool = False, env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, text=True,
                          stdout=subprocess.PIPE if capture else None,
                          stderr=subprocess.PIPE if capture else None,
                          timeout=timeout, env=env)


def devloop(path: Path, *args: str, capture: bool = False,
            timeout: float | None = None,
            env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    merged = os.environ.copy()
    merged["MIMITA_LANE_ID"] = path.name
    merged["MIMITA_COORDINATOR_ROOT"] = str(ROOT.resolve())
    apply_temp_environment(ROOT, path.name, merged)
    if env:
        merged.update(env)
    return run([sys.executable, "devscripts/dev-loop.py", *args], path,
               capture=capture, timeout=timeout, env=merged)


# Dev-loop tooling is shared by every checkout but not snapshotted after lane
# creation. Keep the lane's copy in sync so a coordinator tooling change (for
# example the new --no-dashboard flag) does not make lane commands fail.
TOOLING_FILES = (
    "devscripts/dev-loop.py",
    "devscripts/dev-loop-watch.py",
)
TOOLING_PACKAGE = "devscripts/dev_loop"


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def tooling_paths(root: Path) -> list[Path]:
    paths = [root / relative for relative in TOOLING_FILES]
    package = root / TOOLING_PACKAGE
    if package.is_dir():
        paths.extend(sorted(package.glob("*.py")))
    return [path for path in paths if path.is_file()]


def sync_lane_tooling(path: Path) -> list[str]:
    """Copy changed coordinator dev-loop tooling into a lane worktree."""
    lane_root = resolve_within(ROOT, path, label="lane worktree", must_exist=True)
    changed: list[str] = []
    for source in tooling_paths(ROOT):
        relative = source.relative_to(ROOT)
        destination = resolve_within(
            lane_root, lane_root / relative,
            label=f"lane tooling {relative.as_posix()}")
        if destination.is_file() and _sha256(source) == _sha256(destination):
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        changed.append(relative.as_posix())
    return changed


def lane_run_is_live(path: Path) -> bool:
    result = devloop(path, "--status", "--json", capture=True, timeout=30)
    if result.returncode:
        return False
    try:
        return bool(json.loads(result.stdout).get("run_alive"))
    except (ValueError, AttributeError):
        return False


def prepare_lane_tooling(lane_id: str, data: dict[str, Any], path: Path) -> list[str]:
    """Sync lane dev-loop tooling and restart the daemon when it is safe."""
    changed = sync_lane_tooling(path)
    if not changed:
        return changed
    data["tooling_synced"] = changed
    data["tooling_source_revision"] = git(["rev-parse", "HEAD"]).stdout.strip()
    data["tooling_synced_utc"] = datetime.datetime.now(
        datetime.timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")
    write_metadata(data)
    print("tooling_synced=" + ",".join(changed))
    if any(relative.endswith("dev-loop.py") or
           "/dev_loop/" in relative.replace("\\", "/") for relative in changed):
        if lane_run_is_live(path):
            # A live game pair must not be replaced by tooling alone. The
            # watcher defers the daemon replacement until the run ends.
            print("tooling_restart=deferred (live run present)")
        else:
            devloop(path, "--request", "stop", "--json", capture=True, timeout=60)
            # Let the daemon release its lock before ensure starts the new code.
            time.sleep(1.0)
    return changed


def print_identity(lane_id: str, path: Path, identity: dict[str, Any]) -> None:
    branch = git(["branch", "--show-current"], path).stdout.strip()
    print(f"lane={lane_id} branch={branch or '(detached)'} revision={identity['revision']} "
          f"dirty={str(identity['dirty']).lower()} source_sha256={identity['worktree_sha256']}")


def create_lane(args: argparse.Namespace) -> int:
    lane_id = args.lane_id
    validate_lane_id(lane_id)
    path = lane_path(lane_id)
    lane_root = resolve_within(ROOT, LANE_ROOT, label="lane root")
    meta_file = metadata_path(lane_id)
    base = args.base or working_copy_snapshot()
    if not git(["rev-parse", "--verify", f"{base}^{{commit}}"], check=False).stdout.strip():
        raise RuntimeError(f"base revision does not resolve to a commit: {base}")
    branch = args.branch or f"codex/agent-{lane_id}"
    if path.exists() or meta_file.exists():
        if not (path.is_dir() and meta_file.is_file()):
            raise RuntimeError(f"partial lane state exists at {path}; inspect it before retrying")
        data = read_metadata(lane_id)
        if data.get("status") != "initializing" or data.get("branch") != branch:
            raise RuntimeError(f"lane {lane_id!r} already exists at {path}")
        print(f"resuming lane setup at {path}")
    else:
        lane_root.mkdir(parents=True, exist_ok=True)
        result = git(["worktree", "add", "-b", branch, str(path), base], check=False)
        if result.returncode:
            raise RuntimeError(result.stderr.strip() or "git worktree add failed")
        initial_revision = git(["rev-parse", "HEAD"], path).stdout.strip()
        data = {
            "schema_version": 1,
            "lane_id": lane_id,
            "branch": branch,
            "base": base,
            "worktree": str(path),
            "created_from_revision": initial_revision,
            "source_revision": initial_revision,
            "status": "initializing",
        }
        write_metadata(data)
    lane_manifold_entry = git(["ls-tree", "HEAD", "--", "external/manifold"], path).stdout.strip()
    lane_manifold_revision = lane_manifold_entry.split()[2] if lane_manifold_entry else ""
    cached_manifold_revision = git(["rev-parse", "HEAD"], ROOT / "external" / "manifold",
                                   check=False).stdout.strip()
    prepare_submodules(path)
    cached_manifold = ROOT / "external" / "manifold-prebuilt"
    lane_manifold = path / "external" / "manifold-prebuilt"
    if (cached_manifold.is_dir() and not lane_manifold.exists()
            and lane_manifold_revision == cached_manifold_revision):
        shutil.copytree(cached_manifold, lane_manifold)
    identity = source_identity(path)
    data.update(created_from_revision=identity["revision"], source_revision=identity["revision"],
                status="ready", submodules_initialized=True,
                manifold_prebuilt_copied=lane_manifold.is_dir())
    write_metadata(data)
    print_identity(lane_id, path, identity)
    print(f"worktree={path}")
    print("dependencies=required source submodules isolated; local Manifold prebuilt copied when available")
    print(f"lane created from recorded coordinator snapshot {base}")
    return 0


def status_lane(args: argparse.Namespace) -> int:
    data = read_metadata(args.lane_id)
    path = lane_path(args.lane_id)
    if not path.is_dir():
        print(f"lane={args.lane_id} status=missing worktree={path}")
        return 2
    identity = source_identity(path)
    data["source_revision"] = identity["revision"]
    data["status"] = "dirty" if identity["dirty"] else "ready"
    write_metadata(data)
    print_identity(args.lane_id, path, identity)
    print(f"worktree={path} status={data['status']}")
    current = devloop(path, "--status", "--json", capture=True, timeout=30)
    if current.returncode == 0:
        try:
            state = json.loads(current.stdout)
            build = state.get("build_identity") or {}
            build_source = build.get("source_at_build_start") or {}
            print(f"build_id={build.get('build_id') or '-'} executable={build.get('path') or '-'}")
            print(f"build_source_revision={build_source.get('revision') or '-'} "
                  f"dirty={build_source.get('dirty', '-')} "
                  f"changed_during_build={build.get('source_changed_during_build', '-')} "
                  f"source_sha256={build_source.get('worktree_sha256') or '-'}")
            print(f"run_id={state.get('run_id') or '-'} journal={state.get('events_path') or '-'}")
            for instance in state.get("instances", []):
                print(f"instance={instance.get('instance_id')} role={instance.get('role')} "
                      f"pid={instance.get('pid')} journal={instance.get('journal_path') or '-'}")
        except (ValueError, AttributeError):
            print(current.stdout.strip())
    else:
        print("dev_loop=not_started")
    return 0


def build_lane(args: argparse.Namespace) -> int:
    print("[AGENT LANE] agents must not build. The lane dev-loop auto-detects "
          "edits. Use `ensure` to start/verify it and `status` to read progress.",
          file=sys.stderr)
    return 2


def ensure_lane(args: argparse.Namespace) -> int:
    data = read_metadata(args.lane_id)
    path = lane_path(args.lane_id)
    if not path.is_dir():
        raise RuntimeError(f"lane worktree is missing: {path}")
    identity = source_identity(path)
    print_identity(args.lane_id, path, identity)
    data.update(source_revision=identity["revision"], status="ready")
    write_metadata(data)
    prepare_lane_tooling(args.lane_id, data, path)
    # The coordinator checkout owns the one visible dashboard. Lane commands
    # ensure only their own watchdog/daemon and never open another window.
    result = devloop(path, "--ensure-running", "--no-dashboard",
                     capture=True, timeout=args.timeout)
    if result.returncode:
        if result.stdout:
            print(result.stdout.strip())
        if result.stderr:
            print(result.stderr.strip(), file=sys.stderr)
        return result.returncode
    status_lane(argparse.Namespace(lane_id=args.lane_id))
    print("build_policy=dev-loop-only; no agent build was requested")
    return 0


def run_lane(args: argparse.Namespace) -> int:
    data = read_metadata(args.lane_id)
    path = lane_path(args.lane_id)
    identity = source_identity(path)
    print_identity(args.lane_id, path, identity)
    data.update(source_revision=identity["revision"], source_dirty=identity["dirty"],
                source_worktree_sha256=identity["worktree_sha256"])
    write_metadata(data)
    prepare_lane_tooling(args.lane_id, data, path)
    result = devloop(path, "--request", "launch", "--no-dashboard",
                     "--launch-mode", args.launch_mode,
                     "--profile", args.profile)
    if result.returncode:
        return result.returncode
    status_lane(argparse.Namespace(lane_id=args.lane_id))
    return 0


def sync_lane(args: argparse.Namespace) -> int:
    data = read_metadata(args.lane_id)
    path = lane_path(args.lane_id)
    print_identity(args.lane_id, path, source_identity(path))
    changed = prepare_lane_tooling(args.lane_id, data, path)
    if not changed:
        print("tooling_synced=none")
    return 0


def changed_files(path: Path, base: str) -> list[str]:
    result = git(["diff", "--name-only", "--diff-filter=ACMR", f"{base}...HEAD"], path)
    files = set(filter(None, result.stdout.splitlines()))
    unstaged = git(["diff", "--name-only", "--diff-filter=ACMR"], path)
    files.update(filter(None, unstaged.stdout.splitlines()))
    untracked = git(["ls-files", "--others", "--exclude-standard"], path)
    files.update(filter(None, untracked.stdout.splitlines()))
    return sorted(files)


def check_lane(args: argparse.Namespace) -> int:
    path = lane_path(args.lane_id)
    data = read_metadata(args.lane_id)
    identity = source_identity(path)
    print_identity(args.lane_id, path, identity)
    base = args.base or str(data.get("base") or "HEAD")
    files = changed_files(path, base)
    print(f"check_base={base} changed_files={len(files)}")
    for name in files:
        print(f"changed={name}")
    if not files:
        print("checks=none (no changed or untracked files)")
        return 0

    failures = 0
    python_files = [name for name in files if name.endswith(".py")]
    if python_files:
        print("check=python-syntax")
        result = run([sys.executable, "-m", "py_compile", *python_files], path)
        failures += result.returncode != 0

    config_files = [name for name in files if name.startswith("config/")
                    and (name.endswith(".json") or name.endswith(".jsonc"))]
    if config_files:
        print("check=changed-config-jsonc")
        result = run([sys.executable, "-c",
                      "import sys; from jsonc import load; [load(p) for p in sys.argv[1:]]",
                      *config_files], path)
        failures += result.returncode != 0

    scenario_files = [name for name in files if name.startswith("devscripts/scenarios/")
                      and name.endswith(".json")]
    if scenario_files:
        print("check=changed-scenario-json")
        result = run([sys.executable, "-c",
                      "import json,sys; [json.load(open(p, encoding='utf-8')) for p in sys.argv[1:]]",
                      *scenario_files], path)
        failures += result.returncode != 0

    cpp_files = [name for name in files if name.startswith("src/")
                 and Path(name).suffix.lower() in {".cpp", ".cc", ".c", ".h", ".hpp"}]
    if cpp_files:
        print("check=dev-loop-build-status")
        status_lane(argparse.Namespace(lane_id=args.lane_id))
        print("compile_evidence=read from the lane dev-loop; agent check does not build")
        print("runtime=not_run (launch and exercise the real behavior separately)")

    if not (python_files or config_files or scenario_files or cpp_files):
        print("checks=none (changed files have no automatic focused check; review routed docs/owner checks)")
    print(f"checks_result={'FAIL' if failures else 'PASS'}")
    status_lane(argparse.Namespace(lane_id=args.lane_id))
    return 1 if failures else 0


def list_lanes(_args: argparse.Namespace) -> int:
    if not REGISTRY.is_dir():
        print("lanes=none")
        return 0
    lane_root = resolve_within(ROOT, LANE_ROOT, label="lane root")
    count = 0
    for path in sorted(REGISTRY.glob("*.json")):
        try:
            path = resolve_within(ROOT, path, label="lane metadata", must_exist=True)
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError, RuntimeError):
            continue
        lane_id = str(data.get("lane_id", ""))
        try:
            validate_lane_id(lane_id)
            worktree = resolve_within(
                lane_root, Path(str(data.get("worktree", ""))),
                label="lane worktree")
        except (ValueError, RuntimeError):
            print(f"lane={lane_id or path.stem} status=outside_repo skipped "
                  f"worktree={data.get('worktree', '')}")
            continue
        lock = worktree / ".dev" / "dev-loop.lock"
        try:
            daemon_pid = int(lock.read_text(encoding="utf-8").strip())
        except (OSError, ValueError):
            daemon_pid = 0
        alive = False
        if worktree.is_dir():
            try:
                status = devloop(worktree, "--status", "--json",
                                 capture=True, timeout=10)
                alive = status.returncode == 0
            except (OSError, subprocess.SubprocessError):
                alive = False
        print(f"lane={lane_id} status={data.get('status', '')} "
              f"daemon_pid={daemon_pid or '-'} alive={str(alive).lower()} "
              f"worktree={worktree}")
        count += 1
    print(f"lanes={count}")
    return 0


def cleanup_lane(args: argparse.Namespace) -> int:
    data = read_metadata(args.lane_id)
    path = lane_path(args.lane_id)
    print_identity(args.lane_id, path, source_identity(path))
    status_lane(argparse.Namespace(lane_id=args.lane_id))
    result = devloop(path, "--request", "stop", "--json", capture=True, timeout=60)
    if result.returncode:
        # A lane with no daemon is already clean. Preserve the worktree and its
        # journals/artifacts; destructive removal is deliberately not implicit.
        print("dev_loop=not_running or stop_failed")
        if result.stdout:
            print(result.stdout.strip())
        if result.stderr:
            print(result.stderr.strip(), file=sys.stderr)
        return 0 if "no current run state" in (result.stderr or "") else result.returncode
    data["status"] = "cleaned"
    write_metadata(data)
    print("lane processes stopped; worktree, build artifacts, and journals preserved")
    return 0


def promote(args: argparse.Namespace) -> int:
    """Create a review integration worktree containing explicitly selected commits."""
    lane_id = args.lane_id
    validate_lane_id(lane_id)
    if not args.commit:
        raise ValueError("promote requires at least one reviewed --commit")
    path = lane_path(lane_id)
    if path.exists() or metadata_path(lane_id).exists():
        raise RuntimeError(f"integration lane {lane_id!r} already exists")
    base = args.base
    resolved_base = git(["rev-parse", "--verify", f"{base}^{{commit}}"], check=False).stdout.strip()
    if not resolved_base:
        raise RuntimeError(f"base revision does not resolve to a commit: {base}")
    lane_root = resolve_within(ROOT, LANE_ROOT, label="lane root")
    lane_root.mkdir(parents=True, exist_ok=True)
    branch = args.branch or f"codex/integration-{lane_id}"
    result = git(["worktree", "add", "-b", branch, str(path), resolved_base], check=False)
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or "could not create integration worktree")
    try:
        meta = {"schema_version": 1, "lane_id": lane_id, "branch": branch,
                "base": resolved_base, "worktree": str(path),
                "created_from_revision": resolved_base,
                "source_revision": source_identity(path)["revision"],
                "status": "integration-review"}
        write_metadata(meta)
        for commit in args.commit:
            resolved = git(["rev-parse", "--verify", f"{commit}^{{commit}}"], check=False).stdout.strip()
            if not resolved:
                raise RuntimeError(f"commit does not resolve: {commit}")
            cherry = git(["cherry-pick", resolved], path, check=False)
            if cherry.returncode:
                raise RuntimeError(f"cherry-pick conflict at {resolved}; resolve in {path}, then build manually")
        identity = source_identity(path)
        meta.update(source_revision=identity["revision"], status="integration-review")
        write_metadata(meta)
        print_identity(lane_id, path, identity)
        print(f"integration_worktree={path}")
        print("review combined diff, then run agent-lane.py build " + lane_id)
        if args.build:
            result = build_lane(argparse.Namespace(lane_id=lane_id, mode="debug",
                                                   timeout=args.timeout, jobs=args.jobs))
            if result:
                return result
            return publish_stable(lane_id, path)
        return 0
    except Exception:
        # Keep a conflicted integration checkout so the user can resolve it.
        raise


def publish_stable(lane_id: str, path: Path) -> int:
    """Promote a verified immutable integration artifact to the stable pointer."""
    state_path = resolve_within(
        path, path / ".dev" / "state.json", label="lane state", must_exist=True)
    try:
        state = json.loads(state_path.read_text(encoding="utf-8"))
        folder = str(state["latest_artifact_folder"])
    except (OSError, KeyError, TypeError, json.JSONDecodeError) as error:
        raise RuntimeError(f"cannot identify the integration build in {state_path}") from error
    artifact = resolve_within(
        path, path / ".dev" / "builds" / folder,
        label="lane build artifact", must_exist=True)
    metadata_path = resolve_within(
        artifact, artifact / "build-identity.json",
        label="artifact identity", must_exist=True)
    try:
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    except (OSError, ValueError, json.JSONDecodeError) as error:
        raise RuntimeError(f"missing or invalid artifact identity: {metadata_path}") from error
    start = metadata.get("source_at_build_start") or {}
    end = metadata.get("source_at_publish") or {}
    if not metadata.get("source_changed_during_build") is False or start != end:
        raise RuntimeError("integration source changed during the build; rebuild before stable promotion")
    if start.get("dirty"):
        raise RuntimeError("integration worktree is dirty; commit the reviewed integration before promotion")
    executable = resolve_within(
        artifact, artifact / "mimita.exe",
        label="integration executable", must_exist=True)

    stable_root = resolve_within(
        ROOT, DEV_ROOT / "stable-builds", label="stable build root")
    stable_root.mkdir(parents=True, exist_ok=True)
    stable_artifact = resolve_within(
        ROOT, stable_root / folder, label="stable artifact")
    if stable_artifact.exists():
        raise RuntimeError(f"stable artifact already exists; refusing to replace it: {stable_artifact}")
    shutil.copytree(artifact, stable_artifact)
    pointer = {
        "schema_version": 1,
        "promoted_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(
            timespec="seconds").replace("+00:00", "Z"),
        "lane_id": lane_id,
        "branch": git(["branch", "--show-current"], path).stdout.strip(),
        "source_revision": start.get("revision"),
        "source_worktree_sha256": start.get("worktree_sha256"),
        "build_id": metadata.get("build_id"),
        "artifact_id": metadata.get("artifact_id"),
        "executable": str(stable_artifact / "mimita.exe"),
        "artifact": str(stable_artifact),
    }
    stable_pointer = resolve_within(
        ROOT, DEV_ROOT / "stable-build.json", label="stable build pointer")
    temporary = stable_pointer.with_name(stable_pointer.name + ".tmp")
    temporary.write_text(json.dumps(pointer, indent=2) + "\n", encoding="utf-8")
    os.replace(temporary, stable_pointer)
    print(f"stable_build_id={pointer['build_id']}")
    print(f"stable_executable={pointer['executable']}")
    print(f"stable_manifest={stable_pointer}")
    return 0


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Manage isolated MiMITA agent worktree lanes")
    sub = p.add_subparsers(dest="action", required=True)
    create = sub.add_parser("create", help="create a lane worktree and branch")
    create.add_argument("lane_id"); create.add_argument("--base", default="")
    create.add_argument("--branch", default=""); create.set_defaults(func=create_lane)
    status = sub.add_parser("status", help="show source, build, run, and journal identities")
    status.add_argument("lane_id"); status.set_defaults(func=status_lane)
    build = sub.add_parser("build", help="deprecated: agents must not build")
    build.add_argument("lane_id"); build.add_argument("--mode", default="debug")
    build.add_argument("--jobs", type=int, default=4); build.add_argument("--timeout", type=float, default=60)
    build.set_defaults(func=build_lane)
    ensure = sub.add_parser("ensure", help="ensure this lane's dev-loop auto-builder is running")
    ensure.add_argument("lane_id"); ensure.add_argument("--timeout", type=float, default=120)
    ensure.set_defaults(func=ensure_lane)
    listing = sub.add_parser("list", help="list known agent lanes and dev-loop liveness")
    listing.set_defaults(func=list_lanes)
    runp = sub.add_parser("run", help="launch a build through this lane's dev-loop")
    runp.add_argument("lane_id"); runp.add_argument("--launch-mode", required=True)
    runp.add_argument("--profile", default="npc-navigation"); runp.set_defaults(func=run_lane)
    syncp = sub.add_parser(
        "sync", help="copy current checker dev-loop tooling into a lane")
    syncp.add_argument("lane_id"); syncp.set_defaults(func=sync_lane)
    check = sub.add_parser("check", help="run focused checks selected from changed files")
    check.add_argument("lane_id"); check.add_argument("--base", default="")
    check.add_argument("--jobs", type=int, default=1)
    check.add_argument("--timeout", type=float, default=36000); check.set_defaults(func=check_lane)
    cleanup = sub.add_parser("cleanup", help="stop this lane's dev-loop and preserve its worktree/evidence")
    cleanup.add_argument("lane_id"); cleanup.set_defaults(func=cleanup_lane)
    promote_p = sub.add_parser("promote", help="assemble reviewed commits in an integration worktree")
    promote_p.add_argument("lane_id"); promote_p.add_argument("--base", required=True)
    promote_p.add_argument("--commit", action="append", default=[])
    promote_p.add_argument("--branch", default=""); promote_p.add_argument("--build", action="store_true")
    promote_p.add_argument("--jobs", type=int, default=1)
    promote_p.add_argument("--timeout", type=float, default=36000); promote_p.set_defaults(func=promote)
    return p


def main() -> int:
    args = parser().parse_args()
    try:
        return int(args.func(args))
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        print(f"[AGENT LANE] {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
