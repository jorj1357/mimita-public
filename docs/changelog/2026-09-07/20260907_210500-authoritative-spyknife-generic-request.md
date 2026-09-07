# Authoritative Spy Knife generic request transport

- Branch: current working branch (not renamed or committed by this task)
- Timestamp: 2026-09-07T21:05:00Z
- Pre-existing changes: the worktree contained unrelated configuration,
  documentation, gameplay, and networking edits before this implementation;
  they were preserved and not attributed to this task.
- Specification reviewed: `docs/specs/weapons/melee-weapons.md`, especially
  the 60 Hz contact/10 Hz transmission model, immediate prediction,
  authoritative reconciliation, generic networking ownership, and NPC/player
  equivalence; `docs/specs/networking/networking.md` for client prediction and
  server authority.
- Focused skill reviewed: `docs/skills/spec-behavior-review-v1.md`.

## Changes

- `src/combat/weapon-spyknife.cpp`
  - Routed batched physical knife contacts through the established
    `PACKET_NPC_DAMAGE_REQUEST` transport instead of the rejected knife-only
    packet type.
  - Removed the temporary client-authoritative remote NPC health mutation and
    local kill declaration. Client damage numbers, effects, prediction overlay,
    and temporary impulse remain immediate; authoritative health remains server
    owned.
  - Replaced contact IDs derived from the pending-vector length with a
    monotonically increasing contact serial so contacts remain deduplicable
    across batches.
  - Made the batch interval configurable through the Spy Knife custom weapon
    parameters, defaulting to 10 simulation ticks (approximately 10 Hz).

- `src/network/server-packets.cpp`
  - Preserved the existing single `NpcDamageRequestPacket` behavior.
  - Added size-based routing for the fixed-size physical melee batch on the
    same generic request type, sending it through the existing Spy Knife
    historical validation and authoritative damage handler.

- `src/network/packets.h`
  - Set the physical Spy Knife batch maximum to six contacts, matching the
    initial six contacts per ten simulation ticks target.

- `config/weapons.json`
  - Added hot-reloadable `networkBatchIntervalTicks: 10.0` and
    `networkBatchMaxContacts: 6.0` Spy Knife parameters.

## Validation

- `python build_agent.py`: passed; `build/changelog.txt` reports
  `Status: SUCCESS`, return code 0.
- `git diff --check`: no errors from the files changed for this task. It still
  reports a pre-existing trailing-whitespace warning in
  `docs/specs/weapons/weapons.md:16`.
- JSON/config ownership was checked and the new parameters are under the
  active `spyknife.custom_params` object.

## Remaining human/runtime review

- Run a rebuilt two-client plus client-to-NPC test.
- Confirm logs show generic request receipt, Spy Knife contact acceptance,
  authoritative NPC health reduction, and prediction confirmation/rejection.
- Confirm no packet is logged as knife type 64 or rejected as `unknown-type`.
- Confirm changing the two Spy Knife network parameters while running takes
  effect through the existing weapon configuration hot reload.

## Follow-up slot-resolution fix

- Evidence reviewed after the initial implementation showed the packet reaching
  `[SPYKNIFE_NET] DISPATCH batch=1 bytes=272`, but the server's Spy Knife
  handler could still reject the attacker because Stable Weapons uses logical
  slot 4 while the Spy Knife weapon definition uses native slot 12.
- `src/network/server-packet-handlers.cpp` now resolves the configured logical
  slot with `serverCommunityWeaponLogicalSlot()` and native slot with
  `serverCommunityWeaponNativeSlot()`. It accepts the matching logical,
  native, or weapon-definition slot representation while retaining ownership
  and equipped-weapon validation. It logs the resolved mapping.
- Added centralized handler rejection/application diagnostics in the same file
  for packet size, attacker state, spawn generation, equipped weapon, contact
  validation, distance, dead targets, and authoritative NPC health changes.
- Validation: rebuilt with `python build_agent.py`; `build/changelog.txt`
  reports `Status: SUCCESS`, return code 0. Runtime two-client/NPC acceptance
  remains required.

## Follow-up historical tick fix

- `src/combat/weapon-spyknife.cpp` no longer stamps contacts with
  `MultiplayerContext::tick`, which advances per network update and caused
  future claims such as `contactTick=8002` against `serverTick=2035`.
- Knife contacts now use the existing target-specific `mpFireRenderTickForTarget`
  helper, aligning the claimed contact with the server tick domain and the
  historically rendered NPC pose.
- Validation: canonical build completed with `Status: SUCCESS`, return code 0.
- Remaining validation: run the rebuilt client/server and confirm contacts no
  longer log `contactTick > serverTick`, followed by `[SPYKNIFE_SERVERDMG]
  confirmed=yes` and reduced authoritative NPC health.
