# Counter-Strike NPC teams, Rage2 behavior, enemy targeting, and RMB aim zoom

Date: 2026-10-04
EST timestamp: 2026-10-04 18:18:01 EST
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, pure-test, and in-binary runtime evidence are proven. No live
Counter-Strike match (one CT human + CT NPCs vs T NPCs) was played; that human
acceptance remains required.

## Scope

Implement the handoff "Counter-Strike NPC teams, Rage2 behavior, enemy
targeting, and aim zoom" without touching spectator-camera behavior:

1. Counter-Strike NPCs resolve `rage2` via a gamemode profile (precedence
   explicit > gamemode > role > default).
2. Generic mode-level team targeting (`npc_targeting`), no dependency on the
   global `npc-difficulty.json` for Counter-Strike.
3. Team identity preserved through death/respawn via one source of truth, with
   no silent CT fallback for an unknown team.
4. Counter-Strike-only RMB aim-FOV override resolved from the gamemode, with
   `config/aimbody.json` left as the global fallback.

## Pre-existing / external edits (NOT mine)

The working tree already contained large uncommitted edits from earlier
sessions (`config/npc-difficulty.json`, `config/weapons.json`,
`config/behavior-profiles.json`, `src/npc/npc-*.cpp`, `devscripts/dev-loop.py`,
etc.). `--actor-preset-selftest` still FAILs on the pre-existing revolver/shotgun
value assertions (working tree 35/6/36 and 20/6/32), independent of this change.
A background dev-loop recompiled and relinked `mimita.exe` during the session.

Note: `config/roles.json` already set `behavior_profile: "rage2"` for both CS
roles (the handoff said "balanced"); the gamemode profile now also owns it and
wins by precedence, as requested.

## Files changed

### Config `config/gamemodes/counterstrike.json`
- `npc_behavior_profile: "rage2"`.
- `npc_targeting` block (`mode: opposite_team`, `include_players: true`,
  `include_npcs: true`).
- `camera.aim_fov` block (`enabled`, `input: right_mouse`, `multiplier: 0.5`,
  `duration: 0.5`, `easing: ease_in_out`).

### New pure owner `src/npc/npc-targeting.h` / `.cpp`
- `NpcTargetingMode` (Player/Closest/OppositeTeam), `NpcTargetingPolicy`.
- `npcTargetingIsHostile` (strict opposite-team; legacy FFA for Player mode),
  `npcTargetingIncludesPlayers/Npcs`.
- `NpcTargetCandidate` + `selectNpcTargetId` (nearest hostile; Player mode
  prefers a hostile human).
- `npcTargetingSelfTest`.

### New pure owner `src/entities/aim-fov.h` / `.cpp`
- `AimFovSettings`, `aimFovEase`, `updateAimFovBlend`, `applyAimFov`,
  `aimFovSelfTest`.

### `src/gamemode/gamemode.h` / `.cpp`
- `GamemodeNpcTargeting` and `GamemodeAimFov` structs on `Gamemode`.
- Parse `npc_targeting` (validated mode) and `camera.aim_fov` (clamped values).

### `src/network/server-gamemode.cpp` / `.h`
- `serverResolveActorSpawnProfile`: profile precedence is now
  explicit > gamemode > role (role no longer overwrites the mode profile).
- `resolveNpcBehaviorProfileId(gm, roleProfile)` used by roster descriptors.
- `[NPC BEHAVIOR]` diagnostic now includes `mode=%s`.
- Team source of truth: `actorTeamOf`, `isValidPlayingTeam`,
  `spawnGroupForTeam`, `gamemodeSpawnPointForActor` (logs `[RESPAWN]`, and in a
  team mode rejects an unknown team to the neutral anchor instead of a team
  cluster).
- `sharedAnchor` field; `gamemodeSpawnPoint(d)` now returns the neutral anchor
  (never the CT cluster).
- `assignGamemodeSpawns` logs `[CS SPAWN]` error when a team mode has no team
  spawn tags.
- Kill/map-change/rotate/round-reset/warmup/respawn-all paths use
  `gamemodeSpawnPointForActor` (victim/killer team never cross).
- `validateTeamRoleConsistency` (pure) called at roster build; rejects
  `terrorist/team0`, `counter_terrorist/team1`, `team=-1`, spectator, and
  spawn-group mismatch.

### `src/network/server-npcs.cpp`
- Builds an effective `NpcTargetingPolicy` (mode block, else legacy
  `npc-difficulty.json` fallback), replaces `actorsAreHostile`, gates player
  loops with `includePlayers`, and gates NPC loops with `includeNpcs`.

### `src/engine/engine-tick-camera.cpp`
- Mode-level RMB FOV override: uses `gm.aimFov` (input = raw RMB, independent
  of the physical aim-body mode) when enabled, else the `AimBodyConfig`
  fallback. Blend state resets on mode change, leaving the match, or the local
  actor leaving Alive. Spectator camera block untouched.

### `src/game/game-cli.cpp`
- `--npc-targeting-selftest`, `--aim-fov-selftest`.
- `serverCounterStrikeRoundSelfTest` extended: profile `rage2`, precedence,
  `npc_targeting=opposite_team`, `camera.aim_fov`, valid teams, and
  `validateTeamRoleConsistency` accept/reject cases.
- `serverSpawnTagSelfTest` extended: team spawn-group resolution and valid
  playing teams (loads the gamemode registry itself).

### New tests
- `tests/npc-targeting-test.cpp` (23 checks).
- `tests/aim-fov-test.cpp` (13 checks).

## Reasoning

One pure targeting owner and one pure FOV owner keep the logic testable and
avoid hard-coding "CT vs T": hostility is a single predicate that a future
team-relation table can replace. The respawn change routes every team-aware
path through one resolver so the victim's own team is the only source, and an
unknown team can no longer silently inherit the CT cluster.

## Spec review (`docs/skills/spec-behavior-review-v1.md`)

Result: no blocker findings.

- The handoff stated roles.json used `balanced`; the working tree already uses
  `rage2`. Resolved by adding the gamemode profile and making it authoritative.
- The global `npc-difficulty.json` (`targetMode: "player"`,
  `damageOtherNpcs: false`) is intentionally unchanged; Counter-Strike uses its
  own block and other modes keep the legacy fallback.
- No unresolved `NEEDS_SPEC_DECISION` items.

## Documents and skills

- Handoff: "Counter-Strike NPC teams, Rage2 behavior, enemy targeting, and aim
  zoom" (this session).
- Skill: `docs/skills/spec-behavior-review-v1.md`.
- `docs/features/gamemodes/counterstrike.md` (Attempt 15 appended).
- `docs/operations/task-completion/task-completion.md`,
  `docs/operations/build-and-exe/build-and-exe.md`.

## Validation

### Build

```text
[CXX ] src\network\server-gamemode.cpp
[LINK] mimita.exe
BUILD SUCCESS
Compiled: 1
```

(A background dev-loop also recompiled the other changed objects.)

### Pure tests

```text
build/npc-targeting-test.exe -> PASS (23 checks)
build/aim-fov-test.exe       -> PASS (13 checks)
```

### In-binary selftests

```text
--npc-targeting-selftest          PASS
--aim-fov-selftest                PASS
--cs-round-selftest               PASS (npc_behavior_profile=rage2, npc_targeting=opposite_team)
--spawn-tag-selftest              PASS
--gamemode-selftest               PASS
--npc-movement-policy-selftest    PASS
--npc-navigation-selftest         PASS
--npc-search-behavior-selftest    PASS
--npc-radar-selftest              PASS
--npc-nav-request-selftest        PASS
--counterstrike-acceptance-selftest PASS (all groups)
```

- `--actor-preset-selftest`: FAIL only on the pre-existing weapon-value
  assertions (not caused by this change).

### Runtime / human

Not performed. The live match test below is still required.

## Human review still needed

1. Start Counter-Strike with one CT human + CT NPCs vs T NPCs.
2. CT NPCs ignore the CT human and pursue Terrorists; T NPCs pursue CT actors
   (player and NPC).
3. Killed Terrorists return to T spawn; killed CT actors return to CT spawn; no
   NPC respawns on the enemy spawn.
4. NPC behavior logs show `[NPC BEHAVIOR] mode=counterstrike ... profile=rage2`.
5. RMB zoom works only in Counter-Strike and does not require editing
   `config/aimbody.json`; Sandbox keeps its existing RMB behavior.

## Known limitations

- Target selection still uses the existing behavior-profile scoring/nearest
  path; `selectNpcTargetId` is the pure policy used for tests and the hostility
  predicate, not a replacement for the scored path.
- `explicit` per-actor profile override is a forward hook (no current producer).
- Team relation is a single generic predicate; a RED/BLUE/GREEN table is a
  future extension.
