// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Optional DLL Fail-Open

# Task

- Task ID: session-20261122-optional-dll-fail-open
- Session ID: session-20261122-optional-dll-fail-open
- Owner: cpp-latency-pressure-profile
- Focus: build.py, build_game_dll.py
- Summary: Kept EXE builds successful after optional presentation-DLL failure; cached repeated failures and measured root dev-loop no-op latency.
- Status: completed
- Date, time, timezone: 2026-11-22T07:16:23+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 9fd637d595a308483a9f82c88349a5915c82a0ca
- Final commit: 9fd637d595a308483a9f82c88349a5915c82a0ca

# Pre-existing changes

- Exact status output: initial `git status --short` showed dozens of unrelated edits and untracked issue/changelog records from concurrent agents, including NPC/networking sources, config, source-manifest/build scripts, tests, and docs. Those files were preserved. At initial inspection `build.py` and `build_game_dll.py` were clean and unmodified.
- Files not created or modified by this session: all other paths in the captured automatic Git status. This session changed `build.py`, `build_game_dll.py`, appended evidence through the coordination issue tool, and created this changelog.

# Requested behavior

The user asked that an optional presentation-DLL failure not fail the overall build, and asked to keep pursuing the long-running C++ edit-to-live goal. Repeated failed work must not block every later EXE build; timings and failure state must remain visible.

# Specification alignment

- Current specification paths: `docs/specs/dev-loop/build-time-observability.md`, `docs/specs/dev-loop/change-to-evidence-latency.md`, `docs/specs/dev-loop/agent-lanes-build-authority.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/specs/hotreload/hotreload.md`.
- Exact requirements: keep the root dev-loop as build authority; separate optional diagnostics/fast paths from required build gates; report elapsed stages and terminal cause; preserve old published artifacts after a candidate failure.
- Why the change follows the specification: the legacy presentation DLL is optional, while the EXE no-op build is authoritative. Its error is recorded and the EXE continues; required EXE compilation and linking behavior is unchanged.
- Conflicts or decisions: the 30-minute negative cache uses size/mtime stamps for explicit source/build/toolchain inputs and does not include all transitive headers. It is bounded and can be bypassed with `MIMITA_RETRY_PRESENTATION_DLL=1`; transitive dependency tracking remains an open correctness improvement.

# Exact implementation changes

## File: `build.py`

- Lines/functions/headings: `PHASE_DETAILS` (118), `write_build_metrics` (163), `write_progress` (407), optional presentation DLL stage (646–813).
- Old content or behavior: every EXE build retried an old optional DLL whenever its output mtime was older than three listed inputs. A nonzero result printed one line but had no stage progress, elapsed time, persistent failure reason, or retry suppression.
- New content or behavior: reports an optional DLL stage with one-second progress updates and retains outcome/error/elapsed details in final progress and metrics. It catches launch errors, does not convert an optional child failure into an EXE failure, caches an unchanged failed fingerprint for up to 30 minutes, retries when its tracked inputs change, and allows a forced retry with `MIMITA_RETRY_PRESENTATION_DLL=1`.
- Reason: prevent the same known optional failure from adding up to 78 seconds to each later no-op EXE build, while keeping the failure visible.
- Why unrelated behavior is preserved: required EXE compile/link status remains authoritative; the running runtime and published game-DLL generation were not changed.

## File: `build_game_dll.py`

- Lines/functions/headings: `run_tool` call and nonzero-result reporting (90–95).
- Old content or behavior: only a generic reload failure was printed when the compiler returned nonzero.
- New content or behavior: captures stdout and reports captured output plus the compiler exit code.
- Reason: expose child output to the root build log. The actual diagnostic remains unknown because compiler output was not present in the observed failure log; stderr is not yet merged into stdout.
- Why unrelated behavior is preserved: successful build commands and output-DLL publication remain unchanged.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `build.py` | Repeated optional child attempts were invisible/unbounded per unchanged inputs. | Timed live/final stage state, a 30-minute bounded failure cache, force-retry variable, and optional launch-failure handling. | Keep repeated optional failures from delaying otherwise valid EXE no-op builds. | Revert only this session's leased `build.py` diff. |
| `build_game_dll.py` | Generic nonzero compiler result. | Captured stdout printed with exit code. | Preserve helper output in the parent build log. | Revert only this session's leased `build_game_dll.py` diff. |
| `docs/issues/build-friction/20261122-exe-build-setup-runs-a-failing-presentation-dll--issue-ae248907f1f1.md` | Existing issue with old 143-second failure evidence. | Appended measured fail-open/cache-skip results and remaining diagnostic/header-fingerprint risks through `agent-coord issue suggest`. | Keep current evidence in the existing issue. | Issue discussion is append-only; append a correction if needed. |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` | Prior build-stage evidence. | Appended gen470 and EXE request/worker/response timing, CPU-sample limits, and next discriminating measurement. | Track the remaining end-to-end gaps. | Issue discussion is append-only; append a correction if needed. |
| `docs/changelog/2026-11-22/20261122_071623-optional-dll-fail-open.md` | New file. | This session record. | Required evidence handoff. | Remove this file only if this session is reverted. |

# Contract / ABI changes

- Contract: no C++ ABI or runtime protocol change.
- Version bump: none.
- Compatibility: existing EXE and hot-reload DLL outputs remain unchanged.
- Reverse: revert only the two leased Python files.

# State / schema changes

- Domain: ignored local state at `.dev/presentation-dll-attempt.json`.
- Old schema: none.
- New schema: schema 1 stores outcome, fingerprint, failure time, exit code/error, and elapsed seconds.
- Migration: not applicable; corrupt/unreadable state falls back to attempting the optional build.
- Reverse: delete only `.dev/presentation-dll-attempt.json` to force a fresh attempt.

# Deleted paths

- None.

# Diagnostics

- Owner/category: `build.py` / optional presentation-DLL build stage.
- Input: listed presentation source/header stamps, builder/toolchain scripts, `build.py`, and compiler file stamp.
- Decision: up-to-date, build, cached-failure skip, or forced retry.
- Output: live `build/progress.json`; final `build/progress.json` and `build/build-timing.json` `phase_details.presentation_dll`; root `.dev/build-output.log`.
- Failure or rejection reason: recorded as child exit code; actual compiler diagnostic is still unknown because stderr was not merged into captured stdout.
- Rate limiting: one-second progress writes; failed fingerprint suppresses repeat for at most 30 minutes unless a tracked input changes or force-retry is set.

# Validation

- Focused skill paths and results: `docs/skills/efficiency-checker-v1.md` reviewed; change is limited to an optional build wrapper and does not alter C++ hot-reload behavior.
- Tests and exact commands: `python -m py_compile build.py build_game_dll.py` passed; `git diff --check -- build.py build_game_dll.py` passed.
- Build status: root dev-loop EXE no-op succeeded with 0 compiled / 87 skipped. A failed optional DLL child did not fail the EXE build. An identical retry reported cached failure and succeeded.
- Runtime or hot-reload evidence: no DLL activation or gameplay behavior was tested or claimed. Existing server PID 26748 remained alive; no game instances were launched or stopped.
- Output files: `build/progress.json`, `build/build-timing.json`, `.dev/build-output.log`, `.dev/presentation-dll-attempt.json`, and the two linked issues.

# Measured evidence

- Before values: historical no-op EXE attempt took 143.118s and retried a failing presentation DLL; later failures continued to add 12–78s.
- After values: successful no-op EXE script elapsed 0.941s after first cached failure, then 0.329s on a later repeat; 87 objects skipped, none compiled. The later dev-loop request still took 21.198s end-to-end (about 11.787s before build.py plus 9.082s after it), so the <=10s request-to-result target is not met.
- Failed optional attempts on unchanged inputs: 19.101s, 12.069s, and 77.760s; each EXE no-op itself completed successfully. These failures were triggered by changing the fingerprinted build scripts and are not a matched throughput benchmark.
- Timestamps: root request `2026-11-22T07:10:44.023Z-6d9e7ca9`; successful EXE progress `2026-11-22T07:10:56.277Z`; coordination request result `2026-11-22T07:11:05.359Z`.
- Resource sample during the 77.760s optional attempt: five-second sample on 12 logical CPUs showed host busy 94.1%, server PID 26748 at 5.309 CPU cores, and sampled compiler parent processes at 0 CPU. This is correlated pressure, not proof that host load caused the compile duration.
- Game-DLL context: generation 470 took 89.078s total for one changed `src/npc/npc-navigator.cpp`; that source compiled in 28.578s, link took 29.600s over 642 objects / 480,395,207 bytes, object scan took 2.188s, and publish took 0.760s. It is not this session's gameplay edit or a runtime behavior proof.
- Tick/frame/network measurements: none.

# Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: no gameplay regression was reproduced. The optional build helper failure is tracked as build friction; the EXE stayed successful.
- Related regression paths: none.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed.
- Still unverified: helper compiler's underlying exit-1 diagnostic; effect of cached failure after an indirect header change; controlled one-source save-to-activation and CPU-pressure benchmark; current server mode identity mismatch.

# Agent handoff context

- Owner: cpp-latency-pressure-profile
- Focus: `issue-ae248907f1f1`, linked to `issue-136d4553ff92`.
- Files to check: `build.py`, `build_game_dll.py`, `devscripts/dev_loop/build.py`, root dev-loop request timeline.
- What another agent should not redo: optional fail-open behavior, 30-minute unchanged-failure suppression, final phase-detail persistence, and the two root no-op skip measurements are already implemented and evidenced.
- Next exact step: merge stderr into captured child output; discover the exact helper exit-1 cause; add a reliable transitive-dependency invalidation; profile the 11.787s pre-script and 9.082s post-script intervals; then run a controlled one-source DLL test with synchronized process/resource sampling.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 9fd637d595a308483a9f82c88349a5915c82a0ca / 9fd637d595a308483a9f82c88349a5915c82a0ca
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-22T02:37:06.972018Z", "latest_successful_build_id": "2026-11-22T02:37:06.972018Z:c94488e6bcdca96b94e61c2985a5ef3f30bb6b70250e32f1cb1bd36b8401e344", "latest_successful_source_generation": 1}, "game_current": {"build_mode": "debug", "code_hash": "253bdd8fabd376b696a86ed07bf3870f07c80444e5797be0491ebde10bfd3239", "dll": "build\\game\\generation-470\\game-g470.dll", "generation": 470, "revision": "0a705a433fa4f7f9a326b30ff6ca911aa92c80dce399e63186f0ce0de799147e", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-470\\game-g470.dll", "generation": 470, "published_utc": "2026-11-22T06:53:53.816Z", "revision": "0a705a433fa4f7f9a326b30ff6ca911aa92c80dce399e63186f0ce0de799147e", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 0.329, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-22T07:10:56.277Z"}}
- Generation: 470
- Artifact: build\game\generation-470\game-g470.dll
- Journal path: C:\mimita-v9\logs\2026-11-22\20261122_011031\server-01\events.jsonl
- Session ID: session-20261122-optional-dll-fail-open

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path: none; this is build tooling and remains an open latency goal.

## Captured git state

```text
M build.py
 M build_game_dll.py
 M config/accounts/default.json
 M config/analytics.json
 M config/hot-reload/packet-ledger.json
 m devscripts/blender-ai
 M devscripts/dev_loop/game_build.py
 M devscripts/game_build_worker.py
 M devscripts/packet_registry_codegen.py
 M devscripts/quality/test_game_build_snapshot.py
 M devscripts/source_manifest.py
 M docs/changelog/2026-11-22/20261122_033000-build-wait-reviewer-fcs-npc-kill-plant-evidence.md
 M docs/gold/2026-11-21-autonomous-gameplay-investigation-and-proof.md
 M docs/gold/20261030_033224-agent-session-handoff-gold.md
 M docs/issues/README.md
 M docs/issues/architecture/20261120-nodes-and-connections-ontology-for-project-and-a-issue-75220121d993.md
 M docs/issues/bug/20261120-audit-npc-target-visibility-and-behavior-behind--issue-82cdc1277704.md
 M docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
 M docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md
 M docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
 M docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-setup-issue-8aae0621645f.md
 M docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6bcfb6252dfa.md
 M docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a59a0058df76.md
 M docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ec19949cbd21.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md
 M docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md
 M docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md
 M docs/issues/improvement/20261121-track-and-eliminate-cross-agent-interference-in--issue-c4f2df8edeba.md
 M docs/issues/tech-debt/20261120-tech-debt-audit-duplicate-special-contact-paths--issue-49ded3edbe47.md
 M src/gui/ui-system-buttons.cpp
 M src/gui/ui-system-internal.h
 M src/gui/ui-system.cpp
 M src/gui/ui-text-input.cpp
 M src/main-systems.cpp
 M src/network/multiplayer-tick.cpp
 M src/npc/npc-behavior-module.cpp
 M src/npc/npc-navigator.cpp
 M src/npc/npc.cpp
?? brain.db
?? devscripts/quality/test_packet_registry_codegen.py
?? devscripts/quality/test_source_manifest_cache.py
?? docs/changelog/2026-11-22/20261122_032700-fcs-npc-events-and-plant-proof-review.md
?? docs/changelog/2026-11-22/20261122_035016-issue-ec19949cbd21-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_035017-issue-8aae0621645f-exe-build-setup-wait-review.md
?? docs/changelog/2026-11-22/20261122_035023-cpp-latency-pressure-profile-fcs-npc-evidence-review.md
?? docs/changelog/2026-11-22/20261122_035200-ctx-1f0fb5aa9253-zero-cpu-reading-review.md
?? docs/changelog/2026-11-22/20261122_035600-issue-a59a0058df76-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_040332-issue-f534b31a9a1a-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_040900-issue-0968dd48e9d8-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_041142-issue-75220121d993-independent-review.md
?? docs/changelog/2026-11-22/20261122_041300-issue-f92715ba3c7f-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_042300-fcs-network-and-console-ui-followup.md
?? docs/changelog/2026-11-22/20261122_042500-issue-4d9eb4b1a8ad-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_042700-issue-bc45be2f32a3-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_043000-cpp-latency-cache-stability-and-build-measurement.md
?? docs/changelog/2026-11-22/20261122_043000-issue-49ded3edbe47-independent-review.md
?? docs/changelog/2026-11-22/20261122_043800-fcs-npc-objective-route-recovery.md
?? docs/changelog/2026-11-22/20261122_043900-issue-266f4fcaf861-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_044500-issue-ce4c245a8a70-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_052200-ctx-fc9b2656cbcd-failopen-build-review.md
?? docs/changelog/2026-11-22/20261122_052200-issue-52b59bdd440c-independent-review.md
?? docs/changelog/2026-11-22/20261122_052200-long-running-goal-and-fail-open-diagnostics-gold.md
?? docs/changelog/2026-11-22/20261122_054942-issue-f8873319f12b-independent-review.md
?? docs/changelog/2026-11-22/20261122_060000-fcs-room-rejoin-health-rebase-runtime-proof.md
?? docs/changelog/2026-11-22/20261122_062200-issue-7e086ec27372-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_062509-issue-6d795125c36f-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_063500-persistent-manifest-cache-and-noop-measurements.md
?? docs/changelog/2026-11-22/20261122_065100-issue-a6a32cacba8c-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_065500-issue-89efbbc86535-independent-review.md
?? docs/changelog/2026-11-22/20261122_070217-fun-counter-strike-npc-objective-and-corner-recovery.md
?? docs/changelog/2026-11-22/20261122_070248-build-wait-reviewer-overbudget-game-dll-wait.md
?? docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-0498cbef4ef3.md
?? docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md
?? docs/issues/build-friction/20261122-exe-build-setup-runs-a-failing-presentation-dll--issue-ae248907f1f1.md
?? docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-compiling-issue-4d9eb4b1a8ad.md
?? docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-setup-issue-34969db31bf6.md
?? docs/issues/build-friction/20261122-wait-over-budget-exe-build-queue-at-build-pendin-issue-f92715ba3c7f.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-00e2e9243beb.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-0968dd48e9d8.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-266f4fcaf861.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-2b6479feedc1.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-467dc2860040.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-4fe1a7a2a4bf.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-60f01796d952.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-61edee02549f.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6202f0e8d9f4.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-628187de3d6d.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6b55a3773999.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6d795125c36f.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-7e086ec27372.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-86b3b2ce1e73.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-9c77739954f9.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a6a32cacba8c.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-bc45be2f32a3.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-bccd816a3abd.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-c481f05923e4.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ce4c245a8a70.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-d2466e463a18.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ec91f5dc7366.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-f534b31a9a1a.md
```

## Captured diff stat

```text
build.py                                           | 163 ++++++++++++++--
 build_game_dll.py                                  |   7 +-
 config/accounts/default.json                       |   2 +-
 config/analytics.json                              |   4 +-
 config/hot-reload/packet-ledger.json               | 160 ++++++++--------
 devscripts/blender-ai                              |   0
 devscripts/dev_loop/game_build.py                  |  97 ++++++++--
 devscripts/game_build_worker.py                    | 144 +++++++++++---
 devscripts/packet_registry_codegen.py              | 164 +++++++++++++---
 devscripts/quality/test_game_build_snapshot.py     | 110 +++++++++++
 devscripts/source_manifest.py                      | 130 ++++++++++++-
 ...ld-wait-reviewer-fcs-npc-kill-plant-evidence.md |  43 ++++-
 ...-autonomous-gameplay-investigation-and-proof.md |   6 +
 .../20261030_033224-agent-session-handoff-gold.md  |  31 +++
 docs/issues/README.md                              |  69 +++++--
 ...ntology-for-project-and-a-issue-75220121d993.md |  40 ++++
 ...lity-and-behavior-behind--issue-82cdc1277704.md |  37 ++++
 ...objective-behavior-regres-issue-be1adb51b6ac.md |  88 +++++++++
 ...ocal-ai-player-controller-issue-f8873319f12b.md |  36 ++++
 ...move-avoidable-build-and--issue-e044a931c54b.md |   6 +
 ...budget-exe-build-at-setup-issue-8aae0621645f.md |  50 +++++
 ...e-dll-build-at-game-build-issue-6bcfb6252dfa.md |  25 +++
 ...e-dll-build-at-game-build-issue-a59a0058df76.md |  28 +++
 ...e-dll-build-at-game-build-issue-ec19949cbd21.md |  45 +++++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  66 +++++++
 ...nd-observe-context-bundle-issue-6907072bb155.md |   6 +
 ...s-to-the-latest-file-atom-issue-52b59bdd440c.md |  38 ++++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  20 ++
 ...-a-specific-tick-for-proo-issue-89efbbc86535.md |  38 ++++
 ...opment-and-networking-con-issue-cf9cd77eb056.md |  81 ++++++++
 ...ompleteness-and-readiness-issue-3100aa7b5807.md |  16 ++
 ...-and-build-to-proof-links-issue-b396205aaecf.md |   6 +
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 211 ++++++++++++++++++++-
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md |   6 +
 ...dates-for-findings-and-cr-issue-70cbf4d13c02.md |  14 ++
 ...ss-agent-interference-in--issue-c4f2df8edeba.md |  14 ++
 ...te-special-contact-paths--issue-49ded3edbe47.md |  39 ++++
 src/gui/ui-system-buttons.cpp                      |   4 +
 src/gui/ui-system-internal.h                       |   1 +
 src/gui/ui-system.cpp                              |  38 +++-
 src/gui/ui-text-input.cpp                          |   2 +
 src/main-systems.cpp                               |  20 +-
 src/network/multiplayer-tick.cpp                   |   8 +-
 src/npc/npc-behavior-module.cpp                    |   2 +
 src/npc/npc-navigator.cpp                          |  48 ++++-
 src/npc/npc.cpp                                    |  83 ++++++--
 46 files changed, 2010 insertions(+), 236 deletions(-)
```

## Captured changed paths

```text
M	build.py
M	build_game_dll.py
M	config/accounts/default.json
M	config/analytics.json
M	config/hot-reload/packet-ledger.json
M	devscripts/blender-ai
M	devscripts/dev_loop/game_build.py
M	devscripts/game_build_worker.py
M	devscripts/packet_registry_codegen.py
M	devscripts/quality/test_game_build_snapshot.py
M	devscripts/source_manifest.py
M	docs/changelog/2026-11-22/20261122_033000-build-wait-reviewer-fcs-npc-kill-plant-evidence.md
M	docs/gold/2026-11-21-autonomous-gameplay-investigation-and-proof.md
M	docs/gold/20261030_033224-agent-session-handoff-gold.md
M	docs/issues/README.md
M	docs/issues/architecture/20261120-nodes-and-connections-ontology-for-project-and-a-issue-75220121d993.md
M	docs/issues/bug/20261120-audit-npc-target-visibility-and-behavior-behind--issue-82cdc1277704.md
M	docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
M	docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md
M	docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
M	docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-setup-issue-8aae0621645f.md
M	docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6bcfb6252dfa.md
M	docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a59a0058df76.md
M	docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ec19949cbd21.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md
M	docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md
M	docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md
M	docs/issues/improvement/20261121-track-and-eliminate-cross-agent-interference-in--issue-c4f2df8edeba.md
M	docs/issues/tech-debt/20261120-tech-debt-audit-duplicate-special-contact-paths--issue-49ded3edbe47.md
M	src/gui/ui-system-buttons.cpp
M	src/gui/ui-system-internal.h
M	src/gui/ui-system.cpp
M	src/gui/ui-text-input.cpp
M	src/main-systems.cpp
M	src/network/multiplayer-tick.cpp
M	src/npc/npc-behavior-module.cpp
M	src/npc/npc-navigator.cpp
M	src/npc/npc.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-22T02:37:06.972018Z",
    "latest_successful_build_id": "2026-11-22T02:37:06.972018Z:c94488e6bcdca96b94e61c2985a5ef3f30bb6b70250e32f1cb1bd36b8401e344",
    "latest_successful_source_generation": 1
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "253bdd8fabd376b696a86ed07bf3870f07c80444e5797be0491ebde10bfd3239",
    "dll": "build\\game\\generation-470\\game-g470.dll",
    "generation": 470,
    "revision": "0a705a433fa4f7f9a326b30ff6ca911aa92c80dce399e63186f0ce0de799147e",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-470\\game-g470.dll",
    "generation": 470,
    "published_utc": "2026-11-22T06:53:53.816Z",
    "revision": "0a705a433fa4f7f9a326b30ff6ca911aa92c80dce399e63186f0ce0de799147e",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 0.329,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-22T07:10:56.277Z"
  }
}
```
