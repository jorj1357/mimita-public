// 2026-11-19
/* purpose
* record the agent-driven melee validation harness and the melee/network fixes
* preserve build notes, live-journal evidence, and the unverified boundary
* this file DOES NOT claim melee parity, reconnect recovery, or 60 Hz acceptance
* this file DOES NOT replace the feature record
*/

# Agent-driven melee validation harness and first melee fixes

# Task

- Task ID: session-20261119-235406
- Session ID: session-20261119-235406
- Owner: opencode
- Focus: Mode 1 melee validation on dust2cyberiav4; predicted melee contact;
  snapshot/reconnect reliability; NPC tick stalls
- Summary: Added a real-input AI melee harness (`player_control ai melee <weapon>`),
  NPC-nav-graph route following, a ground/fall guard, a controlled
  `npc_spawn_near` fallback, a `tdm-melee-validation` profile, a scenario, and
  dev-loop AI melee flags/assertions. Also added targeted melee, NPC recovery,
  Recast-cache, and spawn broadphase fixes. Live runs show the harness works but
  a 4.5 s NPC respawn stall still forces the client into reconnect before melee
  contact can be proven.
- Status: NOT_TESTED (latest build pending; no human acceptance)
- Date, time, timezone: 2026-11-19T23:54:06+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Final commit: uncommitted shared-checkout edits

# Pre-existing changes

The shared checkout already contained broad unrelated edits in networking,
melee, NPC/nav, collision, dev-loop, creator, GUI, and documentation paths. This
session preserved them. Files touched by this session are listed below; all
other dirty paths are pre-existing work by other sessions.

# Requested behavior

Give agents one human-like, in-game way to prove Mode 1 melee: use TDM on
dust2cyberiav4, keep the player alive with `healthme 999999`, drive the player's
movement and aim through the normal input boundary, use the existing NPC
navigation owner, avoid falling through the map, and produce client claims,
server accepted/rejected decisions, and authoritative damage in the live
`events.jsonl`. Then use that harness to fix real melee mismatches.

# Specification alignment

- `docs/specs/weapons/melee/melee-weapons.md`: Mode 1 physical contact for
  Chainsaw, Mace, and SwordSword; client prediction; server historical validation.
- `docs/specs/networking/contact-authority.md`: immediate client prediction,
  server referee, one shared damage sink, no weapon-specific damage path.
- `docs/specs/dev-loop/dev-loop.md` and
  `docs/architecture/terminal-commands/terminal-commands.md`: real-input AI and
  bounded live validation.
- `docs/specs/performance/performance.md`: fixed 60 Hz simulation; no gameplay
  tick skipping.

# Exact implementation changes

## Harness and input path

- `src/actor/player-ai.h/.cpp`
  - Added `setMeleeWeapon`/`meleeWeapon` and `player_control ai melee <weapon>`.
  - Forces Mace/Chainsaw/SwordSword through the normal slot/equip path.
  - Uses `NpcNavGraph::findRoutes` with `NpcDifficultyConfig` graph settings;
    replans at a bounded interval and follows waypoints with normal input.
  - Uses `NpcNavigation::groundHeightAt` as a forward floor/drop guard.
  - Latches one close target, ignores targets beyond 25 m, and closes into the
    melee contact volume instead of a ranged stand-off.
  - Requests `npc_spawn_near 1.5 2` after a bounded no-close-target window.
  - Emits `player.ai.navplan`, `player.ai.state` (weapon, route, ground),
    `player.ai.fallback_spawn`, and `player.ai.fall_detected`.
- `src/terminal/player-commands.cpp`
  - `player_control ai|human|ai melee <weapon>`.
- `src/terminal/npc-commands.cpp`
  - `npc_spawn_near <distance_m> [count]`; sends the normal spawn-NPC request and
    lets the server validate/adopt the position.
- `devscripts/dev-loop.py`, `devscripts/dev_loop/cli.py`
  - `--ai-weapon`, `--ai-health`; melee assertions for claimed/accepted contact,
    physical-contact damage, and no fall event.
- `devscripts/dev-profiles/tdm-melee-validation.json`
  - TDM mode 41, map dust2cyberiav4, weapon set 29, 5 NPCs.
- `devscripts/scenarios/melee_tdm_ai_v1.json`
  - Repeatable `healthme 999999` + `player_control ai melee mace` run.

## Melee and NPC fixes

- `src/combat/melee-client.cpp`
  - Continuous Mode 1 claims no longer carry a stale action age when no action is
    active.
  - Added bounded `melee.client.scan` and `melee.client.shape_missing`
    diagnostics.
- `src/network/server-packet-handlers.cpp`
  - Melee claim target resolution now honors the claimed player/NPC namespace.
- `config/weaponcollisions.json`
  - SwordSword damage box scale `[0,0,0]` -> `[1,1,1]`; physical collision remains
    disabled. Mace/Chainsaw were left unchanged.
- `src/network/server-npcs.cpp`
  - Recovery deep checks now cool down for 0.5 s when the root is clear.
  - NPC collision world now rebuilds `solidBounds` so containment uses the
    cached tree instead of full-mesh scans.
  - Cached validated NPC respawn standing points per actor/geometry revision.
  - Bounded respawn spawn resolution to 24 candidates / 25 ms (pending build).
- `src/game/spawn-utils.cpp`
  - Vertical spawn rays and capsule fits use the cached collision broadphase.
- `src/npc/recast-navigation.cpp`
  - Reuses the published navmesh by collision content hash across map commits.
  - Defers an in-tick rebuild when policy disallows it.
  - Marks incremental tile rebuilds collision-changed only when content changed.
- `src/network/server-gamemode.cpp`
  - Skips synchronous Recast prewarm during gamemode map commits and records
    `npc.navmesh.prewarm-skipped`.

## Generated/config refresh

- `config/hot-reload/packet-ledger.json` regenerated with
  `python devscripts/packet_registry_codegen.py`; `--check` passes.

# Contract / ABI changes

None. The existing SpyKnifeContactBatch transport and shared server contact
resolver are reused. No new packet type or damage sink was added.

# State / schema changes

None beyond the generated packet ledger line numbers and the new dev profile and
scenario files.

# Deleted paths

None.

# Diagnostics

- `player.ai.navplan`: bounded route plan result and cost.
- `player.ai.state`: target, weapon, route, ground, position.
- `player.ai.fallback_spawn`: controlled near-NPC fallback.
- `player.ai.fall_detected`: below-map/death/respawn during AI mode.
- `melee.client.scan`: no-claim scan window (remote counts, damage boxes, shape).
- `melee.client.shape_missing`: client contact shape could not be built.
- `npc.navmesh.prewarm-skipped`: map-commit prewarm skip.
- Existing `melee.client.contact_claim`, `melee.contact.claim.accepted/rejected`,
  and `server.damage.applied` remain the correlation chain.

# Validation

- `python -m py_compile devscripts/dev-loop.py devscripts/dev_loop/cli.py`: passed.
- JSON profile/scenario parse: passed.
- `python devscripts/packet_registry_codegen.py --check`: passed after regeneration.
- `git diff --check` on focused files: passed (only Git LF/CRLF warnings).
- Build notes queued before C++ saves: `143e3af35bd7` (initial harness),
  `90f2d52986d2` (ground query), `1b2f5fb17328` (target latch/scan),
  `266cb7d03099` (local target), `54ca876b3c5a` (shape diagnostic),
  `c31a08eda38c` (latch takeover), `39ff41545c14` (bounded respawn resolution).
  `143e3af35bd7`/`90f2d52986d2` were attached to a failed preflight attempt; the
  final source generation was rebuilt under the later notes.
- No manual build, compiler, `--request build`, or `--wait-for-build` was used.
  The root dev-loop built generations 387, 388, and 390 during this session.

## Live runtime evidence

- Run `20261119_221735` (gen 387): TDM online. `player_control ai melee mace`
  enabled; `weapon.equip.accepted` resolved Mace (native slot 26); nav routes
  planned (1-6 ms); a controlled fallback spawned. No melee claim was produced
  before the client entered reconnect.
- Run `20261119_223125` (gen 388): AI latched one far target and moved to it;
  no claim before reconnect.
- Run `20261119_234623` (gen 390): AI enabled and requested fallback; server
  logged a 4.5 s NPC respawn stall at tick 7087
  (`npc.spawn.resolved`/`npc.respawn` for NPC 200006) and a
  `performance.server-tick-stall`; client snapshots stopped, server marked the
  player stale then released the slot, and reconnect requests failed with
  `token_not_found` after the 70 s hard-timeout + grace deadline. No melee claim
  occurred. This reproduces the reconnect failure and ties the first client
  stale event to the NPC respawn stall, but does not prove the stall caused
  every reconnect failure.

# Measured evidence

- AI nav plan cost: `plan_ms` 0.67-6.3 in the live runs.
- Mace equip accepted by the server in run 20261119_221735.
- Server tick windows in run 20261119_234623: p99 about 124 ms, max 4.56 s;
  most windows 50-60 Hz but with repeated 100-200 ms NPC stalls.
- No predicted melee contact was proven. The claimed/accepted equality target
  remains unmeasured.

# Regression review

No new regression entry appended. The reconnect failure and NPC tick overruns
are reproduced, but the reconnect path and the first-respawn cost are not yet
fixed and confirmed, so they remain investigation records rather than a closed
regression fix.

# Human acceptance

No human visual or feel acceptance was performed. Immediate melee feedback,
Mace/Chainsaw/SwordSword parity, continuous NPC presentation, reconnect
recovery, and 60 Hz acceptance remain unverified.

# Agent handoff context

- Next exact step: let the current root build publish the bounded-respawn fix,
  then rerun the isolated `tdm-melee-validation` run:
  `healthme 999999`, `player_control ai melee mace`, 60 s, inspect
  `melee.client.scan`/`contact_claim`, server accepted/rejected, and
  `server.damage.applied`.
- If no claim appears while `damage_boxes > 0` and a target is within 2 m,
  trace `clientMeleeShape`/`clientCollisionDamageBoxes` and the claim geometry.
- If the client still reconnects, treat the 4.5 s NPC first-respawn cost or the
  reconnect token/slot lifetime as the first divergence; do not change melee
  validation to compensate.
- Keep the current shared dirty tree; do not revert unrelated edits.

# Reverse

Reverse only the focused files listed above. Remove the new profile and scenario
if the harness is abandoned. Do not use repository-wide checkout commands against
the shared dirty tree.
