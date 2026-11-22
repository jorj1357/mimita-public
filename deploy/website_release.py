"""Build and verify a website-only release package for mimita.fun.

This tool never contacts the VPS and never changes production. It validates an
exact pushed commit, rejects out-of-scope changes, optionally runs local
website checks, and produces a content-addressed tar.gz plus manifest for the
documented release path in docs/operations/website-agent-deployment.md.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import subprocess
import sys
import tarfile
import time
from pathlib import Path
from typing import Iterable, Sequence

RUNTIME_PATHS = ("website", "content/articles")
ALLOWED_PREFIXES = ("website/", "content/articles/")
ALLOWED_SERVICE_FILES = (
    "deploy/mimita-postgres-watchdog.sh",
    "deploy/mimita-postgres-watchdog.service",
    "deploy/mimita-postgres-watchdog.timer",
    "deploy/vps_storage_config.py",
    "deploy/journald-mimita-storage.conf",
    "deploy/nginx-mimita-site.conf",
)
FORBIDDEN_PREFIXES = (
    "website/node_modules/",
    "website/dist/",
    "website/backups/",
)
FORBIDDEN_SUFFIXES = (".pem", ".key", ".p12", ".pfx")
MIGRATION_PREFIXES = ("website/server/migrations/",)
MAX_MANIFEST_BYTES = 2 * 1024 * 1024


class ReleaseError(Exception):
    pass


def normalize(path: str) -> str:
    return path.replace("\\", "/").lstrip("./")


def is_forbidden(path: str) -> bool:
    p = normalize(path)
    if p.startswith(FORBIDDEN_PREFIXES):
        return True
    if p.endswith(FORBIDDEN_SUFFIXES):
        return True
    for part in p.split("/"):
        if part == "node_modules":
            return True
        if part == ".env":
            return True
        if part.startswith(".env.") and part != ".env.example":
            return True
    if p.startswith(".git/") or p == ".git":
        return True
    return False


def is_migration(path: str) -> bool:
    return normalize(path).startswith(MIGRATION_PREFIXES)


def is_runtime_path(path: str) -> bool:
    return normalize(path).startswith(ALLOWED_PREFIXES)


def is_allowed(path: str) -> bool:
    p = normalize(path)
    if is_forbidden(p):
        return False
    return p in ALLOWED_SERVICE_FILES or p.startswith(ALLOWED_PREFIXES)


def validate_paths(paths: Iterable[str]) -> dict:
    changed = sorted({normalize(p) for p in paths if p})
    forbidden = [p for p in changed if is_forbidden(p)]
    migrations = [p for p in changed if is_migration(p)]
    runtime = [p for p in changed if is_runtime_path(p) and not is_forbidden(p)]
    service = [p for p in changed if p in ALLOWED_SERVICE_FILES]
    outside = [p for p in changed if not is_allowed(p)]
    if forbidden:
        raise ReleaseError(
            "forbidden paths in release: " + ", ".join(forbidden)
        )
    if migrations:
        raise ReleaseError(
            "database migration requires separate human approval: "
            + ", ".join(migrations)
        )
    if outside:
        raise ReleaseError(
            "paths outside the website allowlist: " + ", ".join(outside)
        )
    return {
        "changed": changed,
        "runtime": runtime,
        "service_files": service,
    }


def run(command: Sequence[str], cwd: Path, timeout: int = 900) -> str:
    try:
        result = subprocess.run(
            command,
            cwd=cwd,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False,
        )
    except FileNotFoundError as exc:
        raise ReleaseError(f"missing command: {command[0]}") from exc
    if result.returncode != 0:
        detail = (result.stderr or result.stdout or "").strip()
        raise ReleaseError(
            f"command failed ({result.returncode}): {' '.join(command)}\n{detail}"
        )
    return result.stdout


def require_commit(repo: Path, sha: str) -> str:
    full = run(["git", "rev-parse", "--verify", f"{sha}^{{commit}}"], repo).strip()
    return full


def changed_paths(repo: Path, from_sha: str, to_sha: str) -> list[str]:
    output = run(
        ["git", "diff", "--name-only", "-z", f"{from_sha}..{to_sha}"], repo
    )
    return [p for p in output.split("\0") if p]


def require_pushed(repo: Path, sha: str, branch: str, fetch: bool) -> None:
    if fetch:
        run(["git", "fetch", "--quiet", "origin", branch], repo, timeout=180)
    remote_ref = f"origin/{branch}"
    run(["git", "rev-parse", "--verify", f"{remote_ref}^{{commit}}"], repo)
    result = subprocess.run(
        ["git", "merge-base", "--is-ancestor", sha, remote_ref],
        cwd=repo,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise ReleaseError(
            f"commit {sha} is not reachable from pushed ref {remote_ref}"
        )


def run_checks(repo: Path) -> dict:
    website = repo / "website"
    if not website.is_dir():
        raise ReleaseError(f"missing website directory: {website}")
    checks = {}
    for name, command in (("test", "npm test"), ("build", "npm run build")):
        started = time.time()
        result = subprocess.run(
            command,
            cwd=website,
            shell=True,
            capture_output=True,
            text=True,
            timeout=1800,
            check=False,
        )
        checks[name] = {
            "command": command,
            "returncode": result.returncode,
            "duration_seconds": round(time.time() - started, 2),
        }
        if result.returncode != 0:
            tail = (result.stdout + result.stderr).strip().splitlines()[-20:]
            raise ReleaseError(
                f"local check failed: {command}\n" + "\n".join(tail)
            )
    return checks


def create_archive(repo: Path, sha: str, output: Path, paths: Sequence[str]) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    run(
        [
            "git",
            "archive",
            f"--format=tar.gz",
            f"--output={output}",
            sha,
            *paths,
        ],
        repo,
    )


def build_manifest(archive: Path, sha: str, source: dict) -> dict:
    entries = []
    with tarfile.open(archive, "r:gz") as tar:
        for member in tar:
            name = normalize(member.name)
            if member.isdir():
                continue
            if not member.isfile():
                raise ReleaseError(f"archive member is not a regular file: {name}")
            if not name.startswith(ALLOWED_PREFIXES):
                raise ReleaseError(f"archive member outside allowlist: {name}")
            if is_forbidden(name):
                raise ReleaseError(f"forbidden archive member: {name}")
            handle = tar.extractfile(member)
            if handle is None:
                raise ReleaseError(f"cannot read archive member: {name}")
            digest = hashlib.sha256(handle.read()).hexdigest()
            entries.append(
                {
                    "path": name,
                    "size": member.size,
                    "mode": oct(member.mode & 0o777),
                    "sha256": digest,
                }
            )
    if not entries:
        raise ReleaseError("archive is empty")
    archive_digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    return {
        "schema": 1,
        "commit": sha,
        "created_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "source": source,
        "archive": archive.name,
        "archive_sha256": archive_digest,
        "entries": entries,
    }


def write_manifest(manifest: dict, output: Path) -> None:
    output.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


def verify_manifest(archive: Path, manifest_path: Path) -> dict:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    actual = build_manifest(archive, manifest.get("commit", ""), manifest.get("source", {}))
    if actual["archive_sha256"] != manifest.get("archive_sha256"):
        raise ReleaseError("archive digest does not match manifest")
    expected = {entry["path"]: entry for entry in manifest.get("entries", [])}
    observed = {entry["path"]: entry for entry in actual["entries"]}
    missing = sorted(set(expected) - set(observed))
    extra = sorted(set(observed) - set(expected))
    if missing or extra:
        raise ReleaseError(
            f"archive entries differ from manifest; missing={missing} extra={extra}"
        )
    for path, entry in expected.items():
        if observed[path]["sha256"] != entry["sha256"]:
            raise ReleaseError(f"sha256 mismatch for {path}")
    return {
        "commit": manifest.get("commit"),
        "entries": len(actual["entries"]),
        "archive_sha256": actual["archive_sha256"],
        "verified": True,
    }


def package(args: argparse.Namespace) -> dict:
    repo = Path(args.repo).resolve()
    sha = require_commit(repo, args.to)
    from_sha = require_commit(repo, args.from_sha)
    require_pushed(repo, sha, args.branch, fetch=not args.no_fetch)
    diff = changed_paths(repo, from_sha, sha)
    classification = validate_paths(diff)
    runtime = classification["runtime"]
    if args.route == "website" and not runtime:
        raise ReleaseError("no website runtime files changed in this release")
    checks = {"skipped": True} if args.skip_checks else run_checks(repo)
    out_dir = Path(args.out).resolve()
    archive = out_dir / f"website-{sha[:12]}.tar.gz"
    created = {"from": from_sha, "to": sha}
    if args.route == "website":
        create_archive(repo, sha, archive, RUNTIME_PATHS)
        manifest = build_manifest(archive, sha, created)
    else:
        raise ReleaseError(f"unsupported route: {args.route}")
    manifest_path = out_dir / f"website-{sha[:12]}.manifest.json"
    write_manifest(manifest, manifest_path)
    verify_manifest(archive, manifest_path)
    return {
        "commit": sha,
        "from": from_sha,
        "branch": args.branch,
        "changed": len(classification["changed"]),
        "runtime": len(runtime),
        "service_files": classification["service_files"],
        "checks": checks,
        "archive": str(archive),
        "manifest": str(manifest_path),
        "archive_sha256": manifest["archive_sha256"],
        "entries": len(manifest["entries"]),
    }


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", default=".", help="repository root")
    sub = parser.add_subparsers(dest="command", required=True)

    validate = sub.add_parser("validate", help="check changed paths only")
    validate.add_argument("--from", dest="from_sha", required=True)
    validate.add_argument("--to", required=True)

    pkg = sub.add_parser("package", help="build a verified release archive")
    pkg.add_argument("--from", dest="from_sha", required=True)
    pkg.add_argument("--to", required=True)
    pkg.add_argument("--branch", default="")
    pkg.add_argument("--out", default=".dev/website-releases")
    pkg.add_argument("--route", choices=("website",), default="website")
    pkg.add_argument("--skip-checks", action="store_true")
    pkg.add_argument("--no-fetch", action="store_true")

    verify = sub.add_parser("verify", help="verify an archive against its manifest")
    verify.add_argument("--archive", required=True)
    verify.add_argument("--manifest", required=True)

    args = parser.parse_args(argv)
    try:
        if args.command == "validate":
            repo = Path(args.repo).resolve()
            from_sha = require_commit(repo, args.from_sha)
            to_sha = require_commit(repo, args.to)
            result = validate_paths(changed_paths(repo, from_sha, to_sha))
            result.update({"from": from_sha, "to": to_sha})
        elif args.command == "package":
            if not args.branch:
                args.branch = run(
                    ["git", "rev-parse", "--abbrev-ref", "HEAD"], Path(args.repo)
                ).strip()
            result = package(args)
        else:
            result = verify_manifest(Path(args.archive), Path(args.manifest))
    except ReleaseError as exc:
        print(json.dumps({"ok": False, "error": str(exc)}, indent=2), file=sys.stderr)
        return 2
    print(json.dumps({"ok": True, **result}, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
