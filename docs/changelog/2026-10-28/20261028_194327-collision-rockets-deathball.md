# Collision, NPC rockets, Deathball, and rocket visuals

- Recorded: 2026-10-28T19:43:27Z (2026-10-28 15:43:27 EDT)
- Branch: `20261011zt`
- Starting HEAD: `8f455b01`
- Result: `PASS_WITH_HUMAN_REVIEW`
- Build: `3668` (`source_current=true`, server/client builds matched)
- Runtime journal inspected: `C:\mimita-v9\logs\2026-10-28\20261028_193809\events.jsonl`

## Existing work preserved

The checkout already contained a large set of user/other-agent edits across
config, gameplay source, scenario tooling, and docs. They were preserved. This
session's direct edits are the collision, rocket, Deathball, ragdoll, weapon
config-order, scenario-build, and documentation changes listed below. Unrelated
pre-existing changes are not claimed here.

`docs/specs/open-gameplay-issues-human.md` was already deleted before this
session and was not recreated.

## NPC rockets in TDM

Root cause: `broadcastNpcFiring()` created NPC `ServerProjectile`s without
`weaponDefNetworkId`. TDM deathballs therefore lacked their definition identity,
were simulated as solid rockets, slept at walls, and could be recovered forever
by the client as fake rockets.

Changed:

- `src/network/server-npcs.cpp`
  - set `projectile.weaponDefNetworkId = MimitaNet::weaponDefNetworkIdFor(wdef->id);`
  - set `spawn.weaponDefNetworkId = projectile.weaponDefNetworkId;`
  - added `npc.projectile-created` with weapon identity, lifetime, and
    pass-through/explode flags.
- `src/network/server-projectiles.cpp`
  - added `sendTerminalDespawn(...)` and `server.projectile.terminal`; non-exploding
    lifetime removals now send a reliable despawn instead of disappearing.
- `src/network/multiplayer-projectiles.cpp`
  - stale state-recovery packets past lifetime are discarded;
  - recovered projectiles force a nonzero fallback lifetime.
- `src/npc/npc-combat.cpp`, `src/npc/npc-combat.h`,
  `src/engine/engine-tick-combat.cpp`
  - deleted the unused local `gNpcRocketState` path, local NPC rocket fire call,
    `NpcCombat::updateNpcProjectiles`, and its tick call. NPC projectiles are
    authoritative server projectiles only.

Runtime evidence from TDM (`20261028_193809`):

- NPC rocket spawns recorded `weapon_def_network_id=8`.
- NPC deathball spawn recorded `weapon_def_network_id=27`,
  `pass_through_world=true`, `pass_through_actors=true`, lifetime 10.
- `client.projectile.state-recover = 0`.
- `server.projectile.terminal = 4` at the later check (2 when first inspected).
- `deathball.wall-pass` recorded the 3x lifetime decay.

## Deathball damage

The server Deathball damage branch already existed but required the projectile
to resolve its real `deathball` definition. The NPC identity propagation above
restores that path instead of falling through to `rocket_launcher`.

A live player/NPC Deathball hit was not executed in this session; edge/center
damage and knockback still require a runtime hit scenario. The pass-through,
identity, and wall-decay evidence above is runtime-proven; damage application is
source-corrected and awaiting that hit test.

## Rocket Launcher visuals

Changed:

- `config/weapons/rocket_launcher/config.json`
  - added `render.projectile` using
    `assets/objects/weapons/weapon-objects/mimita-rocket-model-v1.glb`,
    facing axis, scale, rotation, offset, and
    `assets/textureshq/rocketsmokev1.png`;
  - added `render.explosion` using
    `assets/textureshq/explosionmarkv1.png` and
    `assets/objects/things/effects-objects/shockwave-v1.glb`.
- `src/combat/weapon-types.h`
  - added projectile-model/smoke/explosion visual fields.
- `src/combat/weapon-json-config.cpp`
  - parses `render.projectile` and `render.explosion`.
- `src/combat/projectile-render.{h,cpp}`
  - loads and draws the supplied GLB, aligns the configured facing axis, and
    uses one shared config for local, network, and replay rockets.
- `src/combat/weapon-system.cpp`
  - removed the duplicate projectile visual-config builder and added the live
    `rocket_preview` render.
- `src/terminal/weapon-commands.cpp`
  - added `rocket_preview [0|1]`.
- `src/combat/weapon-rocket-launcher.cpp`
  - smoke particles use the configured texture; local explosions pass the world
    surface normal into `spawnExplosionFx`.
- `src/network/multiplayer-projectiles.cpp`
  - network rocket trails use the configured smoke texture.
- `src/combat/explosion-fx.cpp`
  - spawns the configured explosion light and surface-aligned shockwave;
  - explosion smoke uses the smoke texture;
  - keeps the existing red explosion sphere block unchanged.
- `config/lighting.json`
  - added `rocket_launcher.explosion` dynamic light.
- `config/impact_decals.json`, `src/config/impact-decals-config.{h,cpp}`,
  `src/effects/effect-part.{h,cpp}`, `src/effects/effect-part-render.cpp`
  - added the `explosionMarks` group and surface-aligned mark decal.
- `src/effects/effect-part.h`
  - added `modelPath` and `SurfaceDecalKind::ExplosionMark`.

The existing red sphere (`config/weapon_hitfx.json`, radius 5, 3 ticks, red)
was not changed.

## Collision

Changed:

- `src/physics/movement/physics-collision-mesh.cpp`
  - walkable faces sharing an edge now use a relaxed seam threshold (dot 0.50)
    and a 0.05 m contact tolerance; non-walkable corners keep the configured
    exact-coplanar threshold.
- `src/physics/movement/physics-collision-shared.h`
  - part-driven root push is damped to 25% for non-weapon body parts; weapon
    contacts keep the full configured rebound. Frozen collision values
    (`maxSpeed: 50`, `strength: 0.35`) were not edited.
- `src/network/server-npcs.cpp`
  - `buildNpcWorldCollision()` no longer re-decimates already-unioned headless
    triangles;
  - NPC spawn/respawn uses `resolveSafeStandingSpawn`;
  - a root inside static solid geometry is recovered with
    `npc.capsule.recovered`.
- `docs/architecture/collision/collision.md`
  - documented NPC capsule-now / triangle-later scope.

Runtime TDM evidence: four `npc.capsule.recovered` events (one at first inspection) and one player
`collision.last_valid.rollback`. The `tdmtest3` ramp spawn/seam, block-edge,
sphere, and persistent-penetration scenarios still need live player/NPC
traversal review.

## Ragdoll limb limits

`config/ragdoll.json` now bounds head and arm attachment cones and rotation
ranges instead of using `999`/±180. Leg limits are unchanged. Elbow/knee joints
were not added because the v1 spec lists them as non-goals.

## Weapon config order

`devscripts/weapon_json_sync.py`:

- `--check` now reports out-of-order top-level fields, not only missing/unknown
  fields and types;
- added `--reorder`, which preserves JSONC comments and existing values while
  putting fields in exact masterlist order.

All 34 per-weapon configs were reordered. `--check` reports
`masterlist=1 definitions=34 errors=0`. The Mace JSONC comments remain.

## Client/server-only directive

Added to `docs/specs/networking/networking.md` under "End goal" and cross-linked
from `docs/architecture/code-ownership/code-ownership.md`: solo play is still a
client connected to a local authoritative server process; delete local/offline
gameplay paths once the client/server owner exists.

## Scenario build fixes

The checkout's scenario tooling did not compile at the starting HEAD:

- removed the missing `network/multiplayer-packets.h` include from
  `scenario-recorder.cpp` and `scenario-player.cpp`;
- fixed `MP_CONTEXT` availability via `terminal/terminal-state.h`;
- replaced unsupported `nlohmann::json` vector initializers with explicit
  arrays;
- qualified `MimitaNet::serverGamemodeState()`;
- removed stale `Npc` melee-action assignments in
  `scenario-server-playback.cpp`.

These were required only to produce a build; no scenario behavior was claimed.

## Validation

- Dev-loop build `3668`: success, `source_current=true`, server/client matched.
- Runtime journal `20261028_193809`: NPC rocket/deathball identity, wall-pass,
  terminal despawn, zero state recovery, NPC capsule recovery.
- `python devscripts/weapon_json_sync.py --check`: PASS.
- `python -m py_compile devscripts/weapon_json_sync.py`: PASS.
- JSONC parse of modified lighting/impact-decal/ragdoll/rocket/deathball
  configs: PASS.
- `git diff --check`: PASS except CRLF conversion warnings.

## Human review still required

- tdmtest3 ramp spawn/seam, block edge, sphere, and NPC crowd traversal feel.
- Deathball edge/center damage, knockback, repeated tick damage, and duplicate
  local/server damage behavior.
- Rocket model orientation/scale, smoke, light, mark, shockwave, and red sphere.
- Ragdoll limb limits in death/alive/grab poses.
- Weapon config comment/format readability after reorder.

## Deferred

Melee-focused mode changes, projectile revolver/shotgun experiments, heavier
movement mode, and weapon-clash feedback were intentionally not implemented in
this pass.
