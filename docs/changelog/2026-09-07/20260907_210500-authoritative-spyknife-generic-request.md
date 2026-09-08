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

## Follow-up log investigation

- Reviewed the supplied runtime files `Server_log_191651.txt`,
  `Gameterminal_log_191641.txt`, `Network_log_191641.txt`, and
  `SpyKnife_log_191655.txt`.
- The server is receiving the current Spy Knife batch packet and resolving the
  Stable Weapons slot correctly. The latest runtime evidence is not a packet
  type or slot rejection: it logs `bytes=276`, `EQUIP_RESOLVED`, and 21
  authoritative `NPC_APPLIED` contacts.
- The rejection counts in the server log are 71 `invalid_contact`, 31
  `distance`, 4 `attacker_inactive`, and 3 `npc_dead`. The dominant failure is
  future contact ticks, for example `contactTick=30850` through `30854` at
  `serverTick=30794`, which fails the existing `pkt.contactTick > tick` guard.
- The distance guard is currently hard-coded to `3.0f`, and its failure uses
  `return`, ending the whole handler instead of allowing later contacts in the
  same batch to be evaluated. This explains additional under-acceptance when a
  batch contains one invalid contact followed by valid contacts.
- No gameplay validation was loosened in this investigation. The confirmed
  follow-up work is to correct the client/server tick-domain alignment and
  change batch processing to per-contact continuation before tuning the
  specification-defined contact tolerance.

## Safe follow-up implementation

- `src/combat/weapon-spyknife.cpp` now clamps the target-specific rendered
  contact tick to `latestServerTick` (or the latest local snapshot fallback),
  preventing future historical claims while preserving the client-side hit
  presentation and batching behavior.
- `src/network/server-packet-handlers.cpp` now uses `continue` for per-contact
  dead-target, missing-target, inactive-target, and distance failures. A bad
  contact no longer aborts later contacts in the same batch.
- The player distance path now uses centralized Weapons diagnostics rather than
  `printf`, with contact ID and tick context.
- The 3.0-unit validation tolerance was deliberately not widened in this safe
  patch; it needs a separate geometry/spec measurement so the server does not
  accept impossible hits.

## Validation and review

- Routed behavior review: `docs/skills/spec-behavior-review-v1.md` —
  PASS_WITH_HUMAN_REVIEW. The implementation now preserves instant client
  prediction, bounds claims to usable server history, and processes contacts
  independently. Runtime acceptance is still required for actual NPC and
  player play.
- Task completion guidance reviewed: `docs/operations/task-completion/task-completion.md`.
- `git diff --check`: passed; only normal line-ending conversion warnings were
  reported.
- Canonical build: `python build_agent.py` completed successfully at
  `2026-09-07 19:30:37`, compiling `weapon-spyknife.cpp` and
  `server-packet-handlers.cpp`; `build/changelog.txt` reports `Status: SUCCESS`.
- Human review still required: run the rebuilt client/server, confirm the
  server no longer reports future contact ticks, verify multiple valid contacts
  in one batch all apply, and measure whether the unchanged 3.0-unit gate is
  consistent with the shared melee collision geometry.

## Client/server hitbox alignment follow-up

- Investigation found that the client uses the configured Spy Knife OBB while
  the server used only a hard-coded 3.0-unit root-to-target distance. This was
  the remaining geometry mismatch behind many predicted-but-rejected contacts.
- Added hot-reloadable `serverContactRadius: 4.0` under the Spy Knife
  `custom_params` in `config/weapons.json`. The server uses this authoritative
  value for NPC and player contact validation and logs it alongside the active
  `hitboxHalfX/Y/Z` values.
- This is an acceptance-envelope alignment, not client authority: damage,
  health, death, and knockback remain server-owned, and client-supplied damage
  remains ignored.
- Validation after this change: canonical `python build_agent.py` completed at
  `2026-09-07 20:09:17` with `Status: SUCCESS`, compiling and linking the
  updated executable. Runtime two-client/NPC acceptance remains required.

## Hitbox visualization investigation

- `src/combat/weapon-spyknife.cpp` was drawing the configured OBB with the
  generic `DebugVis::drawLine` queue, while the render loop flushes weapon
  collision visuals with `DebugVis::flushWeaponLines()` after the normal debug
  line stage. This explains why `hitboxVisible: 1.0` produced no visible box.
- The supposed fill was also three line segments per face, not filled
  triangles, and used `alpha * 0.3`; `hitboxAlpha: 1.0` therefore was not
  opaque. The edges/faces now use `drawWeaponLine` and the configured alpha is
  clamped directly.
- Full server OBB reconstruction remains the next scoped change. The current
  contact packet does not carry enough historical attacker/knife orientation
  data for the server to reproduce the client's oriented box exactly, so the
  current radius approximation remains authoritative until that packet/state
  extension is implemented.
- Validation after the visualization fix: canonical `python build_agent.py`
  completed at `2026-09-07 20:15:34` with `Status: SUCCESS`, compiling and
  linking `weapon-spyknife.cpp`. `git diff --check` passed. Runtime visual
  confirmation and the subsequent full historical OBB protocol work remain.

## Historical contact OBB transport

- `SpyKnifeContact` now carries the client contact OBB center, half-extents,
  and three axes. The client fills these fields from the same `BladeOBB` used
  for local collision and sends them with each batched contact.
- The server validates finite values, positive extents, approximately
  orthonormal axes, configured-size bounds, and plausible box origin. It then
  performs an OBB-versus-historical-target-sphere query for NPCs and players;
  root-distance is no longer the contact decision.
- Added hot-reloadable `serverTargetBodyRadius` under Spy Knife config for the
  target body approximation used by the shared server query. Server damage and
  health remain authoritative.
- Canonical build completed at `2026-09-07 20:22:07` with `Status: SUCCESS`,
  compiling 114 objects and linking `mimita.exe`. Runtime two-client/NPC
  acceptance remains required, especially checking packet compatibility and
  client/server hitbox agreement.
