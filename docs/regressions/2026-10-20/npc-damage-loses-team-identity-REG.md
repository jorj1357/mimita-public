# NPC Damage Loses Team Identity

Time created: 2026-10-20T22:29:03Z
Time last updated: 2026-10-20T22:29:03Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/gamemodes/juggernaut.md`

Related specification:
`docs/specs/networking/networking.md`

Related changelog:
`docs/changelog/2026-10-20/20261020_222903-juggernaut-damage-identity.md`

---

## Regression Occurrence 1

### Observed

Time:
`2026-10-20T22:20:09Z`

Journal:
`logs/2026-10-20/20261020_221119/events.jsonl`

Expected: enemy Juggernaut hits reduce Fighter health, while same-team hits are
blocked.

Actual: a Juggernaut hitscan hit was rejected as same-team damage. Its
`server.damage.rejected` record reports `attacker_id=0`, `attacker_team=0`, and
`target_team=0`. The NPC hit path still had the real NPC ID/team when it logged
`npc.damage`.

Why this is bad: the player sees and hears incoming attacks but does not lose
health, so the Juggernaut can appear unable to win a round.

### Specification

The Juggernaut specification says the mode reuses shared actor damage and
networking systems. The networking contract says the server owns the final
damage result. The missing requirement was that NPC identity and team survive
the NPC-to-player damage call and reach the friendly-fire policy.

### Exact wrong code

```cpp
ServerDamageResult result = applyServerDamage(
    players, *nearestPlayer, 0, damage, knockback,
    ServerDamageSource::Hitscan);
```

The shared typed owner also rejected every non-player attacker with
`phase1-player-attacker-required`.

### Corrected code

```cpp
damageRequest.attacker = {
    n.id, ServerActorKind::Npc, nullptr, nullptr};
damageRequest.victim = {
    nearestPlayer->id, ServerActorKind::Player, nullptr, nullptr};
ServerDamageResult result = applyActorDamage(
    players, npcs, damageRequest);
```

The shared owner accepts a real NPC attacker and uses its team for the same
friendly-fire rule used for players. Friendly fire remains disabled.

### Confirmed cause

The NPC-to-player call substituted attacker ID zero in the player-only damage
wrapper. The friendly-fire check then used the wrong identity/team and rejected
the enemy hit. The journal fields directly show the substituted ID and the
rejection reason.

### Fix and proof

Fix: preserve the NPC actor reference and team through the shared damage owner;
include attacker kind and team in damage result diagnostics.

Proof: source change is recorded in the related changelog. Build and fresh
runtime proof are pending; do not mark this solution confirmed until a real
Juggernaut round shows enemy damage applying and same-team damage blocked.

### Related changelog

`docs/changelog/2026-10-20/20261020_222903-juggernaut-damage-identity.md`
