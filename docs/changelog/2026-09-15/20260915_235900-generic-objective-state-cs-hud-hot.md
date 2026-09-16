# Generic objective state + CS HUD hot-owned; loadout/spectate audit

Date: 2026-09-15 23:59 EST (UTC 2026-09-16T03:59:00Z) [worktree, uncommitted]
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live appearance is proof debt)

## SUBSYSTEMS MIGRATED
CS HUD objective composition moved hot via a generic objective-presentation
primitive + transitional projection. Loadout/spectate were audited and found to
have no shipping UI owner.

## LOADOUT AUDIT
- current cold owner: none. There is **no shipping loadout/weapon-selection
  menu** (`main-menu.json` has PLAY/SETTINGS/REPLAYS/AVATAR/HELP/EXIT; weapon
  selection is in-game via slot keys).
- current source of available tools: none in a menu; runtime tools via
  relationships/`ToolRefState`.
- current equip path: `WeaponSystem::equip` -> `actorStateEquipWeaponKey`
  (already hot-presented).
- NOT invented: creating a loadout screen would be a new feature, not an
  ownership migration.

## SPECTATE
- There is no spectator label/overlay UI owner; spectate is camera behavior
  (`engine-tick-camera`) + `localActorSpectating`. No Player* hot interface added.

## GENERIC OBJECTIVE STATE
`HotObjectiveStateV1` (`ObjectivePresentationState`): objectiveId, stateHash,
ownerTeam, progress, timer, flags. Reusable across objective kinds; hot
interprets `stateHash`.

## CS HUD OWNERSHIP
- transitional projection: `ModeHud::projectFromClient` writes objective from
  `CommunityMatchClient` (`objectiveBombState/PlantPercent/DefusePercent/
  bombSecondsRemaining`); `counterstrike` marked hot-covered -> `ModeHudClaim`.
- hot composition: `hot.match-hud` renders timer/scores (MatchHudState) + the
  objective line + progress bar (interpreted stateHash).
- cold CS HUD status: `gGamemodeManager.renderHud()` yields when
  `ModeHud::hotOwned()`.
- partial: alive counts and richer CS detail are approximated; recorded.

## SCOREBOARD / DISCRETE SETTINGS SELECT
Not migrated / still deferred (resolution/preset need a SELECT widget).

## NEW ABI / PRIMITIVES
`HotObjectiveStateV1` (`ObjectivePresentationState`). WHY GENERIC? One reusable
objective representation (bomb/capture/payload/flag/zone); no CS-specific
primitive. No kernel ABI added.

## DID ANY HOT WORK REQUIRE KILLING mimita.exe?
No (a lingering selftest exited on its own; cold build waited/retried).

## COLD-RESTART METRIC
Now NO: weapon presentation, actor overlays, TDM/FFA HUD, UI-interaction,
main-menu, settings (subset), **CS HUD objective**. Still YES: scoreboard,
discrete-settings select, audio policy, resource generations, and richer CS
detail.

## LIVE-PROOF DEBT
Interactive CS HUD appearance, scoreboard, audio, resource swap, avatar framing.
Recorded.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E.

## NEXT LARGEST COLD OWNER
Scoreboard (`MatchLeaderboard`) needs a generic per-actor score/kills/deaths
source (composition via repeated render.ui rows is fine; no table ABI); then
discrete settings SELECT; then audio policy.

## Files changed
`src/hot-reload/hot-ui.h`, `src/hot-reload/modules/ui/hud.cpp`,
`src/gui/hud/mode-hud-bridge.cpp`, `src/engine/engine-tick-ui-overlays.cpp`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
