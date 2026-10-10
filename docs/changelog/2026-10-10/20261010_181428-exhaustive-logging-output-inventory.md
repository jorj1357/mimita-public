# Exhaustive logging output inventory documented

- Time: 2026-10-10 18:14:28 EDT
- Branch and commit: not changed.
- Scope: documentation planning only; no source, configuration, build, executable, runtime, or log files changed.
- Pre-existing edits: preserved, including the other untracked changelog files already present in `docs/changelog/2026-10-10`.

## Changed document

- `docs/specs/debug-logging/debug-logging.md:3-510` now contains the expanded Phase 3 and Phase 4 plan and direct-output inventory.
- The inventory names the canonical and noncanonical file sinks, legacy category/summary files, crash artifacts, benchmark/replay export logs, startup paths, visible server/client console creation, FFmpeg windows, stdout/stderr capture, and terminal/in-game scrollback.
- The document includes exact source file and line references for the repository-wide raw print-family and Python launcher/tool/test output snapshot taken on 2026-10-10.
- Phase 4 now requires an owner audit of every `Debug::*`, `LogManager::*`, and terminal-scrollback call to its final sink, so logging-looking APIs are not accepted without tracing.
- The inventory is a dated source snapshot. Contributors must re-run the searches after source edits because line numbers can move.

## Documents and review

- Read the routed logging, logging-checker, runtime-validation, time/formatting, and task-completion guidance, plus `docs/ROUTER.md`.
- The routed `docs/doc-review-09-03-2026.md` path does not exist in this checkout; no replacement document was invented.

## Validation

- Markdown diff check: PASS except for the pre-existing trailing whitespace/BOM on the original first line of `debug-logging.md`.
- Document line count after the update: 1,651.
- Build: NOT RUN; documentation-only change.
- Runtime: NOT RUN; no implementation was authorized or attempted.
- Human review still needed: confirm which intentional user-facing CLI, external-tool, crash, replay, and standalone test outputs may remain outside the canonical game diagnostic stream.
