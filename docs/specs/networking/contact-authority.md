# Contact and Damage Authority

Time created: `2026-10-29T17:13:46Z`

Status: TARGET CONTRACT

## Purpose

Define one authoritative contact and damage model for every weapon in MiMITA.

This specification exists so future work does not add another hitscan path,
another projectile damage formula, another melee special case, or another
direct health writer. The code must match this document. If the code cannot
express this contract, update the specification first and explain why.

## Core rule

Every damaging interaction follows the same pipeline:

```text
geometry query
    -> ContactCandidate
    -> shared validation
    -> shared damage/knockback resolution
    -> applyActorDamage
    -> shared event publication
```

Weapon-specific code owns **how contact is found**. It does not own final health,
death, team policy, rewind policy, or duplicate policy.

## Canonical types

```cpp
enum class ContactKind : uint8_t {
    Hitscan,
    Projectile,
    Melee,
    Explosion,
    Deathball,
    WeaponClash
};

struct ContactCandidate {
    ContactKind kind;
    uint32_t attackerId;
    ServerActorKind attackerKind;
    uint32_t victimId;
    ServerActorKind victimKind;
    uint32_t victimGeneration;
    uint32_t contactTick;
    uint32_t contactId;
    glm::vec3 point;
    glm::vec3 normal;
    glm::vec3 direction;
    float speed;
    float directness;
    float distance;
    WeaponExecution::HitBodyPart bodyPart;
    bool predicted;
};
```

`ContactCandidate` is transport-neutral and simulation-neutral. It must not
contain packet pointers, renderer state, audio state, or weapon-specific
runtime pointers.

## Canonical resolution order

The shared resolver validates in this order:

1. attacker exists and is allowed to attack
2. victim exists and is alive/active
3. victim spawn generation matches the claimed generation
4. contact tick is valid for the shared history window
5. team/friendly-fire policy allows the contact
6. world occlusion does not clearly block the contact
7. distance/reach is plausible for the claimed volume
8. contact is not a duplicate/stale repetition
9. damage and knockback are resolved from the claimed contact plus config
10. `applyActorDamage` applies the result
11. the shared event owner publishes one contact/damage result

No weapon-specific path may skip a step. If a weapon needs a different policy,
that policy is explicit config and still passes through the same order.

## One damage sink

`applyActorDamage` in `src/network/server-damage.cpp` is the only combat path
allowed to change player/NPC health, death, or authoritative knockback.

No new code may contain direct writes such as:

```text
npc.health -= ...
npc.health = ...
player.health -= ...
player.health = ...
knockbackImpulse += ...
```

outside:

- `src/network/server-damage.cpp`
- actor spawn/reset/death lifecycle code
- tests and explicit gamemode scripted damage during migration

A devscript gate enforces this allowlist.

## Damage families

All families share the same contact candidate and application sink. They differ
only in how damage is resolved from config and contact data.

### Hitscan

- geometry: ray or swept beam vs body-part boxes/capsule
- damage: existing `computeHitscanDamage`
- falloff, headshot, limb multiplier, pellet spread stay hitscan-specific

### Projectile

- geometry: swept sphere/capsule vs world/actor
- direct hit: shared projectile contact profile
- explosion: shared splash resolver
- pass-through: explicit profile flags, not weapon-ID checks

### Melee

- geometry: capsule or OBB sweep
- when `melee_contact.use_weapon_collision_hitbox` is true, the client and
  server must use the exact same authored `weaponcollisions.json` box
- when false, both use the same model-derived attachment capsule
- damage: one shared melee contact resolver
- same team: bounce/contact only, no damage, no hitmarker

### Melee percent/knockback

Smash-style melee percent is a separate resource from HP. It uses the same
contact query and event publication, but resolves through the melee percent
resolver rather than the HP damage formula. It must not be duplicated into the
HP path and must not bypass contact validation.

### Deathball

Deathball is a projectile contact profile:

- pass-through actors/world
- contact radius
- center-to-edge damage mix
- knockback per damage
- tick interval
- wall lifetime decay

There must be no `if (weaponId == "deathball")` branch in the shared projectile
contact handler.

### Weapon collision

Weapon-vs-weapon and weapon-vs-body collision is physics interaction only:

- it produces impulse/bounce/push
- it may publish clash/presentation events
- it must never apply health damage

Weapon collision and damage are two consumers of the same contact model, not
two damage paths.

## Contact tick

The client must send the server tick corresponding to the pose it rendered.
Melee must use the same per-target render-tick mapping as hitscan
(`mpFireRenderTickForTarget` or its replacement), not the newest received server
tick.

The server must evaluate the target at the contact tick from bounded history.
If the tick is outside the history window, reject with an explicit reason.

## Move-set identity

Client and server compare content identity, not process-local revision counters.

- `MeleeMoveSet::hash` is the compatibility identity.
- Revision remains diagnostic only.
- A process-local counter must never reject a valid action.

## One contact result

A contact produces at most one damage result per target per weapon per fixed
tick. Client prediction must deduplicate to the same rule and must not display
additional hitmarkers or damage numbers that the server will not confirm.

## Master function map

| Behavior | Authoritative owner | Required function/file |
|---|---|---|
| Actor position | server movement tick | `ServerPlayer::pos`, `ServerNpc::pos` |
| Actor rewind pose | server history | `getPlayerPoseAtTick`, `getNpcPoseAtTick` |
| Contact tick mapping | shared network tick helper | `mpFireRenderTickForTarget`, `estimateServerRewindTick` |
| Target eligibility | shared policy | `serverFriendlyFireBlocks` + lifecycle/generation checks |
| Contact query | weapon geometry adapters | hitscan/projectile/melee/explosion query |
| Contact validation | shared contact resolver | single `resolveContact` entry point |
| HP damage/knockback | shared damage resolver | `resolveContactDamage`, `computeMeleeContactDamage`, `computeMeleeContactKnockback` |
| Melee percent | melee resource resolver | `resolveMeleePercent` |
| Explosion/splash | shared splash resolver | one server/client/offline formula |
| Damage application | server damage owner | `applyActorDamage` with `combat.max_authoritative_damage` safety bound |
| Death | server damage/lifecycle owner | `applyActorDamage` + lifecycle transition |
| Contact event publication | shared event owner | one contact/damage event per result |
| Weapon collision bounce | physics collision owner | `collectBodyWeaponContacts`/response owner |

This table is normative. New code must update it before adding a new owner.

## Geometry roadmap

### Current

- hitscan: ray/beam vs boxes/capsule
- projectile: swept sphere/capsule vs world triangles and actor capsules
- melee: capsule or authored OBB vs target body volume
- deathball: swept sphere with pass-through

### Next

- server melee damage uses the same authored boxes as the client claim when
  `use_weapon_collision_hitbox` is true
- one shared actor hit volume (`buildActorHitVolume`) for hitscan, splash,
  deathball, and melee claim validation

### Future

Arbitrary projectile and weapon meshes may use mesh-vs-mesh collision:

- `projectile_shape: "sphere" | "capsule" | "mesh"`
- transformed projectile triangles cached per tick
- BVH broadphase against world/actor collision meshes
- swept triangle-vs-triangle narrowphase
- contact point/normal produced into `ContactCandidate`
- no per-query allocations in the 60 Hz path
- opt-in per weapon because mesh collision is expensive

Melee may use the same mesh path for model-vs-model contact. Until then,
boxes/capsules are the supported damage geometry.

## Forbidden patterns

- a second HP damage formula for one weapon family
- a weapon-ID special case in shared contact code
- a direct health/knockback write outside the damage sink
- a client claim that the server does not validate
- a server fallback that can apply the same contact twice
- a server and client writing one live journal
- a local/offline gameplay path with different rules
- adding a new packet for a behavior the shared contact model can carry

## Enforcement

The following must exist in the repository:

- `devscripts/quality/check_damage_authority.py`
- `devscripts/quality/check_contact_ownership.py`
- `devscripts/quality/dead_code_report.py`
- `devscripts/quality/duplicate_report.py`
- `devscripts/quality/coverage_report.py`
- actor/weapon/network matrix scenarios

Rules are report-only until the baseline is clean. Changed-file gates become
blocking after the deletion and consolidation work.
## Accuracy targets (2026-10-29)

Accuracy is measured as the percentage of predicted damaging contacts whose
final server outcome matches the client prediction.

- RTT below `100 ms`: `99.99%` claim acceptance and exact damage match.
- RTT `100 ms` or more: `99%` claim acceptance and exact damage match.
- The target is long-term and is measured across the full actor/weapon/network
  matrix, not from a single run.

A predicted damaging contact is the denominator. Weapon-clash bounces and
same-team contacts that correctly produce no damage are excluded.

## Late contact policy

A late but plausible contact is accepted within the bounded history and
tolerance window. Reject only when the contact is impossible or invalid:

- missing/dead/inactive actor
- generation mismatch
- friendly fire/team block
- stale or outside-history tick
- outside the claimed target volume
- beyond the claimed reach
- clearly world-occluded
- duplicate/stale contact
- malformed impact data

Playtesting may tune the tolerance/history window, but it must not introduce a
weapon-specific shortcut.

## Damage cap policy

There is no low artificial damage cap. A weapon configured to deal `250` must
be able to deal `250`.

`applyActorDamage` and `applyServerDamage` use the config value
`config/networkingconfig.json -> combat.max_authoritative_damage` as a
representation/safety bound, not as a gameplay balance cap. The default is
intentionally huge (`999999999999999`). Weapon/action configuration remains the
gameplay damage authority.

## No offline-only gameplay

Local play is still a client connected to a local server. There is no separate
offline gameplay simulation with different contact, damage, or validation rules.
`weapon-contact-local.cpp` is migration debt, not a target architecture.

## Implementation phases

1. content-hash move-set identity
2. per-target render tick and target generation in claims
3. client prediction dedupe and team parity
4. shared target volume and reach validation
5. one shared melee damage/knockback resolver
6. one shared contact resolver for all weapon families
7. `applyActorDamage` as the only combat damage sink
8. client-owned journals and N-client matrix runner
9. server-view debug overlay
10. accuracy measurement and playtesting
## Implementation status (2026-10-29)

`src/network/server-contact.h/.cpp` now provides
`resolveServerContact(...)`, the shared victim lookup, generation/lifecycle/team
check, and `applyActorDamage` path.

Migrated to it:

- hitscan player targets
- hitscan NPC targets
- projectile-rifle direct player targets
- projectile-rifle direct NPC targets
- deathball player/NPC tick contacts

Still pending migration:

- projectile explosion player/NPC splash loops
- godball claim application
- full shared geometry validation for every family
