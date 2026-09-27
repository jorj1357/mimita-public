# Weapon batch, equip lifecycle, world.project + actor overlays substrate

Date: 2026-09-15 20:00 EST (UTC 2026-09-16T00:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live proof deferred by policy)

## SUBSYSTEMS MIGRATED THIS PASS
- Weapon presentation batch (shotgun / rocket_launcher / grenade_launcher).
- Equip identity lifecycle reconciliation (spawn/respawn/join).
- New generic `world.project` capability + `ActorIdentityState` + hot
  `hot.actor-overlays`.

## TOOL IDENTITY LIFECYCLE
- spawn: `PresentationEntities::projectLocalPlayer` reconciles the local actor's
  generic `equips-item` identity from the typed mirror when they disagree.
- respawn: same reconciliation re-creates the edge after reset.
- remote: not yet (server-authoritative counterpart pending).
- join/reconnect: local reconstruction via the reconciliation; remote identity
  coverage pending.

## WEAPON PRESENTATION CATEGORY
- swordsword: hot (WORLD + VIEW).
- revolver: hot.
- shotgun: hot (new binding).
- rocket: hot (new binding).
- grenade: hot (new binding).
- remaining cold owner: cold `WeaponViewModel` is the compatibility/fallback
  draw only; all standard weapons now have a generic tool identity + hot mesh.
- new ABI required? None.

## OVERLAY CATEGORY
- player labels / NPC labels: hot policy implemented (`hot.actor-overlays` emits
  name from `ActorIdentityState`); cold `player-nameplates.cpp` is still the live
  owner.
- health bars: hot policy implemented (BAR + HP text via `render.ui`).
- world projection mechanism: generic `GAME_CAP_WORLD_PROJECT` (`world.project`).
- cold owner removed: not yet (off by default to avoid a duplicate owner until a
  per-actor gate + remote/NPC identity coverage exist).

## UI CATEGORY
- mode HUD: hot `hot.match-hud` exists but has no shipping writer; cold remains.
- menus/interactions: cold.
- remaining cold policy: recorded (see hot-cold-audit).

## AUDIO CATEGORY
- hot: explosion, weapon-fire, footstep, air-jump via `audio.play`.
- cold: NPC/UI/ambient/music policy (recorded for a later category pass).

## RESOURCE GENERATION CATEGORY
- meshes/textures/shaders: generation-aware provider (+ hot `resource.register`).
- audio: still name-keyed cache.
- animation clips/skeleton: static caches.
- fonts: none.
(recorded; not implemented this pass)

## COLD-RESTART METRIC
Ordinary behaviors that STILL require a mimita.exe rebuild: mode HUD composition,
menus/UI interaction, NPC/UI/ambient/music audio policy, remote-actor weapon
presentation, overlay live ownership, animation clip/skeleton resources, audio
resources (all recorded). Weapon presentation policy for all standard weapons:
**no**.

## LIVE-PROOF DEBT
- Real interactive equip + view/third-person sword/revolver/shotgun visual.
- Hot overlay live ownership (cold still owns).
- Resource hot-swap live.
- Hot C++ edit live generation switch.

## BLOCKING ARCHITECTURAL BUGS
None (A/B/C/D/E): no duplicate authority (overlays off by default), no crash, no
stale pointer (attachment resolves live handles), last-good intact.

## NEXT LARGEST COLD OWNER
Overlay live ownership (needs remote/NPC `ActorIdentityState` coverage + per-actor
cold gate), then mode HUD/UI composition, then audio policy.

## Files changed
`src/hot-reload/game-api.h`, `src/live-code/live-behavior.cpp`,
`src/hot-reload/modules/presentation/attachment.cpp`,
`src/hot-reload/modules/ui/actor-overlays.cpp` (new),
`src/network/actor-state.h`, `src/network/actor-state.cpp`,
`src/render/presentation-entities.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
