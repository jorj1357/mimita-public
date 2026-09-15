# Actor overlays hot-owned (per-actor), mode HUD recorded

Date: 2026-09-15 21:00 EST (UTC 2026-09-16T01:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live proof deferred by policy)

## SUBSYSTEMS MIGRATED
Actor overlays (nameplates + health bars) are now hot-owned in the real shipping
path, per actor. Mode HUD recorded as blocked on a generic client match-state
exposure.

## OVERLAY STATE COVERAGE
- local: `ActorIdentityState` + Health written by `projectLocalPlayer`.
- network players: `projectActorOverlayState(id, true, player)` in
  `engine-tick-render.cpp` (Transform + Health + Identity).
- NPC: network NPCs via `projectActorOverlayState(id, false, npc)`; local
  NpcSystem bodies remain cold (not on the generic actor path).
- health: generic `GameHealthComponentV1`.
- team: generic `ActorTeamState` (red default, blue team 1).

## OVERLAY REAL SHIPPING OWNERSHIP
`hot.actor-overlays` (ui.frame, order 15) enumerates `ActorIdentityState`, projects
the head via `world.project`, emits name + health bar + HP text via `render.ui`,
and writes `ActorOverlayClaim`. ON by default.

## COLD OVERLAY OWNER STATUS
`drawPlayerHealthbar` gained an `actorEntity` parameter and yields per-actor when
`ActorOverlayClaim.owned == 1`. Callers: self = `ensureLocalPlayerEntity()`;
network player/npc = `PresentationEntities::actorEntityFor(id, isPlayer)`;
local NpcSystem = 0 (cold fallback).

## RUNTIME-UNKNOWN ACTOR PROOF
A runtime entity with Transform + Health + `ActorIdentityState` gets the overlay
through the same system (selftest "generic actor gets a hot overlay via
render.ui").

## OVERLAY CATEGORY COMPLETE?
Mostly. Hot: labels, health bars, team colour, visibility/dead rule, runtime
generic actor, cold per-actor yield. Deferred (recorded, cosmetic): smoke
occlusion, distance fade tuning, local NpcSystem body coverage.

## MODE HUD
- FFA / TDM / Counter-Strike: cold (`CommunityMatchClient` + JSON layout).
- timer: cold `MatchTimer` (already yields on `hotOwnsHud()`).
- scores/round/results/scoreboard: cold.
- MatchHudState writer: none shipping (reader `hot.match-hud` exists).
- BLOCKED: the cold mode HUD is driven by typed `CommunityMatchClient` and does
  NOT yield on `hotOwnsHud()`. A hot writer needs a generic exposure of client
  match state (or a replicated MatchHudState written by the hot mode) plus a
  mode-HUD claim. A partial writer would duplicate/drop the HUD -> not wired.
- HUD category complete? No.

## UI INTERACTION
- current cold owners: `gui/menus/*`, `gui-main.cpp` switch, `ui-system` buttons.
- hot owner added: none this pass.
- missing generic primitive: generic UI element/action event + hot handler
  (design recorded).

## AUDIO CATEGORY STATUS
Hot: explosion/weapon-fire/footstep/air-jump. Cold: NPC/UI/ambient/music.

## RESOURCE GENERATION STATUS
Mesh/texture/shader generation-aware; audio/clips/skeleton/fonts still cold.

## NEW ABI ADDED?
No. Overlays use existing dynamic components, `world.project`, `render.ui`, and a
hot claim component (hot-only shared state, not a context field).

## COLD-RESTART METRIC
Still YES: mode HUD composition, UI interaction, NPC/UI/ambient/music audio,
remote-actor weapon presentation (server counterpart), animation/resource
generations. NEW NO: player/NPC nameplate policy, health overlay policy, team
overlay policy (hot, per-actor).

## LIVE-PROOF DEBT
Interactive overlay appearance; hot C++ edit; resource swap; remote weapon
presentation. Recorded.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E. No duplicate owner (per-actor claim), no crash, last-good intact.

## NEXT LARGEST COLD OWNER
Mode HUD (needs generic client match-state exposure), then UI interaction, then
audio policy.

## Files changed
`src/hot-reload/hot-presentation.h`,
`src/hot-reload/modules/ui/actor-overlays.cpp`,
`src/gui/hud/player-nameplates.h`, `src/gui/hud/player-nameplates.cpp`,
`src/engine/engine-tick-ui-game-hud.cpp`, `src/engine/engine-tick-render.cpp`,
`src/render/presentation-entities.h`, `src/render/presentation-entities.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
