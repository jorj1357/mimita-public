#include "network/server.h"
#include "network/packets.h"
#include "network/network-weapons.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-types.h"
#include "combat/weapon-fire.h"
#include "debug/debug-log.h"

namespace MimitaNet {

// ── Generic attack validation + dispatch ─────────────────────────────
// Called from the server packet drain loop. Handles all weapon types:
// Hitscan, Projectile, Melee — dispatched by WeaponDefinition.
void handleAttackRequest(
    SOCKET sock,
    const sockaddr_in& from,
    const char* buffer,
    int bytes,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerProjectile>& projectiles,
    uint32_t& nextProjectileId,
    const HeadlessWorld& world,
    uint32_t tick,
    uint64_t& totalPacketsOut)
{
    if (bytes < (int)sizeof(AttackRequestPacket))
        return;

    const AttackRequestPacket* req = reinterpret_cast<const AttackRequestPacket*>(buffer);

    auto shooterIt = players.find(req->header.playerId);
    if (shooterIt == players.end() || !sameAddress(shooterIt->second.addr, from))
        return;

    ServerPlayer& shooter = shooterIt->second;

    // ── Resolve weapon definition ──────────────────────────────────────
    const std::string* wepId = weaponIdForDefNetworkId(req->weaponDefNetworkId);
    if (!wepId)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK] playerId=%u requestId=%u unknown weaponDefNetworkId=%u\n",
                   shooter.id, req->requestId, req->weaponDefNetworkId);
        return;
    }

    const WeaponDefinition* def = WeaponRegistry::instance().get(*wepId);
    if (!def)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK] playerId=%u requestId=%u weaponId=%s not in registry\n",
                   shooter.id, req->requestId, wepId->c_str());
        return;
    }

    // ── Validate ownership and spawn generation ────────────────────────
    if (req->spawnGeneration != shooter.spawnGeneration)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK] playerId=%u requestId=%u stale spawnGeneration req=%u cur=%u\n",
                   shooter.id, req->requestId, req->spawnGeneration, shooter.spawnGeneration);
        return;
    }

    // ── Validate equipped slot ─────────────────────────────────────────
    if (req->equippedSlot != shooter.equippedSlot)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK] playerId=%u requestId=%u slot mismatch req=%d cur=%d\n",
                   shooter.id, req->requestId, req->equippedSlot, shooter.equippedSlot);
        // Could buffer if basedOnInputSequence is recent; for now reject
        return;
    }

    // ── Dead check ────────────────────────────────────────────────────
    if (shooter.dead)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK] playerId=%u requestId=%u dead\n",
                   shooter.id, req->requestId);
        return;
    }

    // ── Dispatch by fire type ─────────────────────────────────────────
    Debug::log(Debug::Category::Weapons, "[ATTACK] playerId=%u requestId=%u weaponId=%s fireType=%d\n",
               shooter.id, req->requestId, wepId->c_str(), (int)def->behaviorType);

    // For now, delegate to existing handlers based on behaviorType.
    // This is the migration bridge — future versions will use a unified FireType.
    if (def->behaviorType == WeaponBehaviorType::Hitscan)
    {
        // TODO: route to generic hitscan handler
    }
    else if (def->behaviorType == WeaponBehaviorType::RocketLauncher ||
             def->behaviorType == WeaponBehaviorType::GrenadeLauncher)
    {
        // Route to existing projectile handler (future: generic Projectile type)
        glm::vec3 origin(req->aimOriginX, req->aimOriginY, req->aimOriginZ);
        glm::vec3 direction(req->aimDirX, req->aimDirY, req->aimDirZ);
        direction = glm::normalize(direction);

        uint8_t netWeapon = networkWeaponTypeForDefinition(*def);
        if (netWeapon == NETWORK_WEAPON_NONE)
            return;

        // Rebuild a ProjectileFireRequestPacket from the generic request
        ProjectileFireRequestPacket pfr{};
        pfr.header.type = PACKET_PROJECTILE_FIRE_REQUEST;
        pfr.header.tick = req->header.tick;
        pfr.header.playerId = req->header.playerId;
        pfr.fireSerial = req->requestId;
        pfr.weapon = netWeapon;
        pfr.originX = req->aimOriginX;
        pfr.originY = req->aimOriginY;
        pfr.originZ = req->aimOriginZ;
        pfr.dirX = req->aimDirX;
        pfr.dirY = req->aimDirY;
        pfr.dirZ = req->aimDirZ;

        handleProjectileFireRequest(sock, from,
            reinterpret_cast<const char*>(&pfr), sizeof(pfr),
            players, projectiles, nextProjectileId, world, tick, totalPacketsOut);
    }
    else if (def->behaviorType == WeaponBehaviorType::Swordsword ||
             def->behaviorType == WeaponBehaviorType::Hafs)
    {
        // TODO: route to generic melee handler
    }
}

} // namespace MimitaNet
