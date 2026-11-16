# Direct Cutover-1 plan — 2026-11-16

## Purpose

This handoff records the agreed plan for the remaining direct work between the
current hot client and Hot Reload Cutover-1.

It is a coordination document. The current authority remains
`docs/specs/hotreload/hotreload.md`.

## Decisions

- Keep `devscripts/dev-loop.py` as the existing dev-loop entry path.
- `hotness.py` is read-only and strict.
- `build/hotness.json` is a disposable generated census, not committed state.
- `.dev/state.json` is the canonical live dev-loop state.
- Delete `.dev/current-run.json` as a duplicate view; it currently has the same
  SHA-256 as `.dev/state.json`.
- Devscripts folder consolidation and `docs/specs/devscripts/*` are deferred
  until after the Cutover-1 direct blockers.
- Packet route data must be generated from code, not from a hand-maintained
  `packet-schema.json` default/override table.
- Use NPC collision and authored-animation visibility as the first real
  same-PID gameplay proof. The local-player animation scope still needs one
  clarification from the user.

## The three user-facing outputs

| Output | Owner | Purpose |
|---|---|---|
| `build/hotness.json` | `hotness.py` | Disposable static/evidence census and gate results |
| `.dev/state.json` | `dev-loop.py` | Live build/run/control state |
| `logs/<run>/.../events.jsonl` | game and dev-loop | Runtime behavior evidence |

All three must be AI-readable. Static rows must expose `declared`, `observed`,
`owner`, `status`, `evidence`, and `reason`. Unknown must never be reported as
resident or pass.

## Current direct blockers

1. **EXE cutover/link gate**
   - `build/hot-reload/build-target-gate.json` is not clean.
   - `build/hot-reload/exe-cutover.json` reports 572 game-owned sources in the
     615-source EXE. The current gate infers this from ownership rules; it must
     read the actual linker input instead.

2. **Packet route truth**
   - `packet-ledger.json` reports `client_dll=12/78`.
   - That number is the count of explicit `packet-schema.json` overrides, not the
     count of actual hot client handlers.
   - `src/network/multiplayer-tick.cpp` is game-owned and already handles
     client inbound packets such as WELCOME and SNAPSHOT.
   - `hot_reload_status.py` hardcodes `client_packets_complete=false` and
     `client_packets_hot=false`.

3. **State migration proof**
   - Eight resident state domains exist and are wired.
   - Add/remove-field migration, validation, atomic commit, and failed-migration
     rollback are not proven.

4. **Audit classification**
   - `audit.json` has 797 rows and 554 high-risk classifications.
   - This is not 554 bugs. It includes resident-adapter-required,
     runtime-proof-required, state-review, and non-game rows.
   - Cutover needs `unresolved_game_blockers == 0`, not every row closed.

5. **Fallback and legacy removal**
   - `MIMITA_GAME_DISABLE` remains.
   - Resident server fallback blocks remain.
   - `render-command-legacy.cpp`, `game-gl-legacy.cpp`, and the other legacy
     bridge files remain.

6. **CAP_RENDER**
   - 42 game sources still contain raw GL/GLAD.
   - Each must move behind CAP_RENDER or become a documented exception.

7. **Same-PID real fix**
   - Repeated swaps pass.
   - A real gameplay fix that is visible, journal-proven, and rolled back is not
     yet proven.

## Direct work order

### 1. First visible fix: NPC collision and animation

Narrow scope:

- identify one NPC path where an NPC enters or remains inside a block;
- use `npc.collision.stuck`, `npc.root.pushout`, and animation evidence;
- fix the collision owner, not a synthetic test;
- bind the authored NPC animation set and make the visible clip/transition
  obvious;
- verify local and remote players see the same intended animation;
- record before/after `events.jsonl` and human visual acceptance;
- keep the change in game-owned source.

The local-player animation scope must be confirmed: “for me” means the local
player's avatar animation is missing, or only NPC animations are missing for the
local viewer.

### 2. Actual EXE link census

- `build.py` will write `build/link-inputs-<mode>.json` at link time.
- It will contain every object and its source path actually passed to the linker.
- `build_target_gate.py` and `exe_cutover_gate.py` will read that file first.
- Source ownership labels come from `build/source-manifest.json`.
- A missing/stale link-input file fails closed.
- The exact game-owned source list becomes the cutover worklist.

This replaces the ownership-rule estimate with actual linker input.

### 3. Generated packet routes now

`src/network/packets.h` still owns the packet enum; the wire structs are split
into `src/network/schema/packet-*.h`. The enum is the current generated-ID input.

The route registry will be generated from actual dispatch code:

- scan `src/network/multiplayer-tick.cpp` for client inbound branches;
- scan `src/network/server-packets.cpp` for server inbound branches;
- record packet ID, direction, source file, line, handler expression, and source
  owner;
- mark a direction `unknown` when no inbound branch exists;
- never default an unknown packet to resident;
- generate the ledger and runtime route header from that scan;
- emit `packet.route` once per packet/direction/generation at the real receive
  path;
- treat declared and observed path mismatch as a strict-gate failure.

No hand-maintained 78-row client mapping.

### 4. State migration

Start with `player_core`:

- v1 → v2 adds a field with a default;
- v2 → v3 removes a field;
- matching fields are copied;
- new fields receive defaults;
- validate before commit;
- failed migration leaves the old generation and old state active;
- journal prepare/validate/commit/rejected events;
- prove this in a hot swap, not only a unit test.

### 5. Audit closure

Classify rows into:

- must move to DLL;
- needs resident adapter;
- needs runtime proof;
- intentional exception.

Gate on unresolved game blockers. AI reviews exceptions and ambiguous rows,
not 797 hand-maintained JSON entries.

### 6. Reviewed EXE cutover

- remove the exact actual game-source worklist from the EXE target;
- delete legacy bridge files and fallbacks;
- make the actual link gate fail if any game-owned source enters the EXE;
- keep only bootstrap/resident/loader/barrier/renderer/logging/GLAD in the EXE.

### 7. Final same-PID proof

- edit → build candidate → activate in the same PID/session;
- visible result in `events.jsonl`;
- rollback restores prior behavior;
- record source/build/runtime/human evidence separately.

## Implemented in this pass

- Mouse Y convention: mouse-down is positive `dy`; both `Camera::updateMouse`
  and `InputRouter::applyPendingLook` subtract pitch so mouse-down looks down.
- `build.py` now writes `build/link-inputs-<mode>.json` with the actual object
  and source list passed to the linker.
- The current linker response was mapped against ownership into
  `build/hot-reload/exe-worklist.json`: 610 linked sources, 571 game-owned,
  36 resident, 3 bootstrap. This is the exact pre-next-build worklist.
- `exe_source_relpaths()` now filters the EXE target to cold owners only. The
  current cold EXE source count is 34.
- A `build/link-inputs-debug.json` sidecar started from the older source-list
  behavior was observed pending with 590 game entries. Do not use that pending
  sidecar as the post-cutover census; use the next build started after the
  ownership filter change.
- `exe_cutover_gate.py` and `build_target_gate.py` read that actual link input
  first and fail closed when it is unavailable.
- `packet_registry_codegen.py` now derives packet paths from actual inbound
  dispatch branches and source references. It no longer reads the hand
  `packet-schema.json`.
- `packet-ledger.json` and `packet-registry-v1.h` were regenerated. Current
  generated counts: client `dll=74`, `resident=0`, `unknown=4`; server
  `dll=74`, `resident=0`, `unknown=4`, `tombstones=4`. The four unknown IDs are not referenced
  as inbound handlers or active source references and need tombstone/retention
  review.
- Runtime `packet.route` events were added at the client and server receive
  boundaries.
- `StateMigrateFn` now returns a validation result. Rejected migration leaves the
  old state committed. `state.migration-selftest` exercises old → new, new field
  default, rejected migration, and old-state preservation in one process.
- NPC authored animation tick-lock now defaults on, so avatars without an
  explicit `npcTickLocked` flag use their authored clips at one authored tick per
  fixed simulation tick.
- `config/debuglogger.json` performance target is now 240 FPS, so the
  `performance.fps` journal budget is 4.16 ms rather than 1 ms. Actual FPS is not
  claimed.
- CAP_RENDER cleanup removed GLAD from 31 type-only game files, removed the six
  legacy bridge files, converted GL types to semantic/`stdint` types, moved
  GPU timer queries behind the resident GlResource capability, and routed
  `gl-debug` through CAP_RENDER `getError`. The current `cap_render_gate`
  reports 0 raw-GL game sources.
- The existing NPC closed-solid safety push and `npc.collision.stuck` evidence
  remain the first collision-fix path.

## Still not proven

- No build or runtime activation was performed in this pass.
- `player_core` generation-to-generation migration is not yet wired; the
  standalone migration API selftest is not a live swap proof.
- Actual link-input census must be regenerated by the next build.
- Packet route annotations must be validated by runtime `packet.route` events.
- NPC collision and authored animation need live visual acceptance.
- Audit bucket closure, EXE cutover, legacy deletion, and final same-PID
  rollback remain open.

## Deferred

- devscripts folder refactor;
- `docs/specs/devscripts/*`;
- consolidation of one-off scripts;
- generated state-schema extraction from C++ annotations.

The direct link census, packet route truth, state migration, audit closure,
fallback deletion, and same-PID proof are not deferred.
