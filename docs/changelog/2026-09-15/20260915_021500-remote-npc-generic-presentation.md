# Real remote-NPC presentation bridge (cold build pending)

Date: 2026-09-15 02:15 EST (UTC 2026-09-15T06:15:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW` (source) / cold link + selftest PENDING

## 1. NPC presentation audit

- Replicated/server NPC identity: `ServerNpc.entityId`; client replica is
  `MultiplayerContext::remoteNpcs` (`Player`), keyed by entityId, updated by
  snapshot + `remoteNpcInterpolation` (A/B projection).
- Draw chain: `engineTickRender` -> `renderNetworkPlayer` ->
  `render/render-player.cpp` -> `Player::renderCurrentPose`
  (`entities/player-render.cpp`) -> skinned/`renderMesh`/capsule draw.
- Inputs: position/velocity/yaw/interpolation = typed client projection (B);
  model/avatar/outline/team color = presentation policy (C); GL draw/VAO/skinning
  = low-level mechanism (D); team id/lifecycle = generic replicated state (A).

## 2. Client actor presentation bridge

`PresentationEntities::ensureActor(actorId, pos, look, meshId, texId, scale,
color)` creates/updates a `ClientReplicated/Npc` entity with Transform +
PresentationState (team color data). `beginActorSync/endActorSync` retire
untouched actors. It is a projection, not a second authoritative identity.

## 3. One NPC = one presentation owner

`engine-tick-render.cpp` remote-NPC loop: when `actorMeshReady()`, the typed
`renderNetworkPlayer` + remote weapon draw is skipped and the hot
`hot.presentation-mesh` system draws the generic entity. When the mesh is not
loaded, typed draws (no invisible NPC, no double draw).

## 4. PresentationState holds representation data

Logical `meshResourceId` (`mesh.actor`), `textureResourceId`, scale, color.
Team color derived from `Player.matchTeam` at projection time (data). No
`NpcPresentationState`/`MonsterPresentationState`.

## 5. Team color/outline policy

Color is data on PresentationState; the hot presentation system owns the draw.
Renderer has no `if npc team == red` branch.

## 6. Transform is a projection

Authoritative/network state remains the existing client NPC
snapshot/interpolation; the bridge copies the interpolated pos/yaw into the
generic entity Transform before render.frame. No dual simulation.

## 7. Real NPC proof

Source-implemented; runtime proof PENDING the cold link (see evidence). Selftest
added: "real NPC replica projects to a generic presentation entity" and
"untouched actor presentation retires with the entity".

## 8. Unknown monster compatibility

Retained: typeless entity (Transform + Health + PresentationState) presents via
the same hot system; no MonsterType, no NPC renderer registration.

## 9. Resource swap

`mesh.actor` uses the existing generation-aware provider (same GLB loader); a
swap changes the handle without recreating the actor entity. No NPC-specific
asset cache for the migrated path.

## 10. Typed NPC ownership reclassification

`NpcSystem::render` / `render-player.cpp` / `Player::renderCurrentPose` =
COMPATIBILITY fallback + animation bridge; low-level GL = mechanism. No dead
code deleted; animation preserved.

## 11. Nameplate/health overlay

Deferred (documented); no NPC-specific overlay added.

## 12-13. Tests / live visual

Selftest checks added (not yet run). No visible client run:
`LIVE VISUAL PROVEN` = no.

## Evidence and blocker

- `python devscripts/live-build.py` -> generation 17 (`mimita-game` DLL).
- `-fsyntax-only` clean: `presentation-entities.cpp`, `engine-tick-render.cpp`,
  `engine-tick-ui.cpp`, `engine-tick-ui-hud.cpp`, `live-ui.cpp`,
  `live-behavior.cpp`, `hot-combat-selftest.cpp`.
- **Cold build blocked:** `mimita.exe` (pid 30680) was running, so
  `build_agent.py` refused with `HOT_RELOAD_BOUNDARY_VIOLATION`. The running
  executable was **not** killed. Cold link and `--hot-combat-selftest` are
  PENDING a no-process window.

## Classification

- SELFTEST PROVEN: none for this pass (selftests not executed; cold link pending).
- COMPILED INTEGRATION: source + syntax-check clean; live DLL generation 17.
- LIVE MULTIPLAYER PROVEN: no.
- LIVE VISUAL PROVEN: no.
- HUMAN VERIFICATION NEEDED: run the cold build, `--hot-combat-selftest`, and a
  visible NPC to confirm the bridge draws and the typed path yields.

## Files changed

`src/render/presentation-entities.{h,cpp}`, `src/render/presentation-render.cpp`,
`src/engine/engine-tick-render.cpp`, `src/hot-reload/hot-presentation.h`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Next cold owner selected

Run the deferred cold build + selftests, then animation presentation
(`AnimationState`/pose -> hot animation system -> cold skinning).
