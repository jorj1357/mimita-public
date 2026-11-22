# One-TU Live DLL Latency and Warm No-Op Measurements

- Task ID: npc-one-tuple-pressure-probe
- Session ID: work-d86eb778a7fc
- Owner: `cpp-latency-pressure-profile`
- Focus: hot-reload latency and build-stage measurement
- Summary: Built one bounded NPC tuning edit into game-DLL generation 479 and observed that generation in the existing server PID. Measured the full changed-source attempt, repeated warm no-op work, and CPU while the existing NPC server ran. The <=10s edit-to-live goal remains unproven; publication took 85.7s from dev-loop request acceptance.
- Status: incomplete; runtime tuning value and human-visible behavior remain unproven.
- Date, time, timezone: 2026-11-22T09:27:43Z, UTC
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base commit: `9fd637d595a308483a9f82c88349a5915c82a0ca`
- Final commit: `9fd637d595a308483a9f82c88349a5915c82a0ca` (working tree remains dirty)

## Pre-existing changes

- Exact status output: the shared checkout was already heavily modified across build drivers, packet/code generation, NPC/gameplay files, issue records, and changelogs. Those changes were preserved and were not attributed to this task.
- Files not created or modified by this task include the pre-existing edits to `devscripts/source_manifest.py`, `devscripts/game_build_worker.py`, `devscripts/dev_loop/game_build.py`, `build.py`, `build_game_dll.py`, `src/npc/npc-behavior-module.cpp`, and `src/npc/npc.cpp`.

## Requested behavior

Edit a small C++ behavior function, compile only what changed, publish a game DLL, and see the change in the already-running game without closing its EXE. Measure every stage and keep pursuing a <=10s update path.

## Specification alignment

- Current specification paths: `docs/specs/hotreload/hotreload.md`; `docs/specs/hotreload/hot-layer-latency-and-language.md`; `docs/specs/dev-loop/change-to-evidence-latency.md`.
- Exact requirements: the root dev-loop owns builds and runtime evidence; a published DLL is not runtime proof; hot behavior requires the same process and its exact journal; source/dependency freshness must be preserved.
- Why the change follows the specification: the normal root dev-loop built and published the candidate; the existing server PID remained alive and later reported the new game DLL generation.
- Conflicts or decisions: no EXE build, extra game instance, direct compiler, or second dev-loop was used. Required compile/link errors remain fail-closed. Optional report-only checks are recorded fail-open so they cannot invalidate an already successful primary build.

## Exact implementation changes

### File: `src/npc/npc-behavior.cpp`

- Lines/functions/headings: line 75, `applySkillTier()` elite profile.
- Old content or behavior: elite profile preferred engagement range was 7.0m.
- New content or behavior: preferred engagement range is 6.0m.
- Reason: bounded C++ behavior edit for a real one-translation-unit compile/link/publish/activation measurement. The live `fun_cs_rush` profile uses the elite tier and does not override `preferred_range` in its JSON profile.
- Why unrelated behavior is preserved: only the elite preferred-range literal changed; no ABI, schema, or build configuration changed.

## Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `src/npc/npc-behavior.cpp` | `preferredRange = 7.0f` | `preferredRange = 6.0f` | One-TU live DLL experiment | Restore only this line to `7.0f` and rebuild through the root dev-loop |
| This changelog | absent | evidence record | Required session record | Remove only this new file |

## Contract / ABI changes

- Contract: none.
- Version bump: none.
- Compatibility: no layout or ABI change.
- Reverse: restore the one tuning literal and publish through the root dev-loop.

## State / schema changes

- Domain: none.
- Old schema: none.
- New schema: none.
- Migration: none.
- Reverse: none.

## Deleted paths

- Path: none.
- Before: none.
- Why: none.
- Reverse: none.

## Diagnostics

- Owner/category: root dev-loop game-DLL timing stream; existing server `game.hotness.summary` event.
- Input: build note `bf4e7c4da1e6` was queued at 09:00:12.513Z; request `2026-11-22T09:00:34.694Z-5a29133a`; build attempt `game-dll-20261122T090034915948Z-7ffb1c`.
- Decision: built and published generation 479 from changed source `src/npc/npc-behavior.cpp`.
- Output: 2 TUs compiled (edited TU and generated identity provider), 641 skipped; `build/game/generation-479/game-g479.dll`; code hash `9167cfea884473eb023b29c4627862868eefbdcc3c1ad8ead02f19e044908830`.
- Failure or rejection reason: no build failure. The resolved preferred range is not emitted by current gameplay telemetry, and the live server has no players, so behavior acceptance is unavailable from this run.
- Rate limiting: existing approximately once-per-second game hotness heartbeat; no new diagnostics were added.

## Validation

- Focused skill paths and results: hot-reload and dev-loop routes inspected; coordination issue `issue-136d4553ff92` updated with evidence and next steps.
- Tests and exact commands: a temporary source-inventory cache experiment passed `python -m unittest devscripts.quality.test_source_manifest_cache` (3 tests); its code/test change was reverted because it produced no warm no-op speedup and narrowed inventory coverage. No test result is claimed for that discarded experiment.
- Build status: root dev-loop request succeeded; generation 479 published. No full EXE build ran.
- Runtime or hot-reload evidence: the existing server PID 26748 later reported `game.hotness.summary` generation 479 with 19 NPCs and 0 players. Earlier records from the same PID reported generation 478. This confirms the new DLL generation in the same server process, but there is no explicit activation event/time and no human-visible aggression proof.
- Output files: `.dev/build-timings/game-dll-20261122T090034915948Z-7ffb1c.jsonl`; `.dev/build-timings/game-dll-20261122T092117950755Z-ed7eef.jsonl`; server journal `logs/2026-11-22/20261122_011031/server-01/events.jsonl`.

## Measured evidence

- One-source attempt: 85.019s build attempt; 85.735s root dev-loop waiter; publish at 2026-11-22T09:02:00.542Z. The file's exact save time was not captured; the build snapshot/change report was generated at 09:01:02.612Z. First observed heartbeat for generation 479 was at 09:03:05.058Z, so it is a latest-by observation, not a precise activation timestamp. Request-to-publish is measured; save-to-activation still needs both endpoints instrumented.
- Changed TU: `src/npc/npc-behavior.cpp` compile 24.145s. `changed_source_preflight_seconds=24.374` is this same changed-TU compile plus about 0.23s orchestration; do not add it again to aggregate compile time (25.667s).
- Object cache and link: object scan 5.810s. Dependency-index prepass 4.690s across 642 depfiles, of which path/fanout normalization was 3.839s. Freshness checks took 0.904s for 641 cached objects; all 641 were skipped. Link response preparation took 0.544s for 644 inputs totalling 480,616,000 bytes; GNU link took 20.914s and produced a 242,706,324-byte DLL. Publish took 0.508s.
- Before-worker stages: resident-state codegen 0.658s; packet-registry codegen 1.445s; manifest snapshot 6.916s; manifest validation 0.078s; source snapshot 10.051s. Changed attempt total is not sub-10s.
- Warm no-op: request `2026-11-22T09:21:17.694Z-c32a92b6`, attempt `game-dll-20261122T092117950755Z-ed7eef`; 11.928s attempt / 12.125s waiter, no compiled TUs and no link. Manifest 5.559s; codegen 2.313s; validation 0.101s; source snapshot 0.086s. The manifest considered 1,008 C++ files and 1,217 headers; digest cache had 2,225 hits, zero misses, and zero files rehashed. It still spent 1.919s enumerating C++ paths and 1.385s / 1.664s in source/header digest passes.
- Warm no-op worker detail: dependency index prepass was skipped because the attempt made no generation. Worker log phases recorded initial source coherence 1.416s, toolchain setup 0.402s, identity/cache 0.027s, object-cache scan 0.083s, and no-op decision 0.011s. `worker_launch_to_first_output` was 2.697s; it spans worker startup and overlaps the worker phases, so it must not be added to them. Link and compilation were skipped entirely.
- CPU, 12 logical / 8 physical CPUs: 5-second live-server/no-build sample averaged host 69.8% and server 468.9% core-scaled (282 threads, 426MiB RSS). A 15-second warm no-op request averaged host 76.8% and the same server 474.5% core-scaled. This is correlation, not proof that the build caused the host-CPU increase. The server had 19 NPCs and 0 players; this is not a Juggernaut/AI-controller pressure run.
- Cache experiment: temporarily excluding root `config/` and bumping inventory schema caused one cold inventory scan (16.085s manifest function, 9.072s enumeration); the next warm no-op was 10.867s and did not beat the earlier 10.143s pass. The experiment was reverted because it showed no speedup and narrowed source discovery. The full-scope schema-1 cache was regenerated and verified.

## Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: no regression was observed. The tuning change lacks gameplay acceptance, so it is not marked behavior-complete.
- Related regression paths: none.

## Human acceptance

- Visual review: not performed.
- Gameplay review: not performed with a human/player target.
- Multiplayer review: not performed.
- Still unverified: actual NPC behavior effect; exact save-to-activation latency; controlled Juggernaut pressure; CPU attribution; sub-10s changed-source path.

## Agent handoff context

- Owner: build-coordinator / C++ latency issue owners.
- Focus: continue `issue-136d4553ff92`; compare one-source attempts under quiet and controlled AI pressure; avoid reopening the reverted config-scope cache change.
- Files to check: `devscripts/game_build_worker.py:1402-1469` for dependency-index prepass; `devscripts/game_build_worker.py:1505-1598` for object freshness; `devscripts/game_build_worker.py:1820` for changed-source preflight; `devscripts/game_build_worker.py:2036-2076` for full GNU link; `devscripts/source_manifest.py:554-665` for manifest source/header enumeration and hash-cache hits.
- What another agent should not redo: the generation 479 one-TU attempt, its warm no-op, and the five-/fifteen-second host/server CPU sample are recorded above and in the issue.
- Next exact step: add a bounded profile-resolved event (or equivalent existing owner telemetry) that records the actual `preferredRange` for the elite `fun_cs_rush` NPC after DLL activation, then prove the behavioral effect in a permitted live player scenario. Separately profile a persistent source/dependency index that avoids per-attempt directory/file stats and an architecture that avoids relinking 644 objects.

## Automatic evidence

- Branch/base/final: `checkpoint/one-checkout-condensation-20261111`; base and final commit `9fd637d595a308483a9f82c88349a5915c82a0ca` (dirty shared working tree).
- Git status: many pre-existing modified/untracked paths; this task changed only `src/npc/npc-behavior.cpp` plus this changelog. The temporary `source_manifest.py` / cache-test edit was reverted.
- Diff stat: one C++ line changed by this task.
- Build identity: root dev-loop build authority; EXE build identity remains `2026-11-21T02:26:50.306117Z:8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871`.
- Generation: 479.
- Artifact: `build/game/generation-479/game-g479.dll`.
- Journal path: `logs/2026-11-22/20261122_011031/server-01/events.jsonl`.
- Session ID: `work-d86eb778a7fc`.

## How to reverse

- Uncommitted source file: restore only `src/npc/npc-behavior.cpp` line 75 to `7.0f`; preserve unrelated edits.
- Deleted file: none.
- Committed change: none.
- Generated file: none.
- ABI/state/schema change: none.
- Reverse only files touched by this session. Preserve pre-existing changes.

## Related feature record

- Feature path: hot-reload latency issue `issue-136d4553ff92`.
