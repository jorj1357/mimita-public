// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# FPS Drop Diagnosis Issue

# Task

- Task ID: session-fps-auto-trace-issue-20261121-0011
- Session ID: session-fps-auto-trace-issue-20261121-0011
- Owner: fps-dip-auto-trace-intake
- Focus: issue intake and performance observability
- Summary: Added a fully specified issue proposing automatic per-frame under-240-FPS tracing and issue creation or updates, linked to the existing 240 FPS and AI-awareness issues.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-21T00:12:51+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d651e1332fccff70bc5cc4601dc24d55784820b5
- Final commit: d651e1332fccff70bc5cc4601dc24d55784820b5

# Pre-existing changes

- Exact status output: the generated Git status below contains concurrent shared-checkout work in multiple source/config/docs paths. This session did not edit those source/config files or attribute them to this task.
- Files not created or modified by this session: all paths in the captured status other than the new issue record `docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md`, generated index `docs/issues/README.md`, and the append-only updates to the two related issue records noted below.

# Requested behavior

Preserve the user's report verbatim and file a high-priority issue for automatic diagnosis whenever a MiMITA.exe client frame exceeds the 4.167 ms budget (below 240 FPS). The feature should capture enough exact runtime information for an AI agent to explain the slowdown, create or extend an issue by stable cause signature, preserve each occurrence, and expose missing observability instead of guessing.

# Specification alignment

- Current specification paths: `docs/specs/performance/performance.md`; `docs/specs/debug-logging/debug-logging.md`; `docs/workflows/issue-intake-and-triage.md`; `docs/specs/dev-loop/agent-context-contract.md`.
- Exact requirements: performance acceptance is per frame at <=4.167 ms; one-second FPS records are diagnostic only. `events.jsonl` is the canonical in-process journal. Issue intake preserves exact user wording and includes reproduction, expected behavior, acceptance, paths, evidence, risk, and proof expectations.
- Why the change follows the specification: the issue proposes an event-level frame miss trigger, bounded canonical-journal context, exact process/build/run identity, and real-executable proof; it labels causes as measured or inferred and treats issue creation as insufficient proof.
- Conflicts or decisions: none. This is an issue proposal only; no specification or implementation was changed.

# Exact implementation changes

## File: `docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md`

- Lines/functions/headings: issue title, immutable human report, summary, reproduction, expected behavior, acceptance, paths, evidence, risk, and proof expectation.
- Old content or behavior: no dedicated issue specified automatic per-frame slowdown capture and issue lifecycle behavior.
- New content or behavior: issue `issue-13d080318e1c` specifies bounded before/after capture in the client `events.jsonl`, stable-signature create-or-extend behavior, occurrence retention, missing-signal reporting, measured-vs-inferred diagnosis, and pinned real-executable acceptance.
- Reason: make the requested AI-facing performance diagnosis capability actionable and reviewable.
- Why unrelated behavior is preserved: no runtime, source, configuration, build, or test code was changed.

## Related append-only records

- `docs/issues/README.md`: generated index now lists `issue-13d080318e1c`.
- `docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md`: appended a suggestion distinguishing the existing 240 FPS outcome issue from the new automatic diagnosis capability.
- `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md`: appended a suggestion linking the performance evidence proposal to the AI-awareness goal.
- The user's context `ctx-6929acf06b43` is primary on `issue-13d080318e1c` and related to both existing issues.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/audio/music-settings.json` |  |  |  | git checkout -- config/audio/music-settings.json |
| `config/behavior-profiles.json` |  |  |  | git checkout -- config/behavior-profiles.json |
| `config/movement/movement-heavy.json` |  |  |  | git checkout -- config/movement/movement-heavy.json |
| `config/movement/movement-source.json` |  |  |  | git checkout -- config/movement/movement-source.json |
| `config/video-settings.json` |  |  |  | git checkout -- config/video-settings.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `devscripts/quality/test_game_build_snapshot.py` |  |  |  | git checkout -- devscripts/quality/test_game_build_snapshot.py |
| `docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md` |  |  |  | git checkout -- docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md` |  |  |  | git checkout -- docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md |
| `docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md` |  |  |  | git checkout -- docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md |
| `docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md` |  |  |  | git checkout -- docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md |
| `docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md` |  |  |  | git checkout -- docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md |
| `docs/specs/dev-loop/agent-build-queue.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-build-queue.md |
| `docs/specs/dev-loop/build-time-observability.md` |  |  |  | git checkout -- docs/specs/dev-loop/build-time-observability.md |
| `src/npc/npc.cpp` |  |  |  | git checkout -- src/npc/npc.cpp |
| `devscripts/quality/test_build_coordinator.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_build_coordinator.py' |
| `docs/changelog/2026-11-20/20261120_232749-investigate-listed-server-join-failure.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_232749-investigate-listed-server-join-failure.md' |
| `docs/changelog/2026-11-20/20261120_233056-safe-generated-data-cleanup-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_233056-safe-generated-data-cleanup-review.md' |
| `docs/changelog/2026-11-20/20261120_233200-fun-counterstrike-npc-regression-intake.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_233200-fun-counterstrike-npc-regression-intake.md' |
| `docs/changelog/2026-11-20/20261120_235343-trace-public-server-row-join-for-9k5ypnd.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_235343-trace-public-server-row-join-for-9k5ypnd.md' |
| `docs/changelog/2026-11-21/` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/' |
| `docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md' |
| `docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md' |
| `docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md' |
| `docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md' |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md' |
| `docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md' |

# Contract / ABI changes

- Contract: no implementation contract changed; the issue documents a proposed diagnostic capability.
- Version bump: none.
- Compatibility: unchanged.
- Reverse: append-only issue intake is retained; cancel or supersede the issue through coordination if its scope is rejected.

# State / schema changes

- Domain: none.
- Old schema: unchanged.
- New schema: none.
- Migration: none.
- Reverse: not applicable.

# Deleted paths

- Path: none.
- Before: none.
- Why: no deletion was requested or needed.
- Reverse: not applicable.

# Diagnostics

- Owner/category: proposed client performance diagnostics and issue-intake automation; no diagnostic owner was implemented in this session.
- Input: rendered-frame duration relative to 4.167 ms and available client process/run/build/workload signals.
- Decision: proposed stable-signature grouping; append an occurrence to a matching issue or create a new issue for a distinct signature.
- Output: proposed bounded diagnostic records in the canonical client `events.jsonl` and evidence-linked issue records.
- Failure or rejection reason: the issue requires missing telemetry and unavailable GPU/thread signals to be named explicitly; no runtime behavior was exercised here.
- Rate limiting: proposed bounded context and measured overhead/log volume; exact limits remain for implementation design and review.

# Validation

- Focused skill paths and results: `docs/skills/issue-triage-v1.md` reviewed; new issue has all required fields and readiness score 8/8. `docs/skills/documentation-checker-v1.md` reviewed for the issue/index docs; no specification document was edited.
- Tests and exact commands: no tests run; documentation/issue intake only. `python devscripts/agent-issue.py ...` created the issue, and `python devscripts/changelog.py --check --session-id session-fps-auto-trace-issue-20261121-0011` confirmed exactly one changelog for this session.
- Build status: no build requested; no source/compiler inputs changed.
- Runtime or hot-reload evidence: none; automatic trace behavior is not implemented or runtime-proven.
- Output files: issue record above, generated `docs/issues/README.md`, append-only suggestions in the two related issue records, and this changelog.

# Measured evidence

- Before values: existing FPS issue `issue-549680e53b26` records a historical 90-120 FPS observation and a 2368.59 ms maximum spike, but has no automatic trace/issue lifecycle requirement.
- After values: no game performance measurement; issue readiness is 8/8 and the new request is linked to both related issues.
- Timestamps: issue created 2026-11-21T00:10:46.554Z; related links recorded by coordination at 2026-11-21T00:11:17Z.
- Tick/frame/network measurements: none collected in this issue-intake session.

# Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: no new behavior was tested or observed; this work only filed a capability proposal.
- Related regression paths: none.

# Human acceptance

- Visual review: not performed or needed for issue intake.
- Gameplay review: not performed; no client was exercised.
- Multiplayer review: not performed or needed for this issue intake.
- Still unverified: whether the proposed instrumentation, signature grouping, and issue automation can run with bounded overhead and identify causes reliably.

# Agent handoff context

- Owner: unassigned in the issue tracker.
- Focus: issue intake and performance observability.
- Files to check: issue `issue-13d080318e1c`; performance and debug-logging specs listed above; related issues `issue-549680e53b26` and `issue-91e3384b219b`.
- What another agent should not redo: do not create a duplicate issue; retain this issue as the capability record and issue-549680e53b26 as the frame-consistency outcome record.
- Next exact step: triage and assign the capability issue, inspect current client frame instrumentation and issue automation owners, then implement through the shared dev-loop and validate a pinned MiMITA.exe run.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / d651e1332fccff70bc5cc4601dc24d55784820b5 / d651e1332fccff70bc5cc4601dc24d55784820b5
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-20T20:20:51.788061Z", "latest_successful_build_id": "2026-11-20T20:20:51.788061Z:64cd9304582622039486e6a1fe369e2f7d466a2dc0eac7975241f98f0ee0df55", "latest_successful_source_generation": 38}, "game_current": {"build_mode": "release", "code_hash": "95fb913f80de54772285da4373d367b47dfbcf882c1d2636f52ff692b791a644", "dll": "build\\game\\generation-417\\game-g417.dll", "generation": 417, "revision": "6159737d84d4818b288feefa86a39447cab164b6c5b24b19a269d45a262871ff", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-417\\game-g417.dll", "generation": 417, "published_utc": "2026-11-21T00:11:58.032Z", "revision": "6159737d84d4818b288feefa86a39447cab164b6c5b24b19a269d45a262871ff", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 31.195, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-20T21:40:39.596Z"}}
- Generation: 417
- Artifact: build\game\generation-417\game-g417.dll
- Journal path: C:\mimita-v9\logs\2026-11-21\20261121_000922\server-01\events.jsonl
- Session ID: session-fps-auto-trace-issue-20261121-0011

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
M config/accounts/default.json
 M config/analytics.json
 M config/audio/music-settings.json
 M config/behavior-profiles.json
 M config/movement/movement-heavy.json
 M config/movement/movement-source.json
 M config/video-settings.json
 m devscripts/blender-ai
 M devscripts/dev-loop.py
 M devscripts/dev_loop/game_build.py
 M devscripts/game_build_worker.py
 M devscripts/quality/test_game_build_snapshot.py
 M docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md
 M docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md
 M docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md
 M docs/specs/dev-loop/agent-build-queue.md
 M docs/specs/dev-loop/build-time-observability.md
 M src/npc/npc.cpp
?? devscripts/quality/test_build_coordinator.py
?? docs/changelog/2026-11-20/20261120_232749-investigate-listed-server-join-failure.md
?? docs/changelog/2026-11-20/20261120_233056-safe-generated-data-cleanup-review.md
?? docs/changelog/2026-11-20/20261120_233200-fun-counterstrike-npc-regression-intake.md
?? docs/changelog/2026-11-20/20261120_235343-trace-public-server-row-join-for-9k5ypnd.md
?? docs/changelog/2026-11-21/
?? docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
?? docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md
?? docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md
?? docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md
?? docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
?? docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   6 +-
 config/analytics.json                              |   2 +-
 config/audio/music-settings.json                   |   2 +-
 config/behavior-profiles.json                      |  23 ++---
 config/movement/movement-heavy.json                |   3 +-
 config/movement/movement-source.json               |   3 +-
 config/video-settings.json                         |   2 +-
 devscripts/blender-ai                              |   0
 devscripts/dev-loop.py                             | 110 +++++++++++++++++++--
 devscripts/dev_loop/game_build.py                  |  21 ++--
 devscripts/game_build_worker.py                    |  93 ++++++++++++++++-
 devscripts/quality/test_game_build_snapshot.py     |  30 +++++-
 ...e-issues-preview-overlay-first-frame-context.md |  30 +++++-
 docs/issues/README.md                              |  22 +++--
 ...cy-no-frame-below-240-fps-issue-549680e53b26.md |   6 ++
 ...ly-first-line-to-opencode-issue-62ab3daa615d.md |  35 +++++++
 ...e-dll-build-at-game-build-issue-0322fa45b0e8.md |  18 ++++
 ...ative-editing-and-credite-issue-421c5be1669f.md |   6 ++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  16 +++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  12 +++
 ...ner-preview-overlays-and--issue-06c8cd76dc9d.md |  26 +++++
 ...opment-and-networking-con-issue-cf9cd77eb056.md |  40 ++++++++
 .../counterstrike-npc-targeting-REG-2026-10-04.md  |  27 +++++
 docs/specs/dev-loop/agent-build-queue.md           |  11 +++
 docs/specs/dev-loop/build-time-observability.md    |  22 +++++
 src/npc/npc.cpp                                    |  11 ++-
 26 files changed, 519 insertions(+), 58 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/behavior-profiles.json
M	config/movement/movement-heavy.json
M	config/movement/movement-source.json
M	config/video-settings.json
M	devscripts/blender-ai
M	devscripts/dev-loop.py
M	devscripts/dev_loop/game_build.py
M	devscripts/game_build_worker.py
M	devscripts/quality/test_game_build_snapshot.py
M	docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md
M	docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md
M	docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md
M	docs/specs/dev-loop/agent-build-queue.md
M	docs/specs/dev-loop/build-time-observability.md
M	src/npc/npc.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-20T20:20:51.788061Z",
    "latest_successful_build_id": "2026-11-20T20:20:51.788061Z:64cd9304582622039486e6a1fe369e2f7d466a2dc0eac7975241f98f0ee0df55",
    "latest_successful_source_generation": 38
  },
  "game_current": {
    "build_mode": "release",
    "code_hash": "95fb913f80de54772285da4373d367b47dfbcf882c1d2636f52ff692b791a644",
    "dll": "build\\game\\generation-417\\game-g417.dll",
    "generation": 417,
    "revision": "6159737d84d4818b288feefa86a39447cab164b6c5b24b19a269d45a262871ff",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-417\\game-g417.dll",
    "generation": 417,
    "published_utc": "2026-11-21T00:11:58.032Z",
    "revision": "6159737d84d4818b288feefa86a39447cab164b6c5b24b19a269d45a262871ff",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 31.195,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-20T21:40:39.596Z"
  }
}
```
