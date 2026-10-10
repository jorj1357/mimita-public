# Canonical logging plan documented

- Time: 2026-10-10 18:04:48 EDT
- Branch and commit: not changed.
- Scope: documentation planning only; no source, build, executable, runtime, or log files changed.
- Pre-existing edits: preserved. The working tree already contained source/config/document changes and several untracked changelog files.

## Changed document

- `docs/specs/debug-logging/debug-logging.md:3-198` now contains the implementation plan for one canonical `logs/<date>/<run>/events.jsonl` diagnostic path.
- The plan defines exact decimal ceilings of 1,000,000,000 bytes for the full `logs` tree and 100,000,000 bytes per file, including concurrent client/server behavior, pre-write reservation, rotation, active-run protection, and quota exhaustion behavior.
- The plan identifies the current `StructuredLogger`, `LogManager`, dev-loop shared-path code, direct file writers, and known raw-output families with source file and line references.
- The plan adds a maintained noncanonical logging inventory template and requires repository-wide re-search and disposition before deletion or migration.

## Documents and review

- Read `docs/ROUTER.md`, `docs/skills/documentation-checker-v1.md`, `docs/specs/debug-logging/debug-logging.md`, and `docs/operations/task-completion/task-completion.md`.
- The routed `docs/doc-review-09-03-2026.md` path does not exist in this checkout; no replacement document was invented.
- Documentation Checker result: PASS for the requested document structure; TODO scan found pre-existing TODO notes elsewhere in `docs`, which were not changed.

## Validation

- Diff inspection: PASS.
- Build: NOT RUN; documentation-only change.
- Runtime: NOT RUN; no implementation was authorized or attempted.
- Human review still needed: approve the canonical segmented-JSONL layout, absolute quota behavior when no deletable files remain, and the disposition of crash/assertion evidence under the hard 1 GB limit.
