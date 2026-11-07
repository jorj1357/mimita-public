# Healthall Left Active NPC Maximum Health Stale

- Recorded: 2026-11-06T23:37:00Z (2026-11-06 18:37 EST, America/New_York)
- Status: FIXED IN SOURCE; LIVE COMMAND AND 500/500 HUD VERIFIED; DAMAGE-SOAK AND HUMAN REVIEW PENDING
- First reported in `docs/specs/open-gameplay-issues.md` under the user's 2026-11-06 18:37 note: “HEALTHALL CMD DOESNT WORKk”. The same note separately says NPCs kill the player too quickly with the revolver.

## Expected behavior

The registered command is described as “Set spawn HP for ALL entities (player, NPCs).” Its existing handler also promises to heal every living entity immediately. When the host runs `healthall 500`, each living player and NPC must have both its current health and authoritative maximum health set to 500, and future spawns must use that override.

The shared damage specification says `applyActorDamage` is the only combat path allowed to change player/NPC health, and the server owns authoritative health and death. `docs/specs/networking/contact-authority.md`, “One damage sink”; `docs/specs/networking/networking.md`, authoritative server model. The terminal-command specification requires connected host commands to use the normal server packet and authority gate.

## Actual behavior and cause

The network command handler healed player `health` and `maxHealth`, but for NPCs it changed only the `ServerNpc.health` mirror. The next mirror synchronization copied that current value to `Npc.body.currentHp` while leaving `Npc.body.maxHp` unchanged. The real NPC body is the combat/AI owner, so its active health state could disagree with the 500 HP displayed or replicated from the mirror.

The first incorrect step was `handleServerCommand` in `src/network/server-packet-chat.cpp`: the old NPC loop assigned only `kv.second.health = effectiveMax`. `syncServerNpcDamageToNpc` in `src/network/server-npcs.cpp` subsequently copied that into `n.body.currentHp`, but did not update `n.body.maxHp`. Spawn/adoption paths already set both body fields, which is why this divergence affected already-living NPCs after an in-match command.

The command does not reduce weapon damage or make actors invulnerable. A hit whose applied damage reaches the current health can still kill an actor. The user's report also mentions high NPC revolver damage; this change fixes the active NPC max-health mismatch and does not claim that weapon lethality is balanced.

## Exact code change

Wrong behavior:

```cpp
const int effectiveMax = serverMaxHp();
for (auto& kv : players)
    if (!kv.second.dead) {
        kv.second.maxHealth = effectiveMax;
        kv.second.health = effectiveMax;
    }
for (auto& kv : npcs)
    if (kv.second.health > 0)
        kv.second.health = effectiveMax;
```

Corrected behavior in `handleServerCommand`:

- Preserve each player's per-role/default health and self-only `healthme` value when `healthall default|reset` removes the global override.
- For every living NPC with a live `Npc` body, set `body.maxHp` and `body.currentHp` together, then set the server snapshot mirror's health to the same effective maximum.
- Emit the rate-limited-by-occurrence structured event `server.healthall.applied` once per command with the accepted command, override, and live player/NPC counts. This is a command-boundary event, not a per-tick log.

## Proof

Source and build:

- Changed owner: `src/network/server-packet-chat.cpp`, `handleServerCommand`, `healthall` branch.
- Build: canonical dev-loop build succeeded for artifact `2026-11-06T23:42:37.678199Z`, source generation 35, SHA-256 `c067c2c143ad2e1f982cc0aaf4dbbb31e06fad6d1131a334507885cd764743ed`.

Live run:

- Run: `20261106_234315`, launch mode 47, Fun Counter Strike, two connected clients and an NPC roster.
- Server journal: `logs/2026-11-06/20261106_234315/server-01/events.jsonl`.
- Client journal: `logs/2026-11-06/20261106_234315/client-01/events.jsonl`.
- Server event `network.server-command.result-sent` (sequence 23665, tick 4855) records `command="healthall 500"`, `accepted=true`, and `is_host=true`.
- Server event `server.healthall.applied` (sequence 23666, tick 4855) records `override_hp=500`, `players_healed=2`, `npcs_healed=18`, and `npc_system_available=true`.
- Client event `network.server-command.result` (sequence 3550) records `accepted=true` and `status="applied: healthall 500"`.
- The live client HUD visibly showed `HP: 500/500` after the command.

The verification run proves the real command packet was accepted and the live server updated two player records and all 18 live NPC bodies. It does not include an observed post-command combat hit or time-to-death comparison. The user interrupted Computer Use with the physical Escape key before that additional interaction. Human damage-soak and revolver lethality review remain open.

## Related record

- Session changelog: [20261106_184930-healthall-live-npc-fix.md](../../changelog/2026-11-06/20261106_184930-healthall-live-npc-fix.md)
