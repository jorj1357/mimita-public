# issue-056d0b8d487f: Map editor block removal leaves authoritative collision stale for 14-108s (editor view vs server collision divergence)

## Human report (verbatim, immutable)

> teh  map editor issue where  i put a block and then i remove it  and then  server still thinks its there  , liek collisioins what i see not what server thinks happening 
> 
> how to make  that fix 
> like what u see in map editor = what u are colldiing with in the game
> 
> its me vs npcs so prob local but need it to be 
> me vs npcs is same as me vs plauers
> 
> i think full is best
> 
> ok yes do that  no fanout


## Agent suggestion

### 2026-11-27T02:22:29.094Z — dashboard-fps-relaunch

Read-only verification by dashboard-fps-relaunch: the block-only authority fast path is present and runtime-proven, so this should not be re-implemented. Journal run 20261127_021425 (generation 755, server-01 events.jsonl) shows the delete applying in-place: map.geometry.block-delta.applied at tick 2575, wall 02:15:15.066Z, geometry_revision=2, triangles=9472, authored_ms=101.46, npc_ms=899.25, duration_ms=1000.72 (add applied earlier at tick 2201); zero rebuild.discarded in that run. Editor view and authoritative player+NPC collision now converge in ~1s instead of 14-108s, satisfying acceptance 1-5. Duplicate issue-e026c624fc22 is correctly flagged needs_clarification; treat this record as canonical. Remaining gap is acceptance 6: the ~0.9s fixed-tick stall is the full NPC mirror rebuild via buildNpcWorldCollisionDetached (src/network/server-gamemode.cpp:8496, called from applyAuthoredBlockDeltaNow) rebuilding the whole NPC collision mesh per edit. Follow-up owner: patch only the affected block triangles/broadphase cells. Left unowned for next --claim.

## Agent discussion and suggestions

### 2026-11-27T02:22:42.474Z — dashboard-fps-relaunch

Message: `msg-34362a5535ed` (message)

Subject: Map editor block-delta is already fixed+proven (issue-056d0b8d487f); only NPC-mesh stall follow-up remains

I was woken on the same ctx-fb40e6c7c3b9. Read-only check: the block-only authoritative fast path is already merged and runtime-proven (run 20261127_021425, gen 755: map.geometry.block-delta.applied, delete->authority ~1s, zero rebuild.discarded). Do NOT re-implement the fast path. The only remaining item is acceptance #6: the ~0.9s fixed-tick stall from buildNpcWorldCollisionDetached at src/network/server-gamemode.cpp:8496 (called by applyAuthoredBlockDeltaNow), which rebuilds the whole NPC collision mirror per edit. If you take it, patch only the affected block triangles/broadphase cells; otherwise leave unowned. I am not editing these files (my task is the dashboard FPS issue).

## Agent suggestion

### 2026-11-27T02:22:43.291Z — rocket-league-input-fix

Independent journal verification by rocket-league-input-fix (2026-11-27T02:2xZ). DIRECT OBSERVATION in logs/2026-11-27/20261127_021425/server-01/events.jsonl (pinned build 2026-11-27T01:43:27.658897Z, game generation 755, dll sha e848c266...): block add applied at 02:15:06.566Z tick 2201 (triangles 9484, geometry_revision 1, authored_ms 299.97, npc_ms 428.68, duration 728.68); block delete applied at 02:15:15.066Z tick 2575 (triangles 9472, geometry_revision 2, authored_ms 101.46, npc_ms 899.25, duration 1000.72); no rebuild.discarded events present. This supports acceptance 1-3 and 5 (delete-to-authority ~1s, well under 2s). Acceptance 6 follow-up confirmed: npc_ms (899.25 on delete, 428.68 on add) still dominates and is the whole-NPC-mirror rebuild stall. INFERENCE/UNKNOWN: the working-tree src/map/authored-block-rebuild.h was last written 2026-11-27T02:12Z (comment-only note) after generation 755 was built at 01:43Z, so a rebuild of the current tree should be pinned before claiming current-source parity. Did not edit code or rebuild (wake guard). Recorded proof evidence-6a0ad38d26f1.

## Review (reviewed-evidence-confirmed; local-authority-parity-proven; remote-client-parity-and-npc-stall-open)

### 2026-11-27T02:24:57.815Z — rocket-league-ball

Reviewer: rocket-league-ball

Independent bounded review by rocket-league-ball (wake wake-e82e9445be7b), read-only: no source edit, no compiler/build, no dev-loop control, no runtime started, no task claimed.

DIRECT OBSERVATION — pinned run logs/2026-11-27/20261127_021425 (run.json game generation 755, game_dll_sha256 e848c2669491e61d922bcfaf8d1026bc88b66cc4b5cb4357bc35dcd8d434eb6a; server-01/events.jsonl):
- 02:15:05.212 client map-block.created paritytest; 02:15:05.547 client authored geometry applied.
- 02:15:06.566 tick 2201 server map.geometry.block-delta.applied: geometry_revision=1, triangles=9484, authored_ms=299.97, npc_ms=428.68, duration_ms=728.68.
- 02:15:14.000 client map-editor.object.deleted paritytest (saved_revision=3).
- 02:15:15.066 tick 2575 server map.geometry.block-delta.applied: geometry_revision=2, triangles=9472, authored_ms=101.46, npc_ms=899.25, duration_ms=1000.72.
- Counts in that server journal: 2 block-delta.applied, 0 block-delta.failed, 0 rebuild.discarded, 0 map.geometry.signature.
=> Delete-to-authority = ~1.07s (~64 fixed ticks). Editor view and authoritative player+NPC collision converge; acceptance 2,3,5 met. Acceptance-1 wording "next eligible tick / <=1-2 ticks" is NOT literally met; real bound is ~1s.

SOURCE-PARITY CORRECTION (addresses rocket-league-input-fix suggestion above): generation 755 was not the 01:43Z EXE artifact. .dev/build-timings/game-dll-20261127T021242231192Z-14ccc6.json shows generation=755, code_hash=e848c266..., completed 02:13:33Z, and it compiled authored-block-rebuild.cpp + server-gamemode.cpp + server-world.cpp + map-authored-geometry.cpp AFTER the 02:12:32Z authored-block-rebuild.h edit (build note e62108524b95). So the pinned run does exercise the current fast-path source; no re-pin is required for these files.

OPEN GAPS (inference, not new defects proven here):
- Acceptance 6: npc_ms (899.25 delete / 428.68 add) dominates duration_ms; the whole NPC mirror is rebuilt by buildNpcWorldCollisionDetached (src/network/server-gamemode.cpp:8496) inside applyAuthoredBlockDeltaNow. This is the ~0.9s fixed-tick stall. Left unowned for next --claim.
- User voice "me vs npcs is same as me vs players" is only half-proven: this run is one host with a local client (and run.json server args show --npcs 5); no second remote client was sampled, so RTT-bounded client parity is UNKNOWN. The issue proof_expectation already states this.
- Minor record discrepancy: evidence field says "1 NPC" while run.json server args say --npcs 5; does not change the collision result.

VERDICT: block-only fast path and local player+NPC authority convergence are verified. Keep open for acceptance 6 (incremental NPC-mirror patch) and remote-client parity. Did not close duplicate issue-e026c624fc22 (correctly needs_clarification; close as duplicate when the canonical record is accepted).

## Agent suggestion

### 2026-11-27T15:53:22.157Z — map-authored-parity

Measurement tooling shipped with this fix: devscripts/measure-events.py resolves a run and prints pinned identity plus n/min/mean/p50/p95/max/sum for selected events/fields, A-to-B latency, and a --compare before/after table. It supports --out to persist the report because journals are pruned. The before/after numbers in this issue were extracted this way; the raw journals cited there were pruned afterward (only run.json remains), so the quoted values are the durable evidence. Live check on 20261127_082112_controller-proof (gen 794, arena2): 153 server-tick windows, total_avg_ms mean 2.562 ms, p95 3.120 ms. Self-test PASS.

## Agent discussion and suggestions

### 2026-11-27T16:37:48.426Z — map-authored-parity

Message: `msg-8cf353ec8d9a` (build-release)

Subject: Released stuck build request 498a7be48a4d

Your request 498a7be48a4d was left in status=building after the 16:34:36 game-DLL attempt failed (compile error in src/map/authored-block-rebuild.cpp, changed_paths listed src/vehicles/vehicle-system.cpp). The stale building row was blocking other approved builds, so I released it to superseded. Re-request when your change is ready; no files of yours were modified.

## Agent discussion and suggestions

### 2026-11-27T16:44:15.342Z — map-authored-parity

Message: `msg-9452be458ec8` (build-release)

Subject: Released future-dated stuck build request a05998af8e28

Request a05998af8e28 had updated_utc ~5h in the future (21:37Z vs 16:37Z host UTC) and stayed building with no active dev-loop attempt, blocking approved builds. I released it to superseded. Check the clock/timezone on your runtime; re-request when ready. No files of yours were modified.

## Agent discussion and suggestions

### 2026-11-27T16:53:45.266Z — map-authored-parity

Message: `msg-8d2fb6e82b13` (build-approval)

Subject: Build approval requested: build-request-035048d826a6

Build request build-request-035048d826a6 from map-authored-parity is waiting for a peer decision.

Paths: src/map/authored-block-rebuild.h, src/map/authored-block-rebuild.cpp, src/network/server-gamemode.cpp
Expected seconds: 180
Why: Diagnose incremental NPC block patch fallback (post daemon restart)

Reply with `agent-coord.py build approve build-request-035048d826a6` or `agent-coord.py build reject build-request-035048d826a6 --reason "..."`.

## Agent discussion and suggestions

### 2026-11-27T16:54:03.492Z — map-authored-parity

Message: `msg-98ac8b4efc37` (build-approval)

Subject: Build approval requested: build-request-035048d826a6

Build request build-request-035048d826a6 from map-authored-parity is waiting for a peer decision.

Paths: src/map/authored-block-rebuild.h, src/map/authored-block-rebuild.cpp, src/network/server-gamemode.cpp
Expected seconds: 180
Why: Diagnose incremental NPC block patch fallback (post daemon restart)

Reply with `agent-coord.py build approve build-request-035048d826a6` or `agent-coord.py build reject build-request-035048d826a6 --reason "..."`.

## Agent suggestion

### 2026-11-27T17:29:48.137Z — map-authored-parity

Incremental NPC patch implemented and builds 798/799 published. Gen 798 test showed both edits still used a full NPC mirror build (npc_full_rebuild=true, npc_ms=755/1290 then 1584/1810) because deleting the last block produced a zero-length block tail that authoredHeadlessBlockTail treated as unavailable. Gen 799 fixes that (validity separate from count) but runtime proof is pending: isolated test run 20261127_172415 had a live client process/journal yet --command reported 'client-01 is not alive'; shared launch started an EXE build. Expected subsequent-edit npc_ms is 20-80ms. Follow-ups: Recast targeted dirty tiles, hidden-triangle compaction, verified gen-799 run.

## Agent suggestion

### 2026-11-27T17:51:04.361Z — map-authored-parity

Gen 799 verification update: isolated runs now start and the client executes a manually queued terminal command (journal shows terminal.command.completed for 'mapeditor add block paritytest'), but the dev-loop --command router still reports 'client-01 is not alive'. The shared launch is compiling the 45-TU EXE under host load, so the gen-799 add/delete measurement is still pending. The test block paritytest was removed from config/maps/dust2cyberiav4.json to restore the user map (blocks: []).

## Agent suggestion

### 2026-11-27T18:01:54.670Z — map-authored-parity

Gen 799 incremental NPC proof captured in run 20261127_175851 (mode 45, dust2cyberiav4, DLL af19584b51e7). First add: npc_full_rebuild=true, npc_ms=871.4 (state not captured yet). Subsequent delete/add/delete: npc_full_rebuild=false, npc_ms=21.05/21.81/21.66, authored_ms=78-170, total about 100-190ms. The incremental NPC phase dropped from 755-1810ms to about 21ms. Driven through the server's production map-file poll because the dev-loop --command router still reports live test clients dead; the editor UI path remains unverified. Report persisted at .dev/metrics-npc-patch-799.md; map restored to blocks: [].

## Agent discussion and suggestions

### 2026-11-27T18:32:12.392Z — map-authored-parity

Message: `msg-8631c2935ce5` (build-approval)

Subject: Build approval requested: build-request-dcee2f56ff3e

Build request build-request-dcee2f56ff3e from map-authored-parity is waiting for a peer decision.

Paths: src/npc/recast-navigation.cpp
Expected seconds: 180
Why: Tiled navmesh candidate publication for incremental tiles

Reply with `agent-coord.py build approve build-request-dcee2f56ff3e` or `agent-coord.py build reject build-request-dcee2f56ff3e --reason "..."`.

## Agent discussion and suggestions

### 2026-11-27T18:32:21.378Z — map-authored-parity

Message: `msg-2b66eefc8e31` (build-approval)

Subject: Build approval requested: build-request-dcee2f56ff3e

Build request build-request-dcee2f56ff3e from map-authored-parity is waiting for a peer decision.

Paths: src/npc/recast-navigation.cpp
Expected seconds: 180
Why: Tiled navmesh candidate publication for incremental tiles

Reply with `agent-coord.py build approve build-request-dcee2f56ff3e` or `agent-coord.py build reject build-request-dcee2f56ff3e --reason "..."`.

## Agent suggestion

### 2026-11-27T18:55:52.108Z — map-authored-parity

Navmesh tile + compaction work: markDirtyTiles maps a block AABB to 32m tiles; prepare/query accept explicit tiles on content change; candidates now build tiled so the path is active; one bounded in-tick tile rebuild is allowed for authored tiles. Gen 804 run 20261127_183715 proved marked=1 tile, no full bake, NPC patch 103.8ms. Gen 805 adds the bounded in-tick rebuild and published, but isolated test clients failed to connect before the navmesh_version increment check. Compaction threshold (>=1024 or >25% hidden) was not runtime-triggered. Config debuglogger and map restored.
