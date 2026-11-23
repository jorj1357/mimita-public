// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Source-only object marker fast path

## Session-specific evidence

This session continues the active Mode 3 objective; it does not meet the goal.
No native build or game process was started. The retained run is
`20261123_055730_01`, with generation 554 as the latest published artifact.
The date journal tree measured 6,999,999,734 of 7,000,000,000 bytes and active
runtime journals had stopped updating. I preserved the existing pair and did
not publish a DLL without a trustworthy activation journal.

### Exact source changes

- `devscripts/game_build_worker.py`: added `source_only_changed_paths()` at
  line 789; it accepts only complete manifest reports for existing `.cpp`
  content edits and rejects header, owner, path-set, malformed, forced, and
  alternate-source-list cases. Added `source_only_cache_marker_valid()` at
  line 966 to verify fresh changed objects and the complete previous published
  object marker. The build scan at line 2260 now uses that path only for a
  strict source-only delta; otherwise it runs the existing full dependency
  scan. Marker object discovery now calls `DirEntry.stat()` once per object.
- The scan snapshot at line 568 now records `scan_mode` and
  `cache_marker_reused_count` in progress and per-attempt timing output. A
  successful fast path records `source_only_complete_marker` and emits one
  aggregate scan record instead of hundreds of individual skip rows.
- Before this change, the source-only helper and the two marker fields did not
  exist; the worker always called `scan_entries(entries, ...)` for this scan.
- `devscripts/quality/test_game_build_cache_marker_fast_path.py` is new. It
  covers strict acceptance/rejection, valid reuse, stale changed objects, and
  missing unchanged objects.

### Validation and measured limits

- `python -m unittest devscripts.quality.test_game_build_cache_marker_fast_path devscripts.quality.test_game_build_pch_profile` — 10 passed.
- `python -m py_compile devscripts/game_build_worker.py` — passed.
- `git diff --check -- devscripts/game_build_worker.py devscripts/quality/test_game_build_cache_marker_fast_path.py` — passed.
- Prior cleanup generation 554 measured object-cache scan 2.525 s, link
  3.840 s, publication 0.441 s, and server activation 46.387 s after request.
  This session has no before/after live timing for the new fast path, so it
  cannot claim the scan reached one second or that total latency improved.
- Remaining work: restore journal observability without losing evidence, then
  run a matched one-source sample and 15-source batch through the root dev-loop;
  compare scan mode/duration, compile count, link, publication, and activation
  in both existing process journals. Keep CPU-agent attribution separate.

# Task

- Task ID: session-a91a0d8347d2
- Session ID: session-a91a0d8347d2
- Owner: mode3-live-dll-test
- Focus: issue-136d4553ff92
- Summary: Add a strictly gated cache marker reuse path for existing .cpp-only deltas; focused tests pass, while live integration remains unmeasured because runtime journals are quota-stopped.
- Status: partial
- Date, time, timezone: 2026-11-23T09:41:01+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 0abd6231aea08bed8580bff0c11369c01fc40bde
- Final commit: 0abd6231aea08bed8580bff0c11369c01fc40bde

# Pre-existing changes

- Exact status output:
- Files not created or modified by this session:

# Requested behavior

# Specification alignment

- Current specification paths:
- Exact requirements:
- Why the change follows the specification:
- Conflicts or decisions:

# Exact implementation changes

## File: `path/to/file`

- Lines/functions/headings:
- Old content or behavior:
- New content or behavior:
- Reason:
- Why unrelated behavior is preserved:

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/build-friction/20261120-game-dll-build-publication-fails-replacing-share-issue-61b5edc937d9.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-game-dll-build-publication-fails-replacing-share-issue-61b5edc937d9.md |
| `docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-dff7e8c0f758.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-dff7e8c0f758.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6413477d361d.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6413477d361d.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-dd54a79216ad.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-dd54a79216ad.md |
| `docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `devscripts/quality/test_game_build_cache_marker_fast_path.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_game_build_cache_marker_fast_path.py' |
| `docs/changelog/2026-11-23/20261123_0805-cpu-phase-evidence-source-mapping.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0805-cpu-phase-evidence-source-mapping.md' |
| `docs/changelog/2026-11-23/20261123_0816-build-15file-latency-and-resource-coupling.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0816-build-15file-latency-and-resource-coupling.md' |
| `docs/changelog/2026-11-23/20261123_0835-wpr-sampled-function-attribution.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0835-wpr-sampled-function-attribution.md' |
| `docs/changelog/2026-11-23/20261123_0846-wpr-workload-runtime-alignment.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0846-wpr-workload-runtime-alignment.md' |
| `docs/changelog/2026-11-23/20261123_0847-issue-6413477d361d-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0847-issue-6413477d361d-independent-review.md' |
| `docs/changelog/2026-11-23/20261123_0902-mode3-pch-off-and-generation554.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0902-mode3-pch-off-and-generation554.md' |
| `docs/changelog/2026-11-23/20261123_0910-cpu-wpr-evidence-crosslink.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0910-cpu-wpr-evidence-crosslink.md' |
| `docs/changelog/2026-11-23/20261123_0920-structured-log-quota-pressure-evidence.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0920-structured-log-quota-pressure-evidence.md' |
| `docs/changelog/2026-11-23/20261123_092247-independent-review-of-game-dll-progress-json-publication-failure.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_092247-independent-review-of-game-dll-progress-json-publication-failure.md' |
| `docs/coordination/decisions/20261123_080111-mode3-live-dll-test-decision-24dfc5502e86.md` |  |  |  | Remove-Item -LiteralPath 'docs/coordination/decisions/20261123_080111-mode3-live-dll-test-decision-24dfc5502e86.md' |
| `docs/coordination/decisions/20261123_082556-mode3-live-dll-test-decision-c01373b5bc38.md` |  |  |  | Remove-Item -LiteralPath 'docs/coordination/decisions/20261123_082556-mode3-live-dll-test-decision-c01373b5bc38.md' |

# Contract / ABI changes

- Contract:
- Version bump:
- Compatibility:
- Reverse:

# State / schema changes

- Domain:
- Old schema:
- New schema:
- Migration:
- Reverse:

# Deleted paths

- Path:
- Before:
- Why:
- Reverse:

# Diagnostics

- Owner/category:
- Input:
- Decision:
- Output:
- Failure or rejection reason:
- Rate limiting:

# Validation

- Focused skill paths and results:
- Tests and exact commands:
- Build status:
- Runtime or hot-reload evidence:
- Output files:

# Measured evidence

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

- Regression entry appended: yes/no
- Why this is or is not a confirmed regression:
- Related regression paths:

# Human acceptance

- Visual review:
- Gameplay review:
- Multiplayer review:
- Still unverified:

# Agent handoff context

- Owner: mode3-live-dll-test
- Focus: issue-136d4553ff92
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 0abd6231aea08bed8580bff0c11369c01fc40bde / 0abd6231aea08bed8580bff0c11369c01fc40bde
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-22T21:32:48.195707Z", "latest_successful_build_id": "2026-11-22T21:32:48.195707Z:e24aaf16cbfb97461c3d51e25f7f1424c56865155889c6be816a7bc9b9b44e82", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "9dae6700e0d2c7eb03a5fdbf22cbbb9e96cb37dea88fa60eb60754219fae28ea", "dll": "build\\game\\generation-554\\game-g554.dll", "generation": 554, "revision": "e8238395bd7704c3f8eb5e6059581c60d477d6cbc3609569362657f235002d54", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-554\\game-g554.dll", "generation": 554, "published_utc": "2026-11-23T08:55:58.789Z", "revision": "e8238395bd7704c3f8eb5e6059581c60d477d6cbc3609569362657f235002d54", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 0.286, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-22T22:02:10.445Z"}}
- Generation: 554
- Artifact: build\game\generation-554\game-g554.dll
- Journal path: C:\mimita-v9\logs\2026-11-23\20261123_055730_01\server-01\events.jsonl
- Session ID: session-a91a0d8347d2

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path:

## Captured git state

```text
M config/analytics.json
 m devscripts/blender-ai
 M devscripts/game_build_worker.py
 M docs/issues/README.md
 M docs/issues/build-friction/20261120-game-dll-build-publication-fails-replacing-share-issue-61b5edc937d9.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-dff7e8c0f758.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6413477d361d.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-dd54a79216ad.md
 M docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
?? devscripts/quality/test_game_build_cache_marker_fast_path.py
?? docs/changelog/2026-11-23/20261123_0805-cpu-phase-evidence-source-mapping.md
?? docs/changelog/2026-11-23/20261123_0816-build-15file-latency-and-resource-coupling.md
?? docs/changelog/2026-11-23/20261123_0835-wpr-sampled-function-attribution.md
?? docs/changelog/2026-11-23/20261123_0846-wpr-workload-runtime-alignment.md
?? docs/changelog/2026-11-23/20261123_0847-issue-6413477d361d-independent-review.md
?? docs/changelog/2026-11-23/20261123_0902-mode3-pch-off-and-generation554.md
?? docs/changelog/2026-11-23/20261123_0910-cpu-wpr-evidence-crosslink.md
?? docs/changelog/2026-11-23/20261123_0920-structured-log-quota-pressure-evidence.md
?? docs/changelog/2026-11-23/20261123_092247-independent-review-of-game-dll-progress-json-publication-failure.md
?? docs/coordination/decisions/20261123_080111-mode3-live-dll-test-decision-24dfc5502e86.md
?? docs/coordination/decisions/20261123_082556-mode3-live-dll-test-decision-c01373b5bc38.md
```

## Captured diff stat

```text
config/analytics.json                              |   2 +-
 devscripts/blender-ai                              |   0
 devscripts/game_build_worker.py                    | 197 ++++++++++++++-
 docs/issues/README.md                              |  14 +-
 ...ion-fails-replacing-share-issue-61b5edc937d9.md |  42 ++++
 ...budget-exe-build-at-setup-issue-dff7e8c0f758.md |   6 +
 ...e-dll-build-at-game-build-issue-6413477d361d.md |  34 +++
 ...e-dll-build-at-game-build-issue-dd54a79216ad.md |  39 +++
 ...move-avoidable-build-and--issue-e044a931c54b.md |  10 +
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  25 ++
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 180 ++++++++++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 274 +++++++++++++++++++++
 12 files changed, 809 insertions(+), 14 deletions(-)
```

## Captured changed paths

```text
M	config/analytics.json
M	devscripts/blender-ai
M	devscripts/game_build_worker.py
M	docs/issues/README.md
M	docs/issues/build-friction/20261120-game-dll-build-publication-fails-replacing-share-issue-61b5edc937d9.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-dff7e8c0f758.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6413477d361d.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-dd54a79216ad.md
M	docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-22T21:32:48.195707Z",
    "latest_successful_build_id": "2026-11-22T21:32:48.195707Z:e24aaf16cbfb97461c3d51e25f7f1424c56865155889c6be816a7bc9b9b44e82",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "9dae6700e0d2c7eb03a5fdbf22cbbb9e96cb37dea88fa60eb60754219fae28ea",
    "dll": "build\\game\\generation-554\\game-g554.dll",
    "generation": 554,
    "revision": "e8238395bd7704c3f8eb5e6059581c60d477d6cbc3609569362657f235002d54",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-554\\game-g554.dll",
    "generation": 554,
    "published_utc": "2026-11-23T08:55:58.789Z",
    "revision": "e8238395bd7704c3f8eb5e6059581c60d477d6cbc3609569362657f235002d54",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 0.286,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-22T22:02:10.445Z"
  }
}
```
