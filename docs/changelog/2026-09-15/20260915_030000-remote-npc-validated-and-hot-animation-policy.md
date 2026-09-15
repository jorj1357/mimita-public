# Remote-NPC presentation validated + generic hot animation policy

Date: 2026-09-15 03:00 EST (UTC 2026-09-15T07:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## 0. Deferred validation of Round 21 (NPC presentation)

A no-process window opened; the previously blocked cold build was run:

- `python build_agent.py` -> `Status: SUCCESS`.
- `build_game_dll.py` -> `build/mimita-game.dll`.
- `--hot-combat-selftest` -> PASS, incl. "real NPC replica projects to a generic
  presentation entity", "live actor presentation persists across a frame",
  "untouched actor presentation retires with the entity".
- Full suite (9 selftests) -> PASS.
Remote-NPC generic presentation is now SELFTEST PROVEN. (A selftest ordering bug
was fixed: the retire check now runs a second empty sync instead of expecting the
touched frame to retire.)

## 1. Animation ownership audit

- movement/combat state -> animation selection: `Player::updateProceduralAnimation`
  / `entities/player-animation.cpp` — A animation policy (typed, COLD).
- playback/time/transitions: typed Player — A (COLD).
- pose generation + skeleton apply: `Player::renderCurrentPose`
  (`entities/player-render.cpp`) — B pose/skeleton.
- skinning/bone upload/draw: same functions — D cold mechanism.
- `animation.update` capability (`capAnimationUpdate` in `live-behavior.cpp`) is
  the existing temporary hot->typed bridge.

## 2. Generic animation state

`hot-reload/hot-animation.h`: `HotAnimationStateV1 { clipId, playbackTime,
playbackRate, loop, flags }`, `HOT_ANIMATION_STATE_COMPONENT` (GAME_NET_ALL),
logical clip ids `anim.idle/move/attack/dead/jump`. No Player/Npc/Monster
animation type.

## 3. Hot animation policy

`modules/presentation/animation-policy.cpp` (`hot.animation-policy`,
`render.frame`, priority 1) selects the clip from generic actor state (Velocity,
Health) and advances playback: dead -> `anim.dead`; speed > threshold ->
`anim.move`; else `anim.idle`. Clip change restarts playback. Editing the file
and saving changes the running client's animation behavior.

## 4. Cold mechanism kept cold

Skeleton decode, bone matrices, skinning, mesh buffers, draw remain in the EXE.

## 5. Generic skeleton/pose capability

Reused the existing capability path; the `animation.update` bridge is left in
place (still used by the typed skinning owner). Removing it needs hot pose
generation, which is the next slice.

## 6/7. First real proof + minimal state set

Clip selection (idle/move/dead) is proven headlessly; attack/jump transitions are
deferred. The selftest actor is a typeless `ClientReplicated/Npc` entity (also
covers the runtime monster case).

## 8. Runtime monster

Same `AnimationState` path; no MonsterType and no NPC renderer registration.

## 9. Live edit

Not run (no visible client). LIVE VISUAL PROVEN = no.

## 10/11. Resource + typed owner

Animation clips/skeletons are not yet logical provider resources (documented as
the next resource bridge). `Player::updateProceduralAnimation` /
`renderCurrentPose` remain compatibility + cold mechanism; not deleted.

## 12. Tests

Added: "hot animation policy selects move for a moving actor", "... selects idle
for a still actor", "... selects death for a dead actor". Full suite PASS. No
Player/Npc/Monster-specific animation type; no new feature-specific game-api
field.

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `--hot-combat-selftest` -> PASS (NPC + animation checks).
- Full suite PASS.

## Classification

- SELFTEST PROVEN: remote-NPC generic presentation (projection/persist/retire);
  hot animation clip selection (move/idle/dead).
- COMPILED INTEGRATION: `hot.animation-policy` system; `AnimationState` schema.
- LIVE MULTIPLAYER PROVEN: no.
- LIVE VISUAL PROVEN: no.
- HUMAN VERIFICATION NEEDED: visible NPC mesh + animation, live animation-policy
  edit, attack/jump clips, typed pose generation becoming hot.

## Files changed

`src/hot-reload/hot-animation.h` (new),
`src/hot-reload/modules/presentation/animation-policy.cpp` (new),
`src/hot-reload/hot-modules.json`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Next cold owner selected

Hot pose generation via `skeleton.apply` to retire the `animation.update` bridge,
then animation clip/skeleton logical resources.
