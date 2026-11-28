// 2026-11-28 09:20 UTC
/* purpose
* record one bounded independent issue-review activation
* append an evidence/counterevidence/next-check review to issue-1d1b9c62ac08
* append one non-duplicate confirming-evidence note to issue-91e3384b219b
* no source, build, close, run, or fix claim by this session
*/

# Independent bounded review of issue-1d1b9c62ac08

- Task ID: wake-b09ff351cfef
- Work session: work-4f4bad9c96bb
- Agent: issue-review-sweeper
- Focus: one read-only review of "Payload Arcade NPCs fall through the world": readiness, proof status, current generation identity, and the newest surviving payload-gamemode/NPC journals
- Summary: Readiness is complete (`missing=[]`, ready=true, 1 open task `payload-arcade-npc-grounding`, state `in_review`); `proof list --issue` is still empty. The candidate collision inputs are byte-identical across the current worktree, the gen-819 published snapshot, and the adjacent gen-809 snapshot. New since the prior review: retained payload-gamemode NPC runs now exist. Run `20261128_062046` (launch_mode 15, mode/gamemode payload with `features.arcade_collision=true`, map `mimita-pipes`, `module_generation=808`) shows 168 `npc.spawn.resolved`, then 87 `npc.below-map.respawned` across 21 NPC ids; a representative actor free-falls from feet z~840 to z~598 with `candidate_count=0` and `world_contact=false` at every sampled tick before the below-map guard respawns it. This reproduces the reported "NPCs fall through the ground in payload arcade" class on the current collision source, but on `mimita-pipes`, not the issue's `funworld3`; the exact issue scenario is still unrun (all funworld3 run.json records lack an events journal). Appended review id 572 and one non-duplicate confirming-evidence note to `issue-91e3384b219b` (existing review-to-execution proposals already cover the gap, so no duplicate proposal was filed). No source edit, build request, run, close, or fix claim.
- Status: complete (review only)
- Date, time, timezone: 2026-11-28T09:20Z, ISO 8601

# Pre-existing changes

- The shared checkout contains extensive pre-existing modifications from other agents (config files, `docs/issues/*`, `devscripts/agent_coord.py`, and many changelogs). This session edited no source or configuration, reverted nothing, and touched only the issue records through `agent-coord.py`, coordination state, and this changelog. No compiler, `build.py`, module worker, second dev-loop, or game runtime was invoked.

# Requested behavior

Run one bounded independent review of `issue-1d1b9c62ac08`: inspect readiness/proof/linked artifacts/latest journals; append one evidence-grounded review with observed facts, inference/uncertainty, contrary evidence, and the next discriminating check; then a short development-process review with one specific, measurable proposal to the best existing improvement issue (or state none). Do not edit source, build, close, or claim a fix.

# Specification alignment

- AGENTS.md gates 0b/0c/0d, 2, 4: coordination begin/observe, observations separated from inference, no compiler/build/second dev-loop in a review wake.
- `docs/specs/dev-loop/agent-proof-and-verification.md`: a build is not behavior proof; acceptance requires a pinned non-zero-generation journal.
- `docs/specs/dev-loop/agent-wake.md`: a woken agent is a bounded activation that checks proof state before ending.

# Exact implementation changes

None. No source, specification, or configuration change.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/bug/20261121-payload-arcade-npcs-fall-through-the-world-issue-1d1b9c62ac08.md` | reviews 61/215/305/407/464/517 | review 572 (unverified) appended | durable independent review evidence | remove only appended review text |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` | existing review-to-execution proposals | one confirming-evidence discussion note appended | seventh instance; execution gap, not capability | remove only appended note |
| `.dev/agent-coord/brain.db` | no review id 572 | review id 572 + discussion note | coordination record | restore from backup if withdrawn |
| `docs/changelog/2026-11-28/20261128_0920-independent-issue-review-issue-1d1b9c62ac08.md` | absent | added | one final session changelog | delete file |

# Validation

- `python devscripts/agent-coord.py begin`, `sync`, `observe`, `proof list`, `issue readiness`, `issue review`, `issue suggest`, and `end`.
- Read-only artifact checks: `build/game/current.json`, `.dev/build-snapshots/game-dll-20261128T090217845310Z-a0bac6/source` and `...-20261128T063600310452Z-db408b/source`, current `src/network/server-npcs.cpp` + `src/physics/movement/physics-collision-glb-*.cpp` + `src/network/server-infinite-collision.cpp` hashes, `logs/2026-11-28/*/run.json`, `logs/2026-11-28/20261128_062046/server-01/events.jsonl`, `logs/2026-11-27/20261127_231557`, `logs/2026-11-28/20261128_054617_01`, `config/gamemodes/payload.json`, and `devscripts/dev-launch-modes.json`. No build or run requested.

# Measured evidence

- Readiness: ready=true, score 8/8, missing=[], state in_review, 1 open task `payload-arcade-npc-grounding`. Proof records: none (`proof list --issue` = []).
- Current build: generation 819, code hash `5f942a3617323fe963306705521e47ed8b3ef3faa03b531d3954645a5def652f`, revision `4e0a2ba...`, published 2026-11-28T09:10:28.619Z; change report only `src/vehicles/vehicle-hud.cpp`.
- Collision hashes equal across worktree, gen-819 snapshot, and gen-809 snapshot: `server-npcs.cpp 2e37709f8e52...`, `physics-collision-glb-main.cpp 0abedb32ac21...`, `physics-collision-glb-sweep-slide.cpp 6b5fd3cec1d3...`, `server-infinite-collision.cpp 7088255dd484...`.
- `20261128_062046` (payload/mimita-pipes, launch_mode 15, `module_generation=808`): 168 `npc.spawn.resolved`; 87 `npc.below-map.respawned` across 21 ids (map_floor_z=606.29, from z 583.4-600.0, respawn target z~845-903); 6802 `npc.collision.fall-sample` (candidate_count=0 in 3441, world_contact=false in 5747, 23 actors); `npc.trace` 1183 grounded / 1393 world_contact nested entries.
- NPC-100022 descent: feet z 840.6 (cand 2) -> 830.3 (0) -> 801.6 (0) -> 755.4 (0) -> 691.8 (0) -> 610.8 (0) -> below-map from 598.4 at t=680.56; velocity to -175 m/s, sweep_hits=0.
- `20261128_063715` (payload gen 809) invalid: exit_code 1, client did not connect, 0 spawns. Funworld3 run records (`20261127_155327`, `_081812`, `_155356`) have no `events.jsonl`. Mode 57 (`Payload - funworld3 NPC behavior`, `--npcs 19`) remains unrun.

# Runtime evidence separation

- Source: current checkout and gen-819/gen-809 snapshot inspection.
- Build evidence: none claimed; the cited run uses an existing gen-808 artifact with an unpinned DLL SHA.
- Runtime evidence: the payload-gamemode below-map falls and descent series are real and observed in `20261128_062046`; they do not cover `funworld3` and do not prove a fix. Cause (gather/broadphase miss vs unauthored floor) is not yet separated.
- Human acceptance: none claimed.

# Regression review

- Regression entry appended: no. This is one bounded review plus one process-evidence note; no behavior change.

# Agent handoff context

- Owner: payload-arcade-collision-fix (task `payload-arcade-npc-grounding`).
- Focus: exact funworld3/mode-57 payload run, then classify the descent cause.
- Files to check: `src/physics/movement/physics-collision-glb-main.cpp:615-646` (fall-sample `candidate_count`), `src/network/server-npcs.cpp:2971-3002` (below-map guard), `devscripts/dev-launch-modes.json` mode 57.
- What another agent should not redo: re-deriving that the candidate collision code is unchanged across recent generations; re-running the invalid gen-809 payload run.
- Next exact step: `python devscripts/dev-loop.py --start-test-run --test-profile npc-navigation --test-launch-mode 57 --test-game-generation 819`, then require `run.json game_generation=819`/`game_dll_sha256=5f942a36...`, `npc.phase-window.module_generation=819`, `npc.spawn.resolved`, and positive `npc.trace`; fail on any `npc.below-map.respawned` or sustained negative-z series.

# Automatic evidence

- Branch/base/final: current shared checkout branch; no commit made by this session.
- Git status: extensive pre-existing modifications; this session only appended issue records and this changelog.
- Diff stat: not attributable to this session (no source edits).
- Build identity: published generation 819, `game-g819.dll`, code hash `5f942a36...`.
- Generation: 819.
- Artifact: `build/game/generation-819/game-g819.dll`.
- Journal path: `logs/2026-11-28/20261128_062046/server-01/events.jsonl` (payload/mimita-pipes, gen 808).
- Session ID: session-445b4f0f6ba0 / work-4f4bad9c96bb.

# How to reverse

- Uncommitted source file: none changed by this session.
- Committed change: none.
- Issue record append: remove only the appended review id 572 and the appended issue-91 discussion note.
- Generated file: delete this changelog.

# Related feature record

- Issue: `docs/issues/bug/20261121-payload-arcade-npcs-fall-through-the-world-issue-1d1b9c62ac08.md`.
