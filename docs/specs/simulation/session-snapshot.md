# Session snapshot

Status: desired behavior; not implemented. Session-first scope.
Scope: server-authoritative save and restore of a live gameplay state so a
human or agent can return to the same mode/round/state in one command.

## Purpose

Reproduce a live state quickly for debugging, tuning, and regression work. A
snapshot is not a replay and not a diagnostic pose dump.

## Non-goals

- Not a second gameplay simulation.
- Not a replacement for the canonical journal.
- Not a world-project or map-editor snapshot.
- Not a claim of full determinism for unsupported components.

## Existing foundation

- `src/devtools/scenario-server-recorder.cpp` captures
  `scenario.record.checkpoint` with actors, NPC AI, projectiles, RNG, and a
  completeness flag.
- `src/devtools/scenario-server-playback.cpp` restores actors, NPC AI, and RNG,
  and explicitly lists unrestored components.
- `scenario_play <path.jsonl>` restores a recorded start state.
- `agent.snapshot` is a diagnostic client snapshot, not a save-state.

Reuse the scenario checkpoint owner rather than creating a second serializer.

## SnapshotStateV1 (first slice)

Required:

- schema version, build/DLL identity, map, mode, round/phase, tick, RNG seeds;
- players and NPCs: identity, transform, velocity, orientation/up/spin,
  movement/action state, cooldowns, health, team/role/assignment, weapons/ammo;
- projectiles and area effects with fuses/lifetimes;
- objective/bomb state and timers;
- active interactions;
- per-life and respawn state needed to continue.

Declared limits for the first slice:

- destructible-world mutations;
- vehicles and other dynamic physical objects that the first serializer does
  not cover.

Every snapshot manifest lists `stored_components` and `unsupported_components`
with a reason. Do not call a snapshot “exact” when a required component is
missing.

## Storage

```text
saves/snapshots/<id>/
  manifest.json
  state.json
  checksums.json
```

- Versioned, checksummed, immutable after save.
- JSON first for inspection; binary later if size/load time requires it.
- A snapshot records the source build/generation and is rejected when the
  current map/mode/schema is incompatible.

## Commands

Server-authoritative:

```text
state_snapshot save <id>
state_snapshot load <id>
state_snapshot list
state_snapshot delete <id>
```

One-command launch:

```text
python devscripts/dev-loop.py --snapshot <id> --launch-mode 45
```

It starts the server, loads at a fixed-tick boundary, starts the client, joins,
increments state version, and broadcasts a full state. Clients reset
interpolation and reconcile to the authoritative state.

## Restore rules

- Load only at a fixed-tick boundary.
- Validate schema, map, mode, and build identity before applying.
- On failure, keep the current state and report the exact incompatibility.
- Do not teleport incrementally; do one full authoritative restore and one full
  snapshot.
- Preserve the prior journal and record the snapshot ID, source tick, restore
  tick, stored/unsupported components, and result.
- A snapshot is session state, not proof of behavior; a later runtime still
  needs its own evidence.

## Acceptance

- Save a mid-round Juggernaut Horde/FCS session, restart, load, and return to the
  same mode/round.
- Compare actor IDs, positions, velocities, health, weapons/ammo, objective/
  timers, and an N-tick event fingerprint.
- Divergence is zero for stored components or explicitly declared unsupported.
- Incompatible snapshot is rejected without changing the current run.
- One command reproduces the state.

## Risks

- Full authoritative state is broad; start with the declared session slice.
- Client presentation may need re-derivation rather than restoration.
- World destruction and vehicles can change gameplay; declare them unsupported
  until proven.
- Snapshot restore must not bypass shared movement, collision, damage, or network
  authority.
