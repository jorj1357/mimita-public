# Logging retention investigation

- Time: 2026-10-10 17:56:00 EDT
- Branch: current working branch (not changed)
- Commit: not changed
- Scope: investigation only; no gameplay or logging source/config changes made.
- Pre-existing working-tree edits: preserved; `git status --short` showed changes in config, devscripts, src, and prior changelog/doc files before this record.

## Evidence

- Read `docs/ROUTER.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/specs/debug-logging/canonical-jsonl.md`, `docs/skills/logging-checker-v1.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.
- `src/debug/structured-log.cpp:654-667` opens the canonical `events.jsonl` with append mode; `src/debug/structured-log.cpp:935-961` writes and flushes records but has no per-file size check, splitting, truncation, or total-retention pass.
- `src/debug/log-manager.cpp:79-120` only rotates up to 30 legacy `Server_`, `Gameterminal_`, `Client_`, or `Game_` files by count; it does not manage canonical `events.jsonl` files or enforce byte limits.
- The active journal `logs/2026-10-10/20261010_213512/events.jsonl` measured 274,422,433 bytes during inspection. The full `logs` tree measured 328,510,040 bytes at the earlier inventory point.
- Event counts in that journal showed `actor-hybrid.pose-input` about 143 MB, `client.npc-avatar.snapshot-reassembled` about 67 MB, and `AIMBODY_LIVE_SAMPLE` about 33 MB. Their owners are `src/ragdoll/ragdoll-mode.cpp:286-290`, `src/network/multiplayer-tick.cpp:1416-1425`, and `src/ragdoll/ragdoll-mode.cpp:286-290` respectively.
- `src/debug/structured-log.cpp:221-340` parses category/sampling/throttling settings but no retention/size settings. The active JSON config has no byte caps.

## Conclusion

The large file is explained by unbounded append-only canonical JSONL plus several high-volume important-level records. Existing rotation is a separate legacy path. A future fix should be owned by `StructuredLogger`/its writer, preserve complete JSON lines, rotate before crossing a 100 MB cap, and apply a startup/shutdown total-tree retention pass that never deletes the active run or protected crash evidence. Runtime validation is still required after implementation.

## Validation

- Source inspection: PASS.
- Build: NOT RUN; investigation-only.
- Runtime: existing live journal inspected; no new executable or scenario launched.
- Human acceptance: NOT APPLICABLE for investigation; implementation and retention behavior still need review.
