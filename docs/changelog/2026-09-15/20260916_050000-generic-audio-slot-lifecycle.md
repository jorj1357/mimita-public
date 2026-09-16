# Generic audio slot lifecycle mechanism

Date: 2026-09-16 05:00 EST (UTC 2026-09-16T09:00:00Z) [worktree, uncommitted]
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live audio is proof debt)

## AUDIO MANAGER AUDIT
`AudioManager` keeps `gActiveSounds` with `ownerId`; `play(AudioEvent)` starts a
voice (fixed position), `stopOwner(ownerId)` unloads. No loop flag, no
idempotence/replace by logical slot, no per-slot model. Mechanism = device/mixer/
decode/voice; policy = which sound. Preserved mechanism, extended minimally.

## GENERIC AUDIO LIFECYCLE PRIMITIVE
- one-shot: unchanged `PLAY_ONESHOT`.
- owner: optional `ownerEntity` (0 = global).
- slot: opaque `slotId` hash (no enum); identity is (ownerEntity, slotId) desired
  state, not the physical voice.
- set/stop/update: SET_SLOT (idempotent; changed sound replaces), STOP_SLOT.
- loop: `loop` flag -> `ma_sound_set_looping`.

## AUDIO IDENTITY
- owner identity: EntityId (0 = global).
- slot identity: gameHash logical id.
- physical voice ownership: cold `AudioManager` synth owner id; never exposed.

## ENTITY DESTRUCTION SAFETY
SET/STOP purge slots whose owner entity is no longer alive and stop their voices.

## GENERATION SAFETY
Desired slot state is cold-side; a generation swap does not invalidate it. A new
generation may re-assert the same state (idempotent) or change/stop it.

## NPC AUDIO
- one-shot: hot (actor.dash, Round 62).
- owner/loop: mechanism now available; policy migration pending.
- policy ownership: hot (mechanism is generic).

## INTERACTION AUDIO
Not migrated.

## TOOL/WEAPON AUDIO
Already hot via `effect.weapon.fire.sound` (no cold weapon branch for fire sound).

## MUSIC
- desired track/global slot: mechanism available (`owner=0`, `slot=music.primary`);
  hot policy migration pending.
- transitions/cold streaming: not changed.

## AMBIENT
Same slot model available; not migrated. Multiple active loops supported (per
owner/slot).

## RUNTIME-UNKNOWN SLOT/SOUND PROOF
`audio SET_SLOT` with an unknown slot id + unknown logical sound is accepted with
no cold enum/switch (selftest).

## COLD AUDIO POLICY LEAKS REMAINING
`AudioManager` call sites still choose sounds directly: `npc_spawn`,
interaction sounds, music manager selection. Classified REAL POLICY LEAK
(candidate next migrations); `audio.play` one-shot paths already hot.

## AUDIO MECHANISM COMPLETE ENOUGH?
Mostly: one-shot, optional owner, arbitrary slot, loop SET, idempotence, A->B
replace, STOP, entity-death cleanup, no handles across boundary, unknown slot/
sound. Remaining: owner position-follow for loops (recorded limitation).

## AUDIO POLICY COMPLETE ENOUGH?
No: NPC owner/loop, music, ambient, interactions still cold.

## NEW ABI / PRIMITIVES
`GameAudioOp` + `GameAudioCommandV1` owner/slot/op/loop; `AudioEvent.loop`.
WHY GENERIC? Persistent audio keyed by (ownerEntity, opaque slot) with mechanism
ops; reusable for any loop (NPC voice, engine, ambient, music); no feature enum.

## DID ANY WORK REQUIRE KILLING mimita.exe?
No.

## COLD-RESTART MATRIX
Not yet wired for policy: change music track/ambient/NPC loop policy (mechanism
ready). Already hot: UI sound policy, NPC one-shot, weapon fire.

## LIVE-PROOF DEBT
Actual audible loop behavior, music, ambient, fades.

## NEXT LARGEST REAL COLD OWNER
Wire hot music/ambient via global slots and NPC owner/loop policy; then
interaction audio; then resource-lane convergence (per mission).

## Files changed
`src/hot-reload/game-api.h`, `src/audio/audio.h`, `src/audio/audio.cpp`,
`src/live-code/live-behavior.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
