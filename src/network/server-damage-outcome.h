// 09 22 2026
/* purpose
* Shared authoritative damage-outcome owner for melee and projectile hot
* behaviors: per-victim damage policy, damage application, DamageConfirmed /
* NPC-damage events, and kill recording. This is the melee/projectile analogue
* of serverResolveHitscanOutcome, so hot melee/explosions do not silently drop
* the authoritative consequences.
* Does NOT compute the trace/splash or own packets.
*/
#pragma once

#include <cstdint>
#include <unordered_map>

#include "network/server.h"
#include "combat/weapon-types.h"

namespace MimitaNet {

struct ServerOutcomeVictim {
    std::uint64_t entity = 0;      // ECS entity id (player or NPC)
    std::uint32_t spawnGeneration = 0;
    std::int32_t damage = 0;
    float knockback[3] = {0.0f, 0.0f, 0.0f};
    float hitPosition[3] = {0.0f, 0.0f, 0.0f};
    float hitNormal[3] = {0.0f, 0.0f, 1.0f};
};

void serverResolveDamageOutcome(
    SOCKET sock,
    std::unordered_map<std::uint32_t, ServerPlayer>& players,
    std::unordered_map<std::uint32_t, ServerNpc>& npcs,
    const ServerPlayer* attacker,
    const WeaponDefinition* def,
    std::uint32_t sourceKind,       // GameDamageSource numeric
    std::uint32_t causeSerial,
    std::uint32_t projectileId,
    const ServerOutcomeVictim* victims,
    std::uint32_t victimCount,
    std::uint32_t tick,
    std::uint64_t& totalPacketsOut);

} // namespace MimitaNet
