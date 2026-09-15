# TDM/FFA mode HUD hot-owned via generic state

Date: 2026-09-15 22:00 EST (UTC 2026-09-16T02:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live proof deferred by policy)

## SUBSYSTEMS MIGRATED
TDM + FFA shipping mode HUD (timer/scores/phase/team labels) is now hot-owned
through generic `MatchHudState` + a `ModeHudClaim`, without hot code depending on
`CommunityMatchClient`. Also fixed a latent timer-starvation coupling.

## MATCH STATE AUDIT
All HUD fields originate in typed cold `CommunityMatchClient` (mode, phase,
phaseTimer, matchStartTick, serverTick, timeLimitSeconds, redScore, blueScore,
goal, goVisible). `serverMatchEntity()` is the generic match entity; hot mode
state components (`FfaMatchState` etc.) exist on it but the client HUD reads the
cold client mirror. No generic client match state existed before this pass.

## GENERIC MATCH HUD STATE
- fields: `HotMatchHudStateV1` (timerSeconds, scoreA/B, phase, phaseText,
  labelA/B) — reused, not extended.
- authoritative source (transitional): `CommunityMatchClient` via the cold
  `ModeHud::projectFromClient()` projection onto the generic match entity.
- temporary compatibility projections: all of the above. Forward path: the
  authoritative hot mode writes/replicates generic state; the projection then
  retires.

## MODE HUD OWNERSHIP CLAIM
`HOT_MODE_HUD_CLAIM_COMPONENT` / `HotModeHudClaimV1`, registered by the hot
package. Written by the projection (owned=1 for tdm/ffa, 0 otherwise) and read by
both `hot.match-hud` (compose only if owned) and the cold gate. Simple whole-HUD
claim; no section claim needed for this slice.

## TDM
- shipping writer: `ModeHud::projectFromClient()` (red/blue score, goal not yet
  shown, timer, phase, team labels).
- hot composition: `hot.match-hud` (timer, score panel, phase text, progress bar).
- cold yield: cold FFA/TDM HUD block + `MatchTimer` gated on `ModeHud::hotOwned()`.

## FFA
- Same substrate; projected via the same client state (scores/labels/timer/phase).

## COUNTER-STRIKE
- shipping writer: none (claim owned=0 -> cold owns).
- objective facts: none generic yet.
- hot composition / cold yield: not migrated (needs generic objective state).

## ROUND / RESULTS / INTERMISSION
Cold for CS/duel; for tdm/ffa the hot HUD shows phase text (COUNTDOWN/GO/
INTERMISSION) from the projection.

## SCOREBOARD
- row source: cold `MatchLeaderboard` (typed). ordering/hot ownership: none.
- cold status: remains the owner.

## HUD CATEGORY COMPLETE?
No. TDM/FFA timer+scores+phase done; CS, scoreboard, full round/results, and the
objective HUD remain cold.

## UI INTERACTION
- current cold owners: `gui/menus/*`, `gui-main.cpp` switch, `ui-system` buttons.
- generic event primitive: none yet (design recorded: element/action hash +
  type + value -> hot handler).
- hot actions migrated: none. cold policy remaining: all menus/interactions.

## NEW ABI ADDED? WHY GENERIC?
No kernel ABI. `HOT_MODE_HUD_CLAIM_COMPONENT` is hot-only shared state;
`MatchHudState` reused. `ModeHud::hotOwned()` is a cold gate helper.

## COLD-RESTART METRIC
Now NO: weapon presentation, actor overlays, TDM HUD, FFA HUD (timer/scores/
phase). Still YES: CS HUD, scoreboard policy, UI interaction, NPC/UI/ambient/
music audio, audio/animation/skeleton resources, remote weapon authority.

## LIVE-PROOF DEBT
Interactive TDM/FFA HUD appearance; overlay appearance; hot C++ edit; resource
swap; remote weapon presentation. Recorded.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E. No duplicate owner (claim gates cold); no loss for unmigrated
modes (claim owned=0).

## NEXT LARGEST COLD OWNER
CS HUD (needs a generic objective-presentation state: bomb/plant/defuse), then
scoreboard, then UI interaction, then audio policy.

## Files changed
`src/hot-reload/hot-ui.h`, `src/hot-reload/modules/ui/hud.cpp`,
`src/gui/hud/mode-hud-bridge.h` (new), `src/gui/hud/mode-hud-bridge.cpp` (new),
`src/engine/engine-tick-ui-hud.cpp`, `src/engine/engine-tick-ui-overlays.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
