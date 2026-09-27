# UI image resources + actor-like generic presentation

Date: 2026-09-15 01:15 EST (UTC 2026-09-15T05:15:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## 1. MatchHudState ownership audit

- The consumer chain (`ui.frame` -> `hot.match-hud` -> `render.ui`) is hot and
  generic.
- `MatchHudState` population is not yet written by the mode package (the
  gamemode/server agent owns TDM/FFA state). Classified B/C (cold projection /
  mode-specific cold logic) until that integration lands; the component is
  registered and readable.
- Cold `MatchTimer` composition already yields via `LiveUi::hotOwnsHud()`
  (compatibility fallback).

## 2/3. Mode-owned HUD state

Not implemented to avoid taking ownership of the mode package's match state
(other-agent area). `MatchHudState` remains generic (timer/scores/phase/labels);
a mode may add its own replicated dynamic components for extra UI.

## 4/5. TDM + runtime mode proof

Deferred to the mode agent; a runtime mode already can register a `ui.frame`
system and read/write generic components.

## 6. Objective HUD preparation

Design-ready: a mode `ui.frame` system can consume generic objective components
and relationships and emit widgets. No bomb-specific kernel UI branch added.

## 7. UiImage -> PresentationResourceProvider

`GAME_UI_IMAGE` now resolves a logical resource id via
`PresentationResourceProvider` (`LiveUi::resolveUiImageHandle`) and draws the
current generation handle with a new cold `uiDrawTexture(GLuint, UIRect, color)`
backend primitive. Path is a fallback. No UI-only hot-reload subsystem.

## 8. HUD live-edit proof

Not run (no visible client). `LIVE VISUAL PROVEN` = none.

## 9-16. Player/NPC presentation

- Audit: typed actor renderers (`NpcSystem::render` -> `render-player.cpp` ->
  `Player::renderCurrentPose`; local `renderPlayer`) still own local player and
  client NPC replicas. Policy (visibility/outline/wireframe) is cold; the low
  level GL is a legitimate cold mechanism.
- Delivered: a typeless **actor-like** generic entity (Transform + Health +
  PresentationState with team color as data) is presented by the hot
  `hot.presentation-mesh` system via `render.mesh`. No Player/NPC/Monster type;
  no new kernel renderer category.
- Not delivered: migrating the real typed NPC/player renderers. That requires the
  client NPC replicas to carry `PresentationState` (analogous to the projectile
  bridge) and is the next slice. No second permanent owner was created.

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `build_game_dll.py` -> `build/mimita-game.dll`.
- `--hot-combat-selftest` -> PASS incl. "ui image resolves a generation-aware
  resource handle" and "typeless actor-like entity presents via render.mesh
  (team color as data)". Full suite PASS.

## Classification

- SELFTEST PROVEN: ui image generation-aware resolution; actor-like typeless
  entity presentation with team color as data.
- COMPILED INTEGRATION: `uiDrawTexture` backend; `GAME_UI_IMAGE` provider
  resolution.
- LIVE VISUAL PROVEN: none.
- HUMAN VERIFICATION NEEDED: visible hot HUD and UI image, mode populating
  `MatchHudState`, real NPC/player generic presentation.

## Files changed

`src/gui/ui-system.h`, `src/gui/ui-system-render.cpp`,
`src/live-code/live-ui.{h,cpp}`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Next cold owner selected

Migrate one real actor path (NPC) to generic PresentationState via a client
actor-presentation bridge, then animation presentation.
