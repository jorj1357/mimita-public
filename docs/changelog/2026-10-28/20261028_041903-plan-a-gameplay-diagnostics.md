# Plan A gameplay diagnostics and fixes

Time created: 2026-10-28T04:19:03Z
Time last updated: 2026-10-28T12:05:15Z

Branch: `20261011zt`
Starting HEAD: `ada6a0d0`
Completion commit: none (left uncommitted for review)

## Request

Implement Plan A only:

1. Update the gameplay issue tracker.
2. Add 200 ms performance evidence and find the first growing cost/resource.
3. Remove map-derived player correction and enforce the 1e9 sanity limit.
4. Trace and fix excessive bouncing.
5. Trace NPC disappearance/freeze while still acting.
6. Trace and fix Mace swing overwrite, preserving movement-cancel behavior.
7. Validate through the dev loop, without implementing Plan B networking reliability.

## Tracker

Updated `docs/specs/open-gameplay-issues.md`:

- existing performance rows now require 200 ms slices, memory, allocation, and
  animation/resource evidence;
- existing movement/jitter rows now specify no map-derived correction, travel
  outside map bounds, and only the 1e9 sanity rejection;
- added excessive-bounce, NPC disappear/freeze, and Mace overwrite entries;
- added matching index rows.

All causes remain unconfirmed unless the matching runtime run identified the
path.

## Performance evidence

Added:

- `performance.frame-slice` every ~200 ms with:
  - frame p50/p95/p99/max, over-budget count
  - top slow scopes
  - top allocation scopes
  - working set/private/pagefile bytes
  - allocations, effects, blood, corpses, NPCs, projectiles, audio
  - animation set/clip cache, avatar cache/pending loads, model upload cache
- `performance.session-growth` every 30 s with retained-memory and allocation
  deltas.
- `perf-evidence.py` support for the new slices, session growth, movement
  corrections, bounce responses, NPC traces, and Mace traces.

Runtime finding:

- Clean runs still show recurring allocation/CPU spikes in
  `Network::SnapshotApply`/`Network::ReceivePoll`, `Replay`, and
  `RagdollCorpseUpdate`.
- Private bytes grew about 270–320 MB over 60–120 s while working set grew about
  60–280 MB in the inspected runs.
- This identifies candidates, not a confirmed cause. Performance is not claimed
  resolved.

Reductions made:

- `anim.state.changed` no longer embeds the candidate array unless
  `anim_priority_debug` is enabled.
- Snapshot membership scratch uses reusable vectors instead of per-snapshot
  unordered maps.
- ICE/UDP receive polling reuses packet vectors through a persistent client
  receive buffer.

## Movement correction

Changed `src/network/movement-validation.h/.cpp`:

- removed map-derived `worldBoundsMin/Max/Padding` policy from movement
  validation;
- `makeMovementValidationConfig()` no longer reads `HeadlessWorld` bounds;
- replaced `OutOfBounds` with `SanityLimit`;
- positions beyond `config.sanityPositionLimit` (1e9) are rejected, not clamped
  to a map;
- counters use `sanityLimitCorrections`.

Changed `src/network/server-packets.cpp`:

- teleport no longer clamps to `world.boundsMin/Max ± 2`;
- non-finite or beyond-1e9 teleport requests are rejected and logged;
- valid outside-map teleport positions are accepted unchanged;
- added bounded `movement.server-position-corrected` records with reason,
  reported/accepted positions, correction vector, input axes, velocities, and
  collision reason.

Runtime evidence:

- `logs/2026-10-28/20261028_040240/events.jsonl`:
  `teleport 50000,0,0` produced an `avatar.pose.sample` at `x=50000` and no
  sanity/map correction.
- The later return teleport produced a `blocking-geometry` correction, which is
  the remaining separate collision rule.

## Bounce

Added `collision.bounce.applied` with contact identity, normal, penetration,
incoming/outgoing velocities and impulses, impact, configured strength/friction,
min speed/max speed/min push/cooldown, and source path.

Evidence showed the dominant server path was an unidentified root/ground
contact with `actor_part=""`, `world_triangle=-1`, and an upward normal.

Fix: ordinary root/walkable-ground contacts now project instead of bouncing.
Configured bounce remains for identified limb/body/weapon contacts, walls,
ceilings, slopes, and rolling avatars.

Runtime after the fix still shows limb/body, wall, slope, and dynamic contact
bounce events. Human motion acceptance remains open.

## NPC trace

Added `npc_trace <entityId|off>`.

New bounded diagnostics:

- `server.npc.lifecycle.trace`: spawn/removal, avatar/transform/health changes,
  snapshot inclusion.
- `client.npc.trace`: focused client state, presence, model/avatar binding,
  render gate, bounds, camera distance.
- `client.npc.trace` with `present_in_remote_npcs=false` when the focused NPC is
  absent from the client map.
- `client.npc.damage-visible`: attacker id and client presentation state when an
  NPC damages the local player.

Runtime: a TDM NPC that damaged the player was present, model-loaded, and
render-gate passed. The reported disappear/freeze case was not reproduced in
this run, so no behavioral fix is claimed.

## Mace animation

Added `mace.anim.trace` for active Mace actions with locked clip, sample tick,
selected state/priority, selected weapon pose, weapon-profile ownership, ground
and movement state.

Changed movement cancellation:

- `ClientActionState` now records whether movement was already pressed at
  action start.
- a Mace swing is cancelled by a **new** movement input, jump edge, dash,
  down-dash, freeze, hurt, death, or respawn;
- continuing walk/run movement and crossing a speed threshold no longer
  overwrite the swing.

Runtime:

- `logs/2026-10-28/20261028_040240/events.jsonl` showed
  `mimita_sizing_6_attackslam_002` selected through windup, active, and
  recovery, with `active_clip_in_weapon_profile=true`.
- A midair start still cancels on the airborne state change; a grounded idle
  swing completed.
- Human confirmation of the visible swing and movement-cancel hitbox stop
  remains required.

## Performance continuation — 2026-10-28 12:05 UTC

Additional changes after the first Plan A pass:

- `src/audio/audio.cpp`
  - sound-file reads now run on a background cache worker;
  - a cache miss requests the file and leaves the current one-shot silent
    instead of blocking the gameplay thread on file I/O;
  - the combat-sound preload list is queued to the worker.
- `src/replay/replay-export.h`, `src/replay/replay-export-json.cpp`,
  `src/replay/replay-factory.cpp`, `config/replayexport.json`
  - `autoRecord` is opt-in; the 60-second recording ring is no longer started
    automatically for every match;
  - `autoSaveKillClips` is opt-in; busy matches no longer snapshot an 8-second
    replay clip for every kill;
  - manual replay recording/save and round-winning clip save remain available.
- `src/network/multiplayer-tick.cpp`
  - snapshot membership uses reusable vectors;
  - client ICE/UDP receive loops reuse persistent packet buffers.
- `src/perf/perf-frame.cpp`
  - 200 ms slices now emit top allocation scopes as well as top slow scopes.

Runtime results:

- Best inspected post-change run:
  `logs/2026-10-28/20261028_120254/events.jsonl`, build `3596`.
  Recent windows averaged p95 10.088 ms and p99 26.649 ms; maximum simulation
  31.747 ms.
- `logs/2026-10-28/20261028_115030/events.jsonl` and
  `logs/2026-10-28/20261028_115845/events.jsonl` were contaminated by
  concurrent Plan B networking edits and showed large
  `Simulation::SimulateTick`/`Combat::HitResolve` spikes.
- 240 FPS for 60 seconds was **not achieved**. The best busy-scene run
  inspected after the audio/replay changes was p95 10.088 ms / p99 26.649 ms;
  the final low-actor run reached p95 2.235 ms / p99 5.323 ms but is not the
  requested busy scene.
- Remaining candidate costs: client snapshot apply on join (one large
  `Network::SnapshotApply` allocation spike), per-tick simulation/combat
  (`Combat::HitResolve` 16–26 ms spikes), and corpse updates. A clean rerun
  after concurrent networking edits stop is required to identify the next
  confirmed owner.

## Validation

- Build `3558`, `3563`, `3566`, `3568`, `3570`, `3572`, `3576`, `3579`,
  `3592`, `3596`, and `3598` succeeded through the dev loop.
- Final launch: build `3598`, server/client builds matched,
  `source_current=true`, run `20261028_120549`.
- The final 20 s launch was a low-actor scene: recent p95 2.235 ms,
  p99 5.323 ms, max 17.928 ms in a 200 ms slice. This is not the busy-scene
  target and does not establish 240 FPS for 60 seconds.
- Final running build: `3572`, build id
  `3572:9903f69a9cc40d60d25f8da9c403d297fcd27d7863ad125ed2259f4254bd31e9`,
  `source_current=true`, server/client builds matched.
- Runtime journals inspected:
  - `logs/2026-10-28/20261028_040240/events.jsonl`
  - `logs/2026-10-28/20261028_041326/events.jsonl`
  - `logs/2026-10-28/20261028_041547/events.jsonl`
- `python devscripts/weapon_json_sync.py --check`:
  `masterlist=1 definitions=34 errors=0`.
- `python -m py_compile devscripts/perf-evidence.py` passed.
- `git diff --check` passed except CRLF conversion warnings.

Pre-existing/other-work state:

- `python devscripts/animation_json_check.py` reports 4 errors in
  `death.json`, `jump.json`, `mimita_sizing_6_dash.json`, and
  `mimita_sizing_6_downdash_001.json` (`invalid keyframe tick`). Those files
  were already modified by other work in the working tree; this session did not
  change them.
- The working tree contains other-agent/user changes in animation settings,
  colliders, weapon configs, docs, and gameplay code. They were preserved.
- `config/analytics.json` was changed by the running game; it is not part of
  this implementation.

## Plan B boundary

Connection/coordinator/ICE/reconnect/NPC-Waves reliability was not implemented
here. Those remain for the separate networking task.
