# NPC fire intent hot command

Date: 2026-09-24 16:29:42 EST
Branch: current working branch

## Result

PASS_WITH_HUMAN_REVIEW. NPC fire intent now originates in the replaceable actor
behavior module through the generic `ActorCommandV1::buttons` field. The hot
module emits `ACTOR_BUTTON_FIRE` for a live NPC with a target. The EXE consumes
that generic command and leaves weapon legality, cooldown, reload, LOS, aim,
ammo, projectile/hitscan execution, and authoritative damage in existing
owners.

## Exact changes

- `src/hot-reload/game-api.h`: documented `ActorCommandV1::buttons` and added
  the append-safe generic `ACTOR_BUTTON_FIRE` value.
- `src/hot-reload/modules/actor-behavior.cpp`: emits the fire button for NPC
  actor state with a target and positive health.
- `src/npc/npc.h`: added per-tick `hotFireIntent` state.
- `src/npc/npc.cpp`: copies the hot command into `hotFireIntent`; the legacy
  target-and-ammo fallback is used only when hot actor behavior is unavailable.

## Hot-reload evidence

`python devscripts/live-build.py` produced generation 2 successfully:
`build/hotreload/p19596/gen2/mimita-live-g000002.dll`, status `ok`, code hash
`bb40d6c8ad7cbc5f863ea60879094552a4344a0d12e200de9ea1a0b01c04a027`.
The running executable was not relinked or replaced.

The actor behavior edit is therefore in the hot generation. The small EXE-side
consumer is a required stable bridge: an already-running EXE must understand
the generic button before a newly loaded DLL can affect NPC firing. Installing
that bridge into an older running EXE requires the repository's intentional
cold-build/install window; this session did not restart or replace the running
game.

`git diff --check` passed. No debug logging was added. Human acceptance still
requires observing an NPC shoot and player health decrease in-game.

Pre-existing edits were preserved and are not attributed to this change.
