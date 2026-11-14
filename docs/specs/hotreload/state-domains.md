# Resident state domains (2026-11-14)

Purpose: make Option A mechanical. All **behavior code** moves to `game.dll`.
Durable records stay in the resident host. The DLL decides; the host owns and
validates state.

This file is normative for resident state shape. It replaces hand-editing one
wide snapshot with generated domain records/ops.

## Why domains

The current `EntitySnapshotV1` is one large struct. A handler can read the whole
record, change one field, and write the whole thing back, clobbering another
handler's update. It also makes every new field a wide ABI change.

A domain record has:

- `structSize`, `version`, `handle`, `fieldMask`;
- only the fields that domain owns;
- generated read/write ops.

A writer updates only its domain. Unrelated fields cannot be lost.

## Domains

| Domain | Owns |
|---|---|
| `player_core` | id, kind, team, position, velocity, health, max health, flags, name |
| `player_session` | join/reconnect tokens, token validity, spawn generation, spawned, last heard, join validated |
| `player_chat` | chat rate tokens/last tick, last request id, cached retry event |
| `player_vip` | account id/role, VIP appearance, style detail, style epoch, progression ticket |
| `player_combat` | weapon state, shot/fire serials, cached fire results, godball state |
| `player_movement` | input command buffer, last input sequence, movement state subset |
| `npc_core` | id, name, position, velocity, health, team, yaw, on-ground, difficulty |
| `npc_combat` | AI state, aim, attack timers, per-life combat fields |

Add a domain only when a real owner needs it. Do not add one field as a domain.

## Source and codegen

Schema input: `config/hot-reload/state-schema.json`.

Generator: `devscripts/resident_state_codegen.py`.

Generated outputs:

- ABI structs and op signatures per domain;
- host adapter read/write/list/create code;
- game bridge wrappers;
- size/version checks and schema version;
- round-trip tests;
- hotness-ledger entries.

Rule: **do not hand-edit generated state files.** Move a field with a schema
edit plus the generator.

## Versioning and migration

- Each domain has its own version.
- A candidate validates every domain version it reads.
- A new field has a default or a migration from the previous version.
- Migration is validated and committed atomically with activation.
- A failed migration leaves the old generation and old state active.
- Unknown fields are ignored; missing fields get the documented default.
- The hot per-tick path uses stable handles and direct ops, never reflective
  field-name lookup.

## What stays resident

Only genuinely cold data: process/window/device handles, sockets/ICE/session
mechanism, file/worker handles, loader/barrier, and the generic records above.
Everything else is behavior in `game.dll`.

## Compatibility path

`EntitySnapshotV1` stays temporarily for unmigrated handlers. It is deprecated.
Delete it when the last handler uses a domain op.

## Ledger rule

Every domain row records: owner, version, fields, writers, readers, migration,
and the handler families that use it. The ledger is generated, not hand-written.

## Implemented foundation (2026-11-14)

- `config/hot-reload/state-schema.json` defines eight initial domains:
  `player_core`, `player_session`, `player_chat`, `player_vip`,
  `player_combat`, `player_movement`, `npc_core`, `npc_combat`.
- `devscripts/resident_state_codegen.py` generates the ABI header
  `src/host/generated/resident-state-v1.h` and the ledger
  `config/hot-reload/state-ledger.json`.
- Run `python devscripts/resident_state_codegen.py` after a schema edit and
  `python devscripts/resident_state_codegen.py --check` as the drift gate.
- Generated structs expose `structSize`, `version`, `handle`, `fieldMask`,
  field enum masks, defaults, `initialize`, and `clearFields`.
- Not yet wired: adapter ops, game bridge wrappers, round-trip tests, and the
  `EntitySnapshotV1` retirement. No domain is authoritative until an owner
  adapter uses it.
