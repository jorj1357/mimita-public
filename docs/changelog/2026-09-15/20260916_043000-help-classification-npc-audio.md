# Help classification + NPC action audio (first NPC batch)

Date: 2026-09-16 04:30 EST (UTC 2026-09-16T08:30:00Z) [worktree, uncommitted]
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live appearance is proof debt)

Note: the pause Settings/ConfirmLeave + UI-audio work was delivered in the prior
round; this round adds the Help classification and the first NPC audio slice.

## PAUSE SETTINGS / CONFIRM LEAVE
Already hot (Round 61): pause Settings routes to hot settings with return;
ConfirmLeave composed hot. Unchanged here.

## PAUSE HELP / GLOBAL HELP
NOT migrated. `help-menu.cpp` is a 49-line generic JSON-layout renderer
(`config/gui/help-menu.json`) already content-hot-reloadable via GuiLayoutManager
with no feature policy. Classified low-value debt (D); migrating would duplicate a
generic renderer. Pause and global Help share it.

## GENERATION SAFETY
Unchanged (dynamic components + per-frame claims).

## UI COLD-OWNER RE-AUDIT
Cold: pause/global Help (generic renderer), replay browser, avatar creator,
login/auth composition, notification/consent/music overlays.

## REPLAY BROWSER CLASSIFICATION
A (mechanical; reuse listing/action pattern). Not migrated this round.

## AVATAR CLASSIFICATION
C (resource/editor dependent) — defer.

## LOGIN/AUTH CLASSIFICATION
B (secure mechanism) — cold; composition optional later.

## NOTIFICATION/CONSENT CLASSIFICATION
B/D — does not block UI completion.

## UI ARCHITECTURE COMPLETE ENOUGH?
YES: main menu, settings (all user-facing), pause core, server browser, TDM/FFA/CS
HUD, actor overlays, scoreboard, tool presentation, UI sound policy are hot.
Remaining cold UI is a generic Help renderer and secure/resource/isolated debt.

## AUDIO AUDIT
- UI sounds: hot (Round 61).
- NPC weapon fire: hot via `effect.weapon.fire.sound`.
- NPC action (dash): now hot via `effect.actor.sound`.
- Still cold: AudioManager owner/loop sounds (`npc_spawn`, music), ambient,
  interaction sounds. Mechanism gap: `audio.play` lacks owner/loop.

## NPC AUDIO STATUS
First slice hot (actor.dash). Owner/loop NPC sounds pending a mechanism extension.

## MUSIC/AMBIENT STATUS
Cold (needs owner/loop/streaming state; `audio.play` is fire-and-forget).

## NEW PRIMITIVES
None this round (`effect.actor.sound` reuses `EffectRequestV1` + `audio.play`).

## DID ANY WORK REQUIRE KILLING mimita.exe?
No.

## COLD-RESTART METRIC
Now NO: main menu, settings, actor overlays, TDM/FFA/CS HUD, scoreboard, pause
core, server browser, UI sound policy, NPC action audio (one-shot), tool
presentation. Still YES: replay browser, avatar, login composition, Help,
NPC/UI owner-based audio, music/ambient, resource generations.

## LIVE-PROOF DEBT
Interactive Help/replay/avatar, owner-based audio, music, resource swap.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E. One recorded primitive gap: `audio.play` owner/loop fields for
NPC-owner and music sounds.

## NEXT LARGEST COLD OWNER
Extend `audio.play` (owner/loop) OR reuse AudioManager owner path to finish
NPC/music audio; then replay browser (mechanical).

## Files changed
`src/npc/npc.cpp`, `src/hot-reload/modules/presentation/effect-composition.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
