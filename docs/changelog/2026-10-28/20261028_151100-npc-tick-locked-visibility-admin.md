# NPC tick-locked animation, visibility categories, live actor collision, and admin commands

Date: 2026-10-28
Author: AI implementation session
Status: Implemented; runtime smoke evidence recorded; human visual and two-client admin acceptance remain open.

## Request

Implement the previous plan for:

- an opt-in NPC tick-locked animation profile;
- live actor-collision capsule radius/height edits for existing actors;
- local `entity_visibility` master and category controls with through-wall
  collision shapes;
- server-authoritative session admin commands (`admincmds`, `player_list`,
  grants, revokes) and a gate over every existing host command;
- matching specification and gameplay-tracker updates.

## Implemented

### NPC tick-locked animation

- Added `AvatarAnimationSet::npcTickLocked`, parsed from
  `avatar.json.animation_profile.npcTickLocked`.
- Added `Player::isNpcActor`; set for local NPC bodies and remote NPC replicas.
- NPCs with the opt-in flag now advance one authored clip tick per fixed
  simulation tick and ignore `speedMultiplier`, velocity scaling, and walk
  frequency scaling. Player actors and unset NPC profiles are unchanged.
- Remote NPC replicas pass `GAMEPLAY_FIXED_DT` for the opt-in profile instead of
  render-frame `dt`, so a new server tick advances exactly one authored tick.
- Added bounded `anim.npc.ticklocked` evidence (actor, avatar, clip, clip tick,
  speed, playback scale; at most four records per second globally).
- Files: `src/entities/player.h`, `src/entities/player-animation.cpp`,
  `src/entities/player-avatar-animations.cpp`, `src/npc/npc-spawn.cpp`,
  `src/network/multiplayer-tick.cpp`, `src/network/multiplayer-interpolation.cpp`.

### Live actor collision

- Root cause confirmed: the active `config/actor-collision.json` default had
  `dead_capsule_length_m: 0.0`, which failed whole-file validation and silently
  retained built-in `0.7 / 3.6 / 2.6` values. Valid radius/height edits never
  loaded.
- Zero dead length now means “derive from standing capsule height”; negative or
  non-finite values still reject the file and retain the last valid revision.
- Reload and rejection now write `actor.collision.reload` /
  `actor.collision.reload_rejected` with path, revision, reason, and fixed tick.
- Periodic `actor.collision.profile` now includes resolved `active_profiles`
  with radius, height, and dead length.
- Exposed the solver’s exact capsule builder for visibility rendering.
- Files: `src/config/actor-collision-config.*`,
  `src/physics/movement/actor-pair-collision.*`, `src/sim/simulate-tick.cpp`,
  `src/network/server-npcs.cpp`, `config/actor-collision.json`.

### Local entity visibility

- Added `src/debug/entity-visibility.*` with an off-by-default master and four
  local category switches.
- `entity_visibility 1|0` is now presentation-only and no longer mutates authored
  `entity.visible` or enables unrelated debug visuals.
- Added `entity_visibility_actor_capsules`, `entity_visibility_weapon_hitboxes`,
  `entity_visibility_npc_positions`, and `entity_visibility_map_objects`.
- Actor capsules use the shared solver capsule; weapon hitboxes, NPC markers,
  map entities, and constraints render through the existing depth-disabled debug
  line path.
- Removed the duplicate `MapConfigRegistry` visibility master and methods.
- Files: `src/terminal/map-entity-commands.cpp`,
  `src/debug/debug-visuals-scene.cpp`, `src/physics/constraints/constraint-debug.cpp`,
  `src/gamemode/map-config.*`.

### Server admin commands

- Added per-session `adminCommandsEnabled`, `adminCommandsGranted`, and
  initialization state to `ServerPlayer`. The host starts enabled; others start
  disabled. State is not persisted.
- Added `admincmds 1|0`, `player_list`, `admincmds_grant N`, and
  `admincmds_revoke N`. Lists exclude stale players, put the host first, and are
  live-list references.
- All existing host commands now use the admin-access gate. `fly` uses the same
  gate. Disabling/revoking clears flight.
- Connected host clients route host-only terminal commands through the server
  packet path; direct application remains only for a headless/unconnected
  console.
- Server command results are also written to the terminal.
- Added bounded grant/revoke/toggle access records and command rejection fields.
- Files: `src/network/server.h`, `src/network/server-packets.cpp`,
  `src/network/server-packet-chat.cpp`, `src/network/multiplayer-tick.cpp`,
  `src/terminal/admin-commands.*`, `src/main-systems.cpp`,
  `src/terminal/debug-commands.cpp`, `src/terminal/actor-commands.cpp`,
  `src/terminal/npc-commands.cpp`, `src/devtools/terminal.cpp`,
  `src/network/packets.h`.

## Documentation

- `docs/specs/open-gameplay-issues.md` and
  `docs/specs/open-gameplay-issues-human.md`: extended the existing live capsule
  issue, updated NPC animation and Admin ESP intake, added this work item, and
  removed the duplicate collision table row rather than adding another issue.
- `docs/specs/animations/animations.md`: added task-guide/reference orientation,
  NPC tick-locked profile behavior, diagnostics, and runtime acceptance step.
- `docs/specs/npc-behavior/npc-behavior.md`: replaced the proposed actor
  collision wording with current ownership, live reload, zero dead-length
  semantics, active-profile evidence, and live-edit acceptance.
- `docs/architecture/collision/actor-vs-actor-collisions.md`: added current
  capsule implementation note and resolved the top TODO text.
- `docs/specs/entity-editor/entity-editor.md`: documented the master/category
  commands, off-by-default state, through-wall rendering, and presentation-only
  ownership.
- `docs/architecture/terminal-commands/terminal-commands.md`: documented admin
  access semantics, command table, gates, failure cases, and visibility commands.
- `docs/specs/networking/networking.md`: documented server-session admin
  authority, host-first live lists, grant lifetime, and direct-request rejection.

## Evidence

Build:

- Debug build 3644 published successfully by the dev loop.
- Final `--versioninfo` executable:
  `C:\mimita-v9\.dev\builds\3644\mimita.exe`.
- Final versioninfo journal: `logs/10-28-2026/20261028_111043/events.jsonl`.
- Final live dev-loop journal: `logs/2026-10-28/20261028_151025/events.jsonl`.

Runtime evidence from this implementation sequence:

- Build 3640, `logs/2026-10-28/20261028_150123/events.jsonl`:
  - `actor.collision.reload` revision 1 shows the previously rejected authored
    `default` `1.7 / 3.6 / 0.0` values are now active.
  - Live radius change produced revision 2 with radius `0.8` on both server and
    client at a fixed tick.
  - Negative radius produced `actor.collision.reload_rejected` with reason
    `profile 'default' capsule_radius_m must be > 0` and retained revision 2.
  - Restoring the file produced revision 3.
  - Host `admincmds` reported `admincmds=1 (host)`; `player_list` reported
    `1=NPC Dev(host)[admin]`.
  - Host `admincmds 0` disabled access; `player_list` and `healthall 250` were
    rejected with `rejected: no admin access`; `admincmds 1` re-enabled it.
  - Host grant to itself was rejected; revoke of a stale number was rejected.
- Build 3642, `logs/2026-10-28/20261028_150757/events.jsonl`:
  - Temporary jason profile with `npcTickLocked: true` and forced NPC avatar
    produced `anim.npc.ticklocked` records with integer authored clip ticks,
    `playback_scale: 1.0`, and movement speeds varying roughly 5–44 m/s.
    Consecutive stable falling samples advanced about 15 authored ticks per
    250 ms, consistent with 60 ticks/second.
  - Temporary avatar/config edits were restored immediately afterward.
- Build 3640 and 3644 live journals: all four visibility category commands were
  delivered and completed. Visual through-wall and independent-category
  appearance were not observed by a human in this session.

## Remaining acceptance

- Human visual review of each visibility category, master off-by-default,
  through-wall rendering, and another client’s unchanged view.
- Two-client admin acceptance: non-host self-enable rejection, grant, grant
  recipient off/on, revoke, disconnect, and renumbered stale references.
- Verify flight actually stops on access disable/revoke in a live movement run.
- No new regression entry was added because no previously working behavior was
  confirmed broken in this session.

## Validation and preserved work

- `git diff --check` was run; it reports only pre-existing trailing whitespace in
  unrelated informal documentation, not errors in the changed code.
- Pre-existing working-tree changes were preserved, including
  `config/accounts/default.json`, `config/analytics.json`,
  `config/audio/music-settings.json`, `config/collision.json`,
  `config/weaponcollisions.json`, `docs/specs/gamemodes/bodycam.md`, and the
  earlier untracked changelog/gold/regression records.

## Skills reviewed

- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/documentation-checker-v1.md`
- `docs/skills/terminal-command-checker-v1.md`
- `docs/skills/logging-checker-v1.md`

## Documentation TODOs encountered

- `docs/architecture/collision/actor-vs-actor-collisions.md:1` formerly said
  `todo explain how actors vs actor collisions work`; replaced with the current
  implementation note.
- `docs/architecture/collision/actor-vs-actor-collisions.md:3` formerly requested
  an NPC hybrid-mode section; the section exists and the note was resolved.
- `docs/architecture/player-npc-systems/npc-movement.md:22` still says
  `todo - explain`; suggested expansion: document current NPC inputs, movement
  executors, and locomotion-clip selection.
- `docs/specs/weapons/melee/swordsword.md:231` still requests animation/action
  details; suggested expansion: list action IDs, required clip IDs, and missing
  poses.
- `docs/specs/weapons/tool-weapon-ideas.md:1` still says `todo explain like`;
  suggested expansion: add a short purpose and index for the document.
