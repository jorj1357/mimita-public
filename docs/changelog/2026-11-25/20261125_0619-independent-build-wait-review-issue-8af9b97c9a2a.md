// 2026-11-25 06:19 UTC
/* purpose
 * record one bounded build/test wait review activation
 * append review evidence to issue-8af9b97c9a2a (game-dll-build wait over budget)
 * append occurrences to the tracked identity/journal/dispatch/build-cost issues
 * no source, build, close, or fix claim by this session
 */

# Task

- Wake ID: wake-9c7b2299476c (automatic reviewer wake for an over-budget wait)
- Session ID: opencode MIMITA_AGENT_ID=build-wait-reviewer (session-08a5c5b087ae)
- Owner: build-wait-reviewer
- Focus: review issue-8af9b97c9a2a, wait game-dll-build:2026-11-25T05:52:56.306Z
  (stage game_build, elapsed 60.325s)
- Summary: The wait was real elapsed time on a healthy in-flight build, not a
  stall. It fired 60.325s into attempt
  game-dll-20261125T055256329120Z-9b2bcf (generation 640), whose single changed
  TU `src/network/multiplayer-projectiles.cpp` compiled for 95.926s and pushed
  the build to 131.437s total. That TU also dominated the two prior attempts at
  50.552s / 52.701s, so this occurrence is a ~1.9x compile-cost jump. The
  wait_id/stage/elapsed fields, one-issue clause, one-queued-wake clause, and
  reason clause pass; the build/run-identity clause fails (stale EXE build id,
  previous game generation, no run id) and the proof-expectation dev-loop wait
  event is absent from retained 2026-11-25 journals. Appended one issue review,
  set related_build/related_run, and recorded occurrences on four tracked
  issues.
- Status: bounded review complete; no code changes
- Date, time, timezone: 2026-11-25T06:19Z, ISO 8601
- Branch: (left as checked out by the shared dev-loop)
- Base commit: none recorded
- Final commit: none

# Pre-existing changes

- `git status --short` shows unrelated modified config, source, changelog, and
  issue files from other agents. None were attributed to, reverted, or edited by
  this session.
- `docs/issues/README.md` was already modified before this session and is not
  attributed to this session.
- Files appended by this session are listed below only.

# Requested behavior

- Review build/test wait issue-8af9b97c9a2a: inspect the issue and dev-loop
  journal, complete any missing reproduction/expected behavior/acceptance/paths/
  proof expectation, and append the review and evidence to the issue. If the
  wait recurs, add the occurrence instead of a duplicate issue. If a code fix is
  needed, report it with evidence and claim it separately. No source edit.

# Specification alignment

- `AGENTS.md` gate 0 (trace before blaming the binary) and gate 0c
  (append-only evidence, no duplicate issue, no fix claim without proof).
- `AGENTS.md` build-friction rule: record why a wait occurred; a wait above the
  measured budget creates a build-friction issue.
- `docs/specs/dev-loop/agent-coordination.md` evidence-gap handoff: append each
  new gap_key once to the best existing issue; do not create duplicate issues.

# Exact implementation changes

- No source or configuration changes.
- Added coordination/repository records only: one issue review on
  issue-8af9b97c9a2a, related_build/related_run, four occurrence suggestions on
  the tracked issues, and this final changelog.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-8af9b97c9a2a.md` | dev-loop issue record | Context + review appended | durable wait review evidence | remove appended context/review |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` | prior occurrences | one occurrence appended | identity/budget recurrence | remove appended suggestion |
| `docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md` | prior occurrences | one occurrence appended | missing dev-loop wait journal event | remove appended suggestion |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` | prior occurrences | one occurrence appended | codex-fallback dispatch latency/duplication | remove appended suggestion |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` | prior gaps | one occurrence appended | single-TU compile 1.9x cost jump | remove appended suggestion |
| `docs/changelog/2026-11-25/20261125_0619-independent-build-wait-review-issue-8af9b97c9a2a.md` | absent | added | one final session changelog | delete file |

# Validation

- `python devscripts/agent-coord.py begin`, `sync`, `observe`, `issue context`,
  `activity set`, `issue review`, `issue suggest`, and `end` completed with
  agent build-wait-reviewer.
- `python devscripts/dev-loop.py --status` was read only; no build, test run, or
  runtime was started or requested by this session.
- Read-only artifact checks: `.dev/build-timings/game-dll-20261125T055256329120Z-9b2bcf.{json,resource.json}`,
  the three 2026-11-25 attempt sidecars, `.dev/agent-wake/requests/wake-9c7b2299476c.json`,
  `.dev/agent-wake/events.jsonl`, `.dev/state.json`, and the 2026-11-25
  `logs/.../dev-loop/events.jsonl` set.

# Measured evidence

- Wait: `game-dll-build:2026-11-25T05:52:56.306Z`, elapsed 60.325s vs budget
  60.0s; request `2026-11-25T05:52:55.970Z-8c380861:mode3:game`;
  source_generation 9; issue opened 05:53:56.693Z.
- Attempt `.dev/build-timings/game-dll-20261125T055256329120Z-9b2bcf.json`:
  success, generation 640, started 05:52:56.329Z, completed 05:55:07.766Z,
  total_seconds 131.437 (worker 113.39); the build continued ~71.1s after the
  wait fired. changed_source_preflight 96.171s (compile of
  `src/network/multiplayer-projectiles.cpp` 95.926s), link 11.578s (lld
  11.375s), module-identity-provider compile 1.688s, publish 0.500s,
  object_cache_scan 0.016s; compiled 2, skipped 643, jobs 10.
- Same-TU compile history (read-only sidecar comparison): gen 638 919e8d
  52.701s (total 74.75s), gen 639 48e192 50.552s (total 69.625s), gen 640
  9b2bcf 95.926s (total 131.437s) — about 1.9x the two prior attempts.
- Sidecar 9b2bcf: status partial, 60 samples @1s, sampler thread did not stop
  before timeout; mean host CPU 95.45%, peak 100%, min available memory
  1,836,306,432 B (~1.71 GiB). Runtime alignment run 20261125_054920, server
  pid 13856 bloodtest (5 NPCs, 1 player), client pid 9676 ~35.95 fps (p95
  70.66ms, max 106.9ms, 36/36 frames over budget); client mode
  stale/unreported. Unavailable: matched_prebuild_resource_baseline,
  host_commit_charge_and_limit, client_mode_independent_confirmation.
- Wake dedup: exactly one `wake.queued` for this wait
  (`wake-9c7b2299476c`, 05:54:00.283Z) in `.dev/agent-wake/events.jsonl`.
- Dispatch: codex claimed/launched 05:54:03.964/05:54:04.006Z (pid 2104) then
  exited; retry to opencode 05:55:32.416Z; opencode claimed/launched
  06:12:40.263/06:12:40.279Z (pid 14452). Queue-to-final launch 1120.0s
  (~18m40s); wait-to-reviewer ~1123.6s; two `wake.launched` for one wake id.

# Evidence gaps

- Issue identity is wrong: evidence.build_id `2026-11-25T01:21:28.987918Z` is the
  last published EXE artifact and evidence.generation 639 is the previous
  published game generation; the waiting build was
  `game-dll-20261125T055256329120Z-9b2bcf` / generation 640. No run_id was
  carried (live run 20261125_054920). Owner path: dev-loop.py:3896-3897
  (`latest_build`; `game_build_generation` assigned only at dev-loop.py:3259).
  Tracked in issue-9a4503f6ed68.
- No `dev-loop.wait.over_budget` or `dev-loop.wait.reviewer-wake` row exists in
  any retained 2026-11-25 dev-loop journal; `.dev/state.json` has an empty
  `events_path` (daemon_pid 4044). Tracked in issue-6e2d7b91e7d9.
- Build cost: the gen 640 TU compile nearly doubled versus the two prior
  attempts, but sidecar 9b2bcf is partial and lacks a matched pre-build
  baseline. Whether the source change (note 1d3703603f8d) or CPU contention
  caused it is unknown. Tracked in issue-136d4553ff92.

# Runtime evidence separation

- Source: read-only inspection of the issue record, build-timings artifacts,
  the wake journal, and dev-loop status.
- Build evidence: the cited attempt compiled, linked, and published successfully.
- Runtime evidence: none claimed as behavior proof; the live pair is only
  concurrency context for the build window.
- Human acceptance: none by this session.

# Regression review

- Regression entry appended: no. This is a bounded review of a recurring
  build-friction class already tracked by issue-9a4503f6ed68, issue-136d4553ff92,
  and issue-84c584957e65, not a new regression.

# Risks and unknowns

- The flat 60s total-elapsed budget remains below normal incremental whole-game
  DLL cost for a single heavy TU on this host under live-pair CPU pressure, so
  healthy builds will keep tripping it.
- The gen 640 compile nearly doubling shows the single-TU cost can regress
  further, which makes the flat budget fire more often and for longer.
- `build_friction.record_wait_incident` mints one issue per wait_id even for
  identical kind/stage, the duplicate-noise class already logged on the umbrella
  issue.
- Whether host CPU (~95%) or the source change materially extended the TU
  compile is unproven; this sample does not establish causation.

# Next steps

- Identity owner: carry the active game-build attempt id/generation (and run_id
  when a pair is live) into wait evidence instead of `latest_build`.
- Budget/aggregation owner: use a per-kind or stall-based budget, or coalesce
  per (kind,stage), to stop the repeating issue class.
- Journal owner: guarantee the root dev-loop events path exists before recording
  wait events.
- Dispatch owner: keep primary-adapter failure from duplicating and delaying the
  reviewer launch beyond the 5-minute target.
- Build-cost owner: explain the gen 640 jump from ~50.5s to 95.9s for
  `src/network/multiplayer-projectiles.cpp` with a matched same-revision compile
  without a live pair or a revert comparison; also check the TU include fan-out.
