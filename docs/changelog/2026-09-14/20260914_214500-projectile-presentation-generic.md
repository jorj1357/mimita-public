# Real projectile presentation onto generic PresentationState + render.mesh

Date: 2026-09-14 21:45 EST (UTC 2026-09-15T01:45:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## Scope

Client-side counterpart of the already-generic server projectile simulation. No
server entity lifecycle, transport, prediction/interpolation redesign, or
gamemode work (other-agent areas).

## 1. Projectile presentation audit (classification)

- A (real): `combat/weapon-system.cpp:639` player rocket (typed), `:647` grenade
  (typed), `npc/npc-combat.cpp:565` NPC rocket, `network/multiplayer-projectiles.cpp:2175`
  network projectile, `engine-tick-render.cpp:415` replay rocket.
- B (compatibility/prediction): the player typed rocket path also renders
  client-predicted rockets (`weapon-system.cpp:854-869`).
- D (dead): `effects/effect-part-render.cpp:383` `replay_rocket` branch (no
  producer); `projectile-render.cpp:343` `clearProjectileMeshes` (no caller).

## 2. PresentationState is canonical (for the migrated path)

Added shared hot header `src/hot-reload/hot-presentation.h`:
`HotPresentationStateV1` (logical mesh/texture ids, flags, scale, color) with
`HOT_PRESENTATION_COMPONENT = gameHash("PresentationState")` and logical ids
`mesh.rocket`/`mesh.grenade`/`texture.rocket`/`texture.grenade`. No
RocketPresentation/GrenadePresentation/NpcProjectilePresentation.

## 3-4. Rocket then grenade migrated

`tools/rocket-tool.cpp` and `tools/grenade-tool.cpp` now write `Transform` +
`PresentationState` on the projectile entity they create. The canonical
`hot.presentation-mesh` render system draws any entity carrying
`PresentationState` through `render.mesh`; orientation follows `Velocity`
(falling back to `Transform.look`). One path for both weapons.

## 5. Prediction compatibility

Not rewritten. The client predicted/legacy network projectiles still use the
typed renderer because their client entities do not yet carry `PresentationState`
(needs the other agent's client entity lifecycle + EntityId mapping). Typed
rendering is now classified compatibility-only for those paths.

## 6. projectile-render.cpp ownership

Reduced to a compatibility adapter: it still serves `weapon-system.cpp`,
`npc-combat.cpp`, `multiplayer-projectiles.cpp`, and replay. It cannot be
deleted yet. Two members are dead (`clearProjectileMeshes`, `replay_rocket`
branch).

## 7. Generic resource ids

Projectile `PresentationState` carries logical ids only. `PresentationRender`
resolves current generation handles from `PresentationResourceProvider`.

## 8-9. Live edit / resource swap proof

Not run visually. Provider semantics are SELFTEST PROVEN (same hash no-op,
changed hash swap, failed load preserves last-good). Real rocket/grenade meshes
and textures are COMPILED INTEGRATION with per-frame content-hash polling, so a
live texture replacement swaps the generation without recreating entities.
LIVE VISUAL PROVEN = pending; HUMAN VERIFICATION NEEDED.

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `build_game_dll.py` -> `build/mimita-game.dll` (sources=25).
- `--hot-combat-selftest` -> PASS, incl. "rocket projectile carries generic
  PresentationState", "grenade projectile carries generic PresentationState",
  render.mesh, and provider checks.
- `--dynamic-replication-`, `--dynamic-lifecycle-`, `--live-code-`,
  `--capability-`, `--gamemode-hot-`, `--movement-parity-`,
  `--hot-authoritative-`, `--entity-slice-selftest` -> PASS.

## Classification

- SELFTEST PROVEN: projectile entities carry `PresentationState`; hot
  render.frame presents them via `render.mesh`; provider generation/no-op/swap/
  last-good.
- COMPILED INTEGRATION: rocket/grenade procedural meshes + textures through the
  provider; per-frame poll.
- LIVE VISUAL PROVEN: none.
- HUMAN VERIFICATION NEEDED: visible rocket/grenade via the generic path, live
  presentation edit, live texture swap, no duplicate typed+generic draw on a
  client that has the replicated entity.

## Files changed

`src/hot-reload/hot-presentation.h` (new),
`src/hot-reload/hot-modules.json`,
`src/hot-reload/modules/tools/rocket-tool.cpp`,
`src/hot-reload/modules/tools/grenade-tool.cpp`,
`src/hot-reload/modules/presentation/debug-presentation.cpp`,
`src/render/presentation-render.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Next cold owner selected

GLB mesh loading through the same `PresentationResourceProvider` (no separate
subsystem), then wiring client predicted/network projectile entities to
`PresentationState` (depends on the other agent's client lifecycle), then the
hot HUD widget tree.
