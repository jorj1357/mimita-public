# Client Prediction and Server Authority Disagreement

Time created: `2026-10-29T17:13:46Z`
Time last updated: `2026-10-29T17:13:46Z`

Status: UNRESOLVED

Related specifications:

- `docs/specs/networking/contact-authority.md`
- `docs/specs/networking/networking.md`
- `docs/specs/weapons/melee/melee-weapons.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/specs/dev-loop/quality-gates.md`

Related changelog:
`docs/changelog/2026-10-29/20261029_171346-contact-authority-cleanup.md`

---

## Regression Occurrence 1

### Observed

Time: `2026-10-29T15:45:06Z`

The client displayed many predicted melee hits that did not become
authoritative damage. The scenario and journal are:

- `devscripts/scenarios/user/scenario-20261029_154506-meleeaaa.jsonl`
- `logs/2026-10-29/20261029_150910_01/events.jsonl`

Journal counts for the session:

- `191` `melee.client.contact_claim`
- `164` `melee.contact.claim.rejected`
- `22` `melee.contact.claim.accepted`
- `98` rejects with reason `target_invalid`
- `53` rejects with reason `out_of_reach`
- `7` rejects with reason `invalid_contact_bounds`
- `6` rejects with reason `outside_claim_radius`

The user also reported frequent “melee move revision rejected” errors, fake
client-only hit feedback, deathball doing no damage to the player, and weapon
contacts appearing in empty space.

### Expected Behavior

If the client sees a weapon contact against an actor and the server accepts the
contact as plausible, the server applies exactly one authoritative result and
notifies the client. Client prediction must match the server’s eligibility,
tick, geometry, deduplication, and damage rules.

Same-team contacts must not predict damage. Weapon-vs-weapon contact is a bounce
or clash and never applies health damage.

### Actual Behavior

The client and server disagreed for several independent reasons. The client
predicted hits that the server rejected, and the client showed more hit feedback
than the server could confirm.

### Confirmed Causes

#### 1. Move-set revision compared process-local counters

`MeleeMoveSetRegistry::set()` increments a process-local counter
(`src/combat/weapon-melee-config.cpp:517-538`). The client sends that counter
(`src/network/multiplayer-packets.cpp:685-688`,
`src/network/multiplayer-tick.cpp:2774-2775`). The server compares it
(`src/network/server-attack.cpp:1258-1268`).

The two processes load move sets in different orders, so the counters differ
even when the content hash matches. Evidence from the recorded session:

- client revisions: mace `55`, chainsaw `53`, captain `51`, swordsword `56`
- server load revisions: mace `6/7`, chainsaw `4/5`, captain `2/3`, swordsword
  `1/8`
- matching content hashes: mace `1736998449`, chainsaw `789656715`, captain
  `3489839331`, swordsword `2929735422`

The rejection reason is not a real content mismatch.

#### 2. Friendly-fire/team policy was not applied to client prediction

The server blocks same-team actor damage when friendly fire is disabled
(`src/network/server-gamemode.cpp:5505-5534`,
`src/network/server-damage.cpp:358-375`). The claim handler reports that as
`target_invalid` (`src/network/server-packet-handlers.cpp:1188-1200`).

The policy log records `98` blocked melee claims with attacker team `0`, victim
team `0`, and friendly fire `false`. The target health values show the targets
were alive. The client predicted those hits because its prediction path did not
apply the same team rule.

#### 3. Claim tick was not the rendered target tick

The client sends `ctx.latestServerTick` as `contactTick`
(`src/combat/melee-client.cpp:531-532`). That is the newest received server
tick, not necessarily the pose the client rendered. The server then rewinds the
target to that tick (`src/network/server-packet-handlers.cpp:1215-1220`).

Hitscan already uses a per-target render tick
(`mpFireRenderTickForTarget`, `src/network/multiplayer-packets.cpp:80-106`).
Melee did not. This produced target-position disagreement, including the
recorded `out_of_reach` rejects with client-visible reach but server
`reach=9.4 m` against `max_reach=6.27 m`.

#### 4. Validation used the target center instead of the claimed contact

The server used the target center at `+0.9 m` for distance/overlap
(`src/network/server-packet-handlers.cpp:1221-1249`). A valid limb hit or the far
end of a long mace box can be inside the claimed volume while the target center
is farther away.

#### 5. Client prediction was not deduplicated like the server

The client applied predicted damage and hit feedback for every detected contact
(`src/combat/melee-client.cpp:511-526`). The server deduplicates by
`(targetId, contactTick)` (`src/network/server-packet-handlers.cpp:1123-1126`).
Multiple box/body-part contacts in one tick therefore produced several client
effects but only one server result.

#### 6. Three different melee damage formulas

- client prediction: `clientContactDamage`
  (`src/combat/melee-client.cpp:263`)
- server claim: inline formula
  (`src/network/server-packet-handlers.cpp:1299-1327`)
- server actor fallback: inline formula
  (`src/network/server-physical-contact-actors.cpp:550-592`)

They use different speed sources and also differ on `force_scale` in knockback.
This causes predicted-versus-confirmed damage disagreement even when a contact
is accepted.

#### 7. The journal contained more than one runtime owner

`logs/2026-10-29/20261029_150910_01/events.jsonl` contains:

- a dev-loop sandbox server, pid `416`, map `funworld3`
- a client-local TDM runtime, pid `7348`, map `dust2cyberiav4`

The dev-loop server had no players. The played session was not the dev-loop
sandbox server. The mixed journal makes evidence ambiguous and prevents
multi-client recording.

#### 8. Deathball and same-team actors

Deathball player contact is filtered by the same team/friendly-fire policy, but
there is no explicit rejection diagnostic for that filter. The session recorded
`deathball.contact` only against NPC targets and no player contact event.

### Why This Is Bad

- Client prediction is supposed to be immediate feedback, not a different game.
- Same-team prediction creates fake damage and fake hitmarkers.
- Wrong tick/geometry rejection makes valid visual hits disappear.
- Duplicate formula owners make accepted damage disagree with prediction.
- A mixed-process journal makes debugging and multiplayer evidence unreliable.
- Deathball appears broken because the filtering decision is invisible.

### Specification

`docs/specs/networking/contact-authority.md`

Relevant requirements:

- one contact query -> validation -> damage -> `applyActorDamage` pipeline
- one damage sink
- same-team contacts do not damage
- contact tick is the rendered target tick
- one result per target/tick
- client and server journals are separate

### Wrong Code

- `src/combat/weapon-melee-config.cpp:517-538`
- `src/network/multiplayer-packets.cpp:685-688`
- `src/network/multiplayer-tick.cpp:2774-2775`
- `src/network/server-attack.cpp:1258-1268`
- `src/combat/melee-client.cpp:511-545`
- `src/network/server-packet-handlers.cpp:1123-1249`
- `src/network/server-packet-handlers.cpp:1299-1334`
- `src/network/server-physical-contact-actors.cpp:550-592`

### Confirmed Cause

The project had parallel melee contact owners, process-local move-set identity,
client prediction without server team/tick/geometry policy, and one mixed
client/server journal.

### Attempted Fix

None yet. This record is the baseline for the deletion and consolidation work.

### Corrected Code

Pending implementation.

### Fix

Planned:

1. content-hash move-set identity
2. per-target render tick for claims
3. shared target capsule/body-volume validation
4. server and client use the same authored melee box when configured
5. same-team prediction suppression
6. client prediction dedupe
7. one shared melee damage/knockback resolver
8. explicit deathball contact rejection diagnostics
9. client-owned scenario journals and separate server journals

### Proof

Human review:

Pending.

Automated proof:

Pending. The future matrix must cover NPC/NPC, NPC/player, player/NPC, and
player/player, including bad-connection profiles.

### Solution

`UNRESOLVED`

---

## Legacy and Duplicate Owner Inventory

This section is the append-only deletion and rejection record. It exists so a
future change does not reintroduce the same parallel owners.

### Delete Now — Inert or No Callers

| Item | Evidence | Replacement |
|---|---|---|
| `server-melee.cpp` legacy file path | `handleMeleeHitRequest` returns at `:135`; `tickServerSwordCombat` has no caller | shared contact pipeline |
| `handleMeleeHitRequest` | only reached by dead `PACKET_MELEE_HIT_REQUEST` route | `handleAttackRequest` + shared contact |
| `tickServerSwordCombat` | no caller; declaration only in `server.h:1181` | `tickServerMeleeActions` / shared contact |
| `computeBladeCapsule`, `computeHitDamage`, local `sweptSphereOverlap` | only used by the inert functions above | shared contact geometry/damage |
| `mpSendMeleeHitRequest` | no call sites | `mpSendAttackRequest` |
| `MeleeHitRequestPacket` | sender is dead; receiver is no-op | `AttackRequestPacket` / contact claim |
| `PACKET_MELEE_HIT_REQUEST` route | routes only to no-op | remove with packet |
| `startServerSwordAttack` + branch | guarded by absence of move set; shipped sword family has move sets | `serverMeleeStartAction` |
| `projectileConfig(uint8_t)` | only `#if 0` | `projectileConfigFromDefinition` |
| `handleProjectileFireRequest` | disabled stub | `handleGenericProjectileAttack` |
| `mpSendProjectileFireRequest` | dead stub | `mpSendAttackRequest` |
| `WeaponSwordsword` legacy update path | `update()` returns at `:465` for animation-profile weapons | server melee action/contact |
| duplicate flush gate in `melee-client.cpp:943-948` | hard-coded `6`; configured flush exists at `:582-589` | configured `claimBatchTicks` owner |

### Delete After Replacement — Conditional

| Item | Delete After | Reason |
|---|---|---|
| `tickServerPhysicalContactWeapons` fallback loop | HAFS/QuickHit/Godball migrate to shared contact | duplicate damage loop |
| `buildPhysicalShape` | same migration | alternate blade shape |
| `physicalContactDamage` / `physicalContactKnockback` | shared melee resolver adopted | duplicate formula |
| `weapon-contact-local.cpp` | offline/local gameplay path removed or explicitly retained by spec | second application path |
| `applyCollisionContact` | legacy collision fallback removed and actor-triangle path covers all | duplicate response owner |
| `collectPlayerBodyCollisionSamples` | legacy/debug callers removed | body-sample approximation |
| `collectCapsuleRecoveryContacts` | legacy collision callers removed | superseded recovery contacts |
| `applyServerDamage` | all callers use `applyActorDamage` | compatibility wrapper |
| `MeleeHitEventPacket` | weapon-clash presentation moves to shared contact event | presentation-only packet |

### Do Not Reintroduce

- separate melee fallback and claim damage formulas
- process-local revision comparison
- target-center-only reach validation
- weapon-ID special cases in shared contact code
- direct `npc.health`/`player.health` mutation outside the damage sink
- one live journal written by both client and server
- a new packet for behavior the shared contact pipeline can carry
- local/offline gameplay rules that differ from server rules
