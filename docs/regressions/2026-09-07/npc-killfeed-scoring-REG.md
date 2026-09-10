# NPC Killfeed and FFA Scoring Broken

Time created: 2026-09-07T00:00:00Z
Time last updated: 2026-09-10T00:00:00Z

Status: ATTEMPTED FIX (4)

Related specification:
`docs/specs/networking/networking.md`

Related changelog:
`docs/changelog/2026-09-09/` (multiple sessions)

---

## Regression Occurrence 1

### Observed

Time:
`2026-09-07T00:00:00Z`

### Expected Behavior

When an NPC kills a player in FFA mode:
1. The killfeed should show the NPC's name and weapon (e.g., "NPC-1000 killed admin with Shotgun")
2. The NPC should receive +1 point in FFA scoring
3. All clients (victim, attacker, observers) should see the killfeed entry
4. The killfeed weapon should resolve to the NPC's actual equipped weapon display name

### Actual Behavior

1. Killfeed shows "unknown killed admin with unknown" — both NPC name and weapon are "unknown"
2. Neither the NPC nor the player receives FFA points for NPC kills
3. Only the victim's client sees the killfeed entry (observers see nothing)
4. The weapon shows "unknown" because `networkWeaponTypeForDefinition` only maps 8 hardcoded weapons

### Why This Is Bad

NPC kills are a core gameplay mechanic in FFA mode. Without proper killfeed display, scoring, and universal visibility, the game mode is non-functional for NPC combat.

### Specification

`docs/specs/networking/networking.md`

Relevant requirement:
All kills should be visible to all connected clients through the killfeed system, with correct attacker name, victim name, and weapon display name.

### Wrong Code

File:
`src/network/network-weapons.cpp:18-39`

```cpp
uint8_t networkWeaponTypeForDefinition(const WeaponDefinition& definition)
{
    if (definition.id == "revolver" || ...)
        return NETWORK_WEAPON_REVOLVER;
    // ... only 8 weapons mapped
    return NETWORK_WEAPON_NONE;  // <-- any other weapon returns NONE
}
```

File:
`src/network/multiplayer-projectiles.cpp:1175-1179`

```cpp
std::string weaponDisplay = "unknown";
const char* weaponId = networkWeaponTypeName(event->weapon);  // "none" for unmapped
if (const WeaponDefinition* wdef = WeaponRegistry::instance().get(weaponId))
    if (!wdef->displayName.empty())
        weaponDisplay = wdef->displayName;  // never reached for unmapped weapons
```

### Confirmed Cause

1. `networkWeaponTypeForDefinition` is a hardcoded if-chain that only maps 8 specific weapon IDs. Any NPC weapon not in this list gets `NETWORK_WEAPON_NONE`, which resolves to "none" in the registry, which has no display name, so the killfeed shows "unknown".

2. The `DamageConfirmedEventPacket` carries `uint8_t weapon` (the hardcoded enum) but has no field for the dynamic `weaponDefNetworkId` that the `AttackRequestPacket` system already uses.

3. FFA scoring in the mapOnly (community match) path increments `d.communityScores` but never calls `broadcastDuelState`, so clients never receive score updates. Additionally, the leaderboard reads `d.ffaKills` not `d.communityScores`.

4. Observer killfeed is blocked by `presentConfirmedDamage` at line 86: `if (event.attackerPlayerId != ctx.localPlayerId) return false;` — NPC attackers never match the local player ID.

### Attempted Fix 1

Time:
`2026-09-09T00:00:00Z`

Change:
Added NPC killfeed entry in `mpProcessDamageConfirmedEventPacket` victim path (lines 1167-1201). Added `NpcKillfeedLog::log` calls throughout the NPC kill data path for diagnostics.

Result:
Killfeed entry was generated but weapon still showed "unknown" due to hardcoded mapping. NPC name fell back to "NPC-{id}" (acceptable but not ideal).

### Attempted Fix 2

Time:
`2026-09-09T00:00:00Z`

Change:
Added NPC damage tracking (`lastNpcDamageSourceId`, `lastNpcDamageTick`) to `ServerPlayer`. When a player kills another player, checked if victim was recently damaged by an NPC and re-attributed the kill.

Result:
Kill attribution worked for self-damage-after-NPC-hit scenarios. But the fundamental weapon display issue remained.

### Attempted Fix 3

Time:
`2026-09-10T00:00:00Z`

Change:
Added kill queue promotion in the mapOnly path of `serverGamemodeTick`. Added `broadcastDuelState` call after FFA scoring.

Result:
FFA scoring now increments and broadcasts to clients. But the leaderboard still showed "unknown" weapons because the `DamageConfirmedEventPacket` had no dynamic weapon ID field.

### Attempted Fix 4

Time:
`2026-09-10T00:00:00Z`

Change:
1. Added `uint16_t weaponDefNetworkId` field to `DamageConfirmedEventPacket`
2. Added `weaponDisplayName()` helper to `network-weapons.h/cpp` — resolves any registered weapon's display name via the dynamic system
3. Server populates `weaponDefNetworkId` from `weaponDefNetworkIdFor(weaponDefId)` in `queueServerDamageConfirmedEvent`
4. Client uses `weaponDisplayName(event->weaponDefNetworkId)` for killfeed weapon resolution
5. Added observer killfeed path for NPC kills in `mpProcessDamageConfirmedEventPacket`
6. Added custom NPC name support via `DuelConfig.npcNames`
7. Synced `d.communityScores` to `d.ffaKills` in mapOnly FFA scoring
8. Deprecated `NetworkWeaponType` enum with code comments
9. Enabled `DBG(Network,...)` output in `config/debuglogger.json`

Result:
Pending human review.

### Corrected Code

File:
`src/network/packets.h:792`

```cpp
uint16_t weaponDefNetworkId = 0;  // Dynamic weapon ID — use weaponIdForDefNetworkId() to resolve
```

File:
`src/network/network-weapons.cpp` (new function)

```cpp
const char* weaponDisplayName(uint16_t defNetworkId)
{
    if (defNetworkId == 0) return "unknown";
    const std::string* id = weaponIdForDefNetworkId(defNetworkId);
    if (!id) return "unknown";
    if (const WeaponDefinition* def = WeaponRegistry::instance().get(*id))
        if (!def->displayName.empty())
            return def->displayName.c_str();
    return id->c_str();
}
```

### Fix

The fix uses the existing dynamic `weaponDefNetworkId` registration system (already used by `AttackRequestPacket`) instead of the hardcoded `NetworkWeaponType` enum. The server registers all weapons at startup via `registerWeaponDefNetworkId`, and the `DamageConfirmedEventPacket` now carries the dynamic ID. The client resolves it to a display name via `weaponDisplayName()`.

### Proof

Human review:

Pending — requires in-game testing with NPC kills in FFA mode.

Automated proof:

Build succeeded. DBG output enabled to `logs/<MM-DD-YYYY>/Network_log_<HHMMSS>.txt`.

### Solution

`PENDING human review as of 2026-09-10T00:00:00Z`
