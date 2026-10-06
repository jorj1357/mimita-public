# Ragdoll JSONL Process Ownership Diagnostics

Time: 2026-10-06T18:18:00Z

## Outcome

Clarified canonical JSONL ownership for live ragdoll investigations. A shared
journal can begin with a server record and still contain client records; the
diagnostics now make that explicit and identify the exact executable for each
logger startup.

## Source changes

- `src/debug/structured-log.cpp`
  - `logger.started` now includes `process_role` and the absolute executable
    path.
- `src/game/game-cli.cpp`
  - `--versioninfo` now prints `PROCESS_ROLE=client|server` and includes the
    role in `versioninfo.executed`.
- `src/devtools/dev-log-commands.cpp`
  - `log_open` now reports records by process, startup identities, death-event
    count, ragdoll-event count, and an explicit warning when neither event
    family is present.
- `docs/regressions/2026-10-06/ragdoll-jsonl-process-ownership-REG.md`
  - Records the recurring client/server journal ambiguity and prevention rules.

## Build evidence

- Executable: `mimita-20261006T-log-ownership-v2.exe`
- Build status: SUCCESS
- Compiled: 1 source file
- Tests: none added or run, per the live-runtime investigation scope.

## Runtime evidence

`mimita-20261006T-log-ownership-v2.exe --versioninfo` printed:

```text
PROCESS_ROLE=client
EVENTS_JSONL_PATH=logs/10-06-2026/20261006_141704/events.jsonl
```

The corresponding `logger.started` record contained the executable path,
`process=client`, and `process_role=client`.

The supplied journal
`logs/10-06-2026/20261006_141256/events.jsonl` contained both a server and a
client `logger.started` record, but no `death.*` or `ragdoll.*` records. A
fresh live Counter-Strike death using the newly named executable is still
required to prove the complete death-to-ragdoll event chain.

## Follow-up render/lifetime diagnostics

- `src/ragdoll/ragdoll-mode.cpp`
  - logs `ragdoll.corpse.render_submitted` the first time each live corpse
    reaches `renderNetworkPlayer`;
  - logs `ragdoll.corpse.position_sample` every 60 corpse simulation updates,
    including age, position, velocity, fade, and active count;
  - logs `ragdoll.corpse.evicted` when the active corpse cap removes the oldest
    corpse.
- `src/ragdoll/ragdoll-mode.h`
  - stores per-corpse lifetime sample ticks and render-log state.

The source has a hard active-corpse limit of 12. The new eviction event makes
that limit visible in the live journal instead of silently dropping the oldest
corpse.

Follow-up build:

- Executable: `mimita-20261006T-ragdoll-render-trace-v1.exe`
- Build status: SUCCESS
- Compiled: 13 source files
- Tests: none added or run; live runtime evidence remains required.

## Second-order render diagnostics

- `src/render/render-player.cpp`
  - adds bounded ragdoll-stage events for renderer absence, dead-state gates,
    and entry into the inner player renderer;
  - records shader ID, model readiness, physical body/mesh counts, fallback
    mesh counts, and corpse position.
- `src/entities/player-render.cpp`
  - records which actual render path was selected: physical body meshes,
    fallback render mesh, or capsule;
  - records drawable part, batch, and vertex counts so a successful outer call
    cannot be mistaken for an actual draw.
- `src/ragdoll/ragdoll-mode.cpp`
  - includes username and match team in corpse diagnostics so teammate and
    enemy deaths can be correlated.

Follow-up build:

- Executable: `mimita-20261006T-ragdoll-render-trace-v2.exe`
- Build status: SUCCESS
- Runtime identity probe: `PROCESS_ROLE=client`,
  `EVENTS_JSONL_PATH=logs/10-06-2026/20261006_143024/events.jsonl`.
- Tests: none added or run; this remains a live executable investigation.

## Authoritative damage/death boundary diagnostics

Added four bounded canonical `events.jsonl` events at the owning code paths:

- `src/network/server-damage.cpp`
  - `server.damage.applied` records attacker, target, team, requested/applied
    damage, health before/after, position, spawn generation, and knockback.
  - `server.death.transition` records the authoritative alive-to-dead change,
    target identity/team, health, position, source, and respawn state.
- `src/network/multiplayer-projectiles.cpp`
  - `client.death.received` records accepted lethal damage packets, event IDs,
    target spawn generations, replica presence in both player and NPC maps,
    damage, health, and hit position.
- `src/network/multiplayer-interpolation.cpp`
  - `client.death.applied` records the remote snapshot `>0 -> <=0` transition,
    the exact health/position/server tick, and whether the corpse spawn gate
    was eligible.
- `src/network/server-attack.cpp`
  - adds the same server damage/death events to the Counter-Strike NPC
    hitscan path, which directly mutates `ServerNpc.health` and therefore does
    not pass through the player damage function.
- `src/combat/weapon-fire-damage.cpp`
  - records predicted-death entry, already-dead/already-predicted skips,
    presentation skips, and the resulting prediction state.
- `src/network/multiplayer-shots.cpp`
  - records reliable NPC death receipt, replica presence, flag values before
    handling, and explicit presentation-skip reasons.
- `src/network/multiplayer-interpolation.cpp`
  - records the death-presentation reset before and after values, including the
    actor ID, health, and snapshot tick.

Follow-up build:

- Executable: `C:\mimita-v9\.dev\builds\1553\mimita.exe`
- Build status: SUCCESS; 1 additional source file compiled and linked after
  the first four-event build.
- Runtime identity probe: `PROCESS_ROLE=client`,
  `EVENTS_JSONL_PATH=logs/10-06-2026/20261006_144254/events.jsonl`.
- Tests: none added or run; this remains a live executable investigation.

Evidence-only follow-up build:

- Executable: `C:\mimita-v9\.dev\builds\1554\mimita.exe`
- Build status: SUCCESS; 1 source file compiled and linked after the
  diagnostic additions.
- Runtime identity probe: `PROCESS_ROLE=client`,
  `EVENTS_JSONL_PATH=logs/10-06-2026/20261006_145212/events.jsonl`.
- No ragdoll behavior fix was applied in this pass.
- Tests: none added or run; the next step is live gameplay evidence.

## Ragdoll Attempt 1 behavior fix

Inspection of `logs/10-06-2026/20261006_145207/events.jsonl` found three NPC
death presentations skipped because `networkDeathPresented` was already true
from an earlier life, with no intervening NPC reset event. The same journal
had 14 corpse attempts, 14 successful spawns, zero rejections, and zero
evictions, so this occurrence moved the primary failure boundary to per-life
state and the missing local-player presentation path.

Implemented:

- `src/network/multiplayer-interpolation.cpp` clears
  `networkDeathPresented` during the fallback dead-to-alive health reset and
  logs `client.death.presentation.health_reset` plus its completion event.
- `src/network/multiplayer-projectiles.cpp` now presents one corpse for a
  lethal authoritative local-player damage confirmation and logs local apply
  and duplicate-skip outcomes.
- A detailed regression/spec record was added at
  `docs/regressions/2026-10-06/ragdoll-per-death-presentation-ATTEMPT-1-REG.md`.

Validation:

- Executable: `C:\mimita-v9\.dev\builds\1561\mimita.exe`
- Build status: SUCCESS.
- Runtime identity probe: `PROCESS_ROLE=client`,
  `EVENTS_JSONL_PATH=logs/10-06-2026/20261006_150036/events.jsonl`.
- Tests: none added or run; live gameplay acceptance remains pending.
