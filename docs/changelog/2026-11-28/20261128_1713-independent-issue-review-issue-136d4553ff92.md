// 2026-11-28 17:13 UTC
/* purpose
* record one bounded independent issue-review activation
* append an evidence/counterevidence/next-check review to issue-136d4553ff92
* append one non-duplicate proof-reconciliation proposal to issue-9a4503f6ed68
* no source, build, close, run, or fix claim by this session
*/

# Independent bounded review of issue-136d4553ff92

- Task ID: wake-7f80ca3ba980
- Work session: work-4e777880a2b0 / session-199fa56faa16
- Agent: issue-review-sweeper
- Focus: one read-only review of "Account for every DLL build stage and investigate end-to-end speed": readiness, proof status, linked artifacts, and the newest 2026-11-28 build attempts plus their same-PID activation journals
- Summary: Readiness complete (`missing=[]`, ready=true, score 8/8, state in_review, 11 tasks: 3 done runtime_proven, 3 in_progress with no proof, 5 open). New since the 2026-11-27T04:50Z review: the newest single-source attempt game-dll-20261128T170629149896Z-5ccf06 (generation 820, only `src/vehicles/vehicle-system.cpp`) reached a real same-PID live reload. Request accepted 17:06:28.970Z, attempt started 17:06:29.149Z; generation published 17:06:57.294Z; two already-live servers (run `20261128_082433` PID 26304 and run `20261128_084634` PID 5308) activated generation 820 at 17:06:57.100Z, reason `candidate committed`, giving request->same-PID activation 28.130 s. Host pressure was low (mean CPU 46.162%, peak 67.2%, min available ~3.01 GiB), and the changed TU itself compiled in 17.750 s (parsing 10.06 s, preprocessing 7.52 s, opt+generate 4.20 s), so the monolithic changed-TU compile + ~4 s link is still the gating unit, not pressure. The activation pair was `vehicle_freeplay`/`arena2` with 0 players/0 NPCs, so the required heavy Juggernaut-horde 15-20-file <=10 s scenario remains unproven. A 13-path attempt (c68ca8) failed fast at compile with the GCC diagnostic now retained; a 163-TU header-catch-up attempt (a0bac6) took 490.8 s under 98% CPU. The `cpp-latency-compile-time-report` task is still in_progress with `latest_proof=null` while parsed `compiler_timing` rows are live, so that proof gap has now persisted ~2 days. Appended review id 573 and one proof-reconciliation proposal to `issue-9a4503f6ed68`. No source edit, build request, run, close, or fix claim.
- Status: complete (review only)
- Date, time, timezone: 2026-11-28T17:13Z, ISO 8601

# Pre-existing changes

- The shared checkout contains extensive pre-existing modifications from other agents (source, config, `docs/issues/*`, `devscripts/agent_coord.py`, and many changelogs). This session edited no source or configuration, reverted nothing, and touched only the issue records through `agent-coord.py`, coordination state, and this changelog. No compiler, `build.py`, module worker, second dev-loop, or game runtime was invoked by this session.

# Requested behavior

Run one bounded independent review of `issue-136d4553ff92`: inspect readiness/proof/linked artifacts/latest journals; append one evidence-grounded review with observed facts, inference/uncertainty, contrary evidence, and the next discriminating check; then a short development-process review with one specific, measurable proposal to the best existing improvement issue (or state none). Do not edit source, build, close, or claim a fix.

# Specification alignment

- AGENTS.md gates 0b/0c/0d, 2, 4: coordination begin/sync/observe, observations separated from inference, no compiler/build/second dev-loop in a review wake.
- `docs/specs/dev-loop/agent-proof-and-verification.md`: a build is not behavior proof; a task needs a pinned record or explicit decline.
- `docs/specs/dev-loop/agent-wake.md`: a woken agent is a bounded activation that checks proof state before ending.

# Exact implementation changes

None. No source, specification, or configuration change.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` | reviews through id 519 | review id 573 (evidence-gap) appended | durable independent review evidence | remove only appended review text |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` | prior wait-identity/proof proposals | one proof-reconciliation proposal appended | repeated missing proof record | remove only appended proposal |
| `.dev/agent-coord/brain.db` | no review id 573 | review id 573 + issue discussion | coordination record | restore from backup if withdrawn |
| `docs/changelog/2026-11-28/20261128_1713-independent-issue-review-issue-136d4553ff92.md` | absent | added | one final session changelog | delete file |

# Validation

- `python devscripts/agent-coord.py begin`, `sync`, `observe`, `issue brief`, `issue readiness`, `proof status`, `activity set`, `issue review`, `issue suggest`, and `end`.
- Read-only artifact checks: `.dev/build-timings/game-dll-20261128T170629149896Z-5ccf06.{json,jsonl,resource.json,resource.jsonl}`, `.dev/build-timings/game-dll-20261128T090217845310Z-a0bac6.json`, `.dev/build-timings/game-dll-20261128T085721640685Z-c68ca8.json`, `.dev/build-timings/game-dll-20261128T084130319731Z-1d504b.json`, `logs/2026-11-28/20261128_092602/dev-loop/events.jsonl`, `logs/2026-11-28/20261128_082433/server-01/events*.jsonl`, `logs/2026-11-28/20261128_084634/server-01/events.jsonl`, and `logs/2026-11-28/20261128_170820/{dev-loop,server-01}/events.jsonl`. No build or run requested.

# Measured evidence

- Readiness: ready=true, score 8/8, missing=[], state in_review, 11 tasks (3 done runtime_proven, 3 in_progress with latest_proof=null, 5 open).
- gen820 attempt 5ccf06: control accepted 17:06:28.970Z (build-request-54de4849570b), attempt start 17:06:29.149Z, result 17:06:56.139Z, ready 17:06:57.294Z. total 27.891 s, worker 23.218 s, compile 18.187 s (changed TU 17.750 s + identity 0.362 s), link 3.969 s (646 inputs / 506,848,427 B), scan 0.042 s, publish 0.343 s; 2 compiled / 645 skipped; no invalidations.
- Same-PID activation gen820 at 2026-11-28T17:06:57.100Z (old 819): run `20261128_082433` PID 26304 and run `20261128_084634` PID 5308, reason `candidate committed`. Request->activation 28.130 s; result->activation 0.961 s.
- gen820 resource: 27 samples @1s, mean host CPU 46.162%, peak 67.2%, min available 3,158,757,376 B (~3.01 GiB); sampler_setup_wall_ms 94 (vs 3,953 ms on gen642), process discovery async (4,869 ms wall, overlapping).
- gen820 GCC -ftime-report: parsing 10.06 s (65%), preprocessing 7.52 s (49%), template instantiation 1.84 s (12%), opt+generate 4.20 s (27%), lang deferred 1.13 s.
- Activation workload: `vehicle_freeplay`, `arena2`, 0 players / 0 NPCs on both activating servers (not the Juggernaut-horde acceptance scenario).
- c68ca8: 13 changed paths, header_fanout 41, failed at compile 34.563 s (total 54.594 s); `worker_errors` now retains `src/vehicles/vehicle-hud.cpp:209:21: error: 'THE_CAMERA' was not declared in this scope`.
- a0bac6: gen819, changed_paths only `src/vehicles/vehicle-hud.cpp`, 163 compiled / 484 skipped from prior header invalidations; total 490.843 s, compile 471.469 s, link 6.031 s, queue_wait_max 424.422 s; mean host CPU 98.37%, min available 90.4 MiB; same-PID gen819 activation 09:10:29.033Z.
- Postmortem fanout attribution inconsistent: a0bac6 header_fanout=null/headers=[] despite 163 dependents; c68ca8 header_fanout=41/headers=[]; 1d504b header_fanout=41/headers=[].

# Runtime evidence separation

- Source: current checkout and build artifact inspection only; no edit.
- Build evidence: gen820/819/817 attempts above from `.dev/build-timings/`.
- Runtime evidence: gen820 and gen819 same-PID `game.generation.activated` observed on already-live isolated servers in `logs/2026-11-28/20261128_082433` and `20261128_084634`. This proves same-PID DLL activation and that the vehicle-source change loaded; it does NOT prove the heavy-mode 15-20-file <=10 s acceptance.
- Human acceptance: none claimed.

# Regression review

- Regression entry appended: no. This is one bounded review plus one process proposal; no behavior change.

# Agent handoff context

- Owner: build-coordinator (issue owner); task owners cpp-latency-digest-cache (compile-time-report), root-mode3-latency (npc-behavior-root-candidate-dll), cpp-latency-pressure-profile (npc-one-tuple-pressure-probe).
- Next exact steps: (1) record or decline the compile-time-report task-proof against 5ccf06; (2) one coordinated 15-20 source batch on a pinned connected Juggernaut-horde pair measuring request->same-PID activation; (3) target the changed-TU frontend (parsing/preprocessing) or the candidate-module path, since one TU alone is 17.75 s at 46% CPU; (4) standardize postmortem header_fanout/headers.
- What another agent should not redo: re-deriving the gen820/819 stage timings or the same-PID activation; re-testing the assumption that host pressure causes the one-source ~28 s floor (it was measured at low pressure).
- Files to inspect: `.dev/build-timings/game-dll-20261128T170629149896Z-5ccf06.*`, `logs/2026-11-28/20261128_082433/server-01/events.jsonl`, `logs/2026-11-28/20261128_084634/server-01/events.jsonl`, task records in `devscripts/agent_coord.py` proof commands.

# Automatic evidence

- Branch/base/final: current shared checkout branch; no commit made by this session.
- Git status: extensive pre-existing modifications; this session only appended issue records and this changelog.
- Build identity: generation 820, `build\game\generation-820\game-g820.dll`, code hash `f29b47820547586ebdd7280ade602de5a09bdf6f7adfa86423f292fcd6ddb92a`.
- Generation: 820.
- Artifact: `build/game/generation-820/game-g820.dll`.
- Journal path: `logs/2026-11-28/20261128_082433/server-01/events.jsonl`, `logs/2026-11-28/20261128_084634/server-01/events.jsonl`, `logs/2026-11-28/20261128_092602/dev-loop/events.jsonl`.
- Session ID: session-199fa56faa16 / work-4e777880a2b0.

# How to reverse

- Uncommitted source file: none changed by this session.
- Committed change: none.
- Issue record append: remove only appended review id 573 and the appended issue-9a4503f6ed68 proposal.
- Generated file: delete this changelog.

# Related feature record

- Issue: `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`.
