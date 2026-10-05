// 10 05 2026, 01 13 UTC
/* purpose
* specify the generic JSON-defined gamemode runtime: mode packs, capabilities,
* the fixed-tick action graph, deterministic disasters, and the migration path
* for existing modes
* describe desired behavior; code implements it
* this file DOES NOT replace docs/specs/gamemodes/gamemodes.md or bombtag.md
* this file DOES NOT permit mode-name branches in shared runtime code
*/

# Mode Packs and Disaster Runtime (Milestone 1)

## Goal

Replace mode-name dispatch with a registry-driven runtime:

```text
mode manifest
→ declared capabilities
→ fixed-tick action graph
→ shared engine primitives
```

"Generic" means shared server logic contains no `if mode == "ffa"` /
`else if mode == "tdm"` style branch that identifies an individual mode.
Conditionals may still exist inside reusable capability implementations; they
must never identify a mode name. Capability and win-policy ids
(`inventory.random_per_actor`, `win.last_actor_alive`) are reusable vocabulary,
not mode names.

## Mode packs

A community mode pack is a validated JSON manifest discovered under
`config/mode-packs/`. The registry:

- scans only configured local roots;
- rejects malformed JSON, unsupported schema versions, duplicate ids, unknown
  capabilities, and missing required fields;
- replaces the catalog atomically;
- retains the last valid catalog on failure;
- reports the exact pack, field, and capability that failed.

A pack's stable `id` comes from the manifest, never from its filename or
display name. Catalog order is deterministic (sorted by id) and independent of
filesystem iteration order.

Schema (`schema_version: 1`):

| Field | Meaning |
|---|---|
| `schema_version` | must equal the supported version |
| `id` | stable mode-pack id |
| `name`, `description` | display metadata |
| `gamemode_id` | bridge to `config/gamemodes/<id>.json` lifecycle values |
| `capabilities[]` | stable capability ids (see below) |
| `disasters[]` | disaster definitions (see below) |
| `hud_layout`, `banner_audio` | presentation references |
| `maps`, `weapons`, `actors`, `assets` | declared references |

Disaster definition:

| Field | Meaning |
|---|---|
| `id`, `name`, `description` | stable id + display |
| `duration_seconds` | bounded duration; must be > 0 |
| `weapon_pool[]` | logical weapon ids for per-actor assignment; non-empty |
| `win_policy` | win policy id; defaults to `last_actor_alive` |

## Capabilities

Stable capability ids (allowlist owned by `CapabilityRegistry`):

```text
lifecycle.intermission_countdown
participants.all_actors
inventory.random_per_actor
win.last_actor_alive
hazard.spawn
damage.area
objective.finish_volume
presentation.disaster_banner
```

A manifest may only declare ids the compiled server can execute. Unknown ids
fail the load.

## Fixed-tick action graph

The server resolves a manifest into an ordered `ActionGraph` before the match
starts. Actions are scheduled by phase entry, match start, interval, elapsed
time, completion, or failure. The active match executes the matching compiled
capability handlers without checking the mode name.

## Determinism

- The server owns one authoritative `matchSeed`, replicated with the disaster.
- Disaster selection, per-actor weapon assignment, and timeout-winner selection
  are pure functions of `(seed, stable ids)`.
- Selection is order-independent: reordering the manifest, the weapon pool, or
  the actor list cannot change the outcome for a fixed seed.
- The server and every client agree on disaster identity, seed, start tick, and
  duration through `DisasterStatePacket`.
- Selection must never depend on `std::random_device`, `std::rand`, wall-clock
  time, or filesystem iteration order.

## Replication

`DisasterStatePacket` carries mode/disaster id, display name + description,
seed, start tick, duration ticks, active flag, and the resolved winner. It rides
the existing state cadence (sent alongside `DuelStatePacket`). The client only
displays replicated disaster state; it never decides outcomes. Stale packets are
rejected by `(duelId, stateVersion)`.

## First disaster: `random_weapon_last_alive`

- Reuses participant registration, FFA-style combat, weapon definitions/sets,
  damage/death events, one-life respawn suppression, results/intermission,
  leaderboard, and killfeed.
- Adds only: deterministic per-actor weapon assignment from the replicated
  round seed, teamless last-survivor evaluation, a bounded duration, and a
  deterministic timeout resolution.
- No disaster-specific C++ class and no mode-name branch.

## Presentation

The generic HUD token system supplies `{mode_name}`, `{disaster_name}`,
`{disaster_description}`, `{seconds}`, `{round}`, and `{objective}`. A mode's
layout is selected by its replicated mode id from `config/gui/gamemode-meta-gui.json`.
No `renderTornadoHud()` / `renderDuckHud()` style per-disaster renderer exists.

## Compatibility and migration

During migration, `config/onlinemodes.json` and `config/gamemodes/*.json`
remain compatibility inputs, and existing mode-name branches stay until their
mode is migrated. Milestone 1 requires only the new mode to be branch-free.

Migration order (one family of branches removed per validated step):

1. FFA
2. TDM
3. NPC Waves
4. Elimination
5. Counterstrike
6. Bomb Tag (expose carrier assignment, transfer-on-contact, timer expiration,
   elimination/respawn policy as capabilities first)
7. Duel / legacy paths

## Deferred (not milestone 1)

- Embedded scripts (sandboxing, budgets, deterministic APIs, versioning).
- Remote distribution/signing/caching/moderation; packs ship as local folders.
- Persistent destructible-map damage; destruction resets at the match boundary.

## Open decisions carried from milestone 1

- The hot-audio recipe system described by
  `docs/architecture/live-development/hot-audio-contract.md` is not present on
  the `afad20a-rebuild` branch. Milestone 1 uses existing `playEventSound`
  primitives; adopting recipe ids is a later milestone.
- `GamemodeRegistry::loadDirectory` clears before loading (not atomic). New
  registries must use the atomic pattern; the existing registry may be migrated
  later.
