# Segmented JSONL retention contract documented

- Time: 2026-10-10 18:27:10 EDT
- Branch and commit: not changed.
- Scope: documentation planning only; no source, configuration, build, executable, runtime, or log files changed.
- Pre-existing edits: preserved, including the other untracked changelog files already present in `docs/changelog/2026-10-10`.

## Changed document

- `docs/specs/debug-logging/debug-logging.md:106-358` now defines the concrete segmented-writer contract for the 2026-10-10 plan snapshot.
- The contract specifies numbered `events-000001.jsonl` segments, the exact 100,000,000-byte per-segment and 1,000,000,000-byte folder ceilings, a 99,000,000-byte rotation target, pre-write reservation, and oversized-record rejection.
- It maps the planned work to the current `StructuredLogger` ownership points in `src/debug/structured-log.cpp` and `src/debug/structured-log.h`, including `createLogDir`, `init`, `writeEvent`, `writeJsonLine`, `eventsFileMutex`, and current single-file state.
- It specifies the first safe cross-process algorithm, including the decision to open/write/flush/close under the named mutex until a measured lease protocol justifies a persistent-handle optimization.
- It defines oldest-segment quota cleanup, retention classes, bounded drop summaries, shared client/server run-directory discovery, reader behavior, and required real-runtime validation cases.

## Documents and review

- Read the routed logging, logging-checker, runtime-validation, time/formatting, and task-completion guidance, plus `docs/ROUTER.md`.
- The routed `docs/doc-review-09-03-2026.md` path does not exist in this checkout; no replacement document was invented.

## Validation

- Markdown diff check: PASS.
- Source-change check: PASS; no implementation files changed.
- Build: NOT RUN; documentation-only change.
- Runtime: NOT RUN; no implementation was authorized or attempted.
- Human review still needed: approve the segmented filename/reader contract, the bounded protected-retention policy, and the correctness-first open/write/close approach before implementation.
