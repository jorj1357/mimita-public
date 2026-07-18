#include "network/server.h"
#include "network/packets.h"
#include "network/network-weapons.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-types.h"
#include "combat/weapon-fire.h"
#include "debug/debug-log.h"

namespace MimitaNet {

// ── Idempotent attack result cache ────────────────────────────────────
// Keyed by (playerId, spawnGeneration, requestId).
// A retry returns the exact same result without mutating gameplay state.
struct CachedAttackResult {
    uint32_t playerId = 0;
    uint32_t spawnGeneration = 0;
    uint32_t requestId = 0;
    bool accepted = false;
    uint8_t reason = 0;
    uint32_t projectileId = 0;
    int32_t magazineAmmo = 0;
    int32_t reserveAmmo = 0;
    uint64_t nextAllowedFireTick = 0;
    uint32_t stateRevision = 0;
    bool valid = false;
};
static CachedAttackResult s_attackCache[64];
static uint8_t s_attackCacheNext = 0;

static void cacheAttackResult(const ServerPlayer& player, const AttackRequestPacket* req,
    bool accepted, uint8_t reason, uint32_t projectileId,
    int32_t magazineAmmo, int32_t reserveAmmo,
    uint64_t nextAllowedFireTick, uint32_t stateRevision)
{
    auto& slot = s_attackCache[s_attackCacheNext];
    slot.playerId = player.id;
    slot.spawnGeneration = req->spawnGeneration;
    slot.requestId = req->requestId;
    slot.accepted = accepted;
    slot.reason = reason;
    slot.projectileId = projectileId;
    slot.magazineAmmo = magazineAmmo;
    slot.reserveAmmo = reserveAmmo;
    slot.nextAllowedFireTick = nextAllowedFireTick;
    slot.stateRevision = stateRevision;
    slot.valid = true;
    s_attackCacheNext = (s_attackCacheNext + 1) % 64;
}

static bool lookupCachedAttackResult(const ServerPlayer& player, const AttackRequestPacket* req,
    AttackResultPacket& out)
{
    for (int i = 0; i < 64; ++i)
    {
        const auto& c = s_attackCache[i];
        if (!c.valid) continue;
        if (c.playerId == player.id && c.spawnGeneration == req->spawnGeneration && c.requestId == req->requestId)
        {
            out.header.type = PACKET_ATTACK_RESULT;
            out.header.playerId = player.id;
            out.requestId = req->requestId;
            out.spawnGeneration = req->spawnGeneration;
            out.accepted = c.accepted ? 1 : 0;
            out.reason = c.reason;
            out.projectileId = c.projectileId;
            out.magazineAmmo = c.magazineAmmo;
            out.reserveAmmo = c.reserveAmmo;
            out.nextAllowedFireTick = c.nextAllowedFireTick;
            out.stateRevision = c.stateRevision;
            return true;
        }
    }
    return false;
}

// ── Helper: send generic AttackResult to the requesting player ───────
static void sendAttackResult(SOCKET sock, const ServerPlayer& player,
    const AttackRequestPacket* req, uint32_t tick,
    bool accepted, uint8_t reason, uint32_t projectileId,
    int32_t magazineAmmo, int32_t reserveAmmo,
    uint64_t nextAllowedFireTick, uint32_t stateRevision)
{
    AttackResultPacket result{};
    result.header.type = PACKET_ATTACK_RESULT;
    result.header.tick = tick;
    result.header.playerId = player.id;
    result.requestId = req->requestId;
    result.spawnGeneration = req->spawnGeneration;
    result.accepted = accepted ? 1 : 0;
    result.reason = reason;
    result.projectileId = projectileId;
    result.magazineAmmo = magazineAmmo;
    result.reserveAmmo = reserveAmmo;
    result.nextAllowedFireTick = nextAllowedFireTick;
    result.stateRevision = stateRevision;
    result.serverTick = tick;

    // Cache the result so retries are idempotent
    cacheAttackResult(player, req, accepted, reason, projectileId,
        magazineAmmo, reserveAmmo, nextAllowedFireTick, stateRevision);

    serverSendToPlayer(sock, player, &result, sizeof(result));
}

// ── Generic attack validation + dispatch ─────────────────────────────
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

    Debug::log(Debug::Category::Weapons, "[ATTACK REQUEST RX] playerId=%u requestId=%u spawnGen=%u weaponDefNetId=%u slot=%d\n",
               shooter.id, req->requestId, req->spawnGeneration, req->weaponDefNetworkId, req->equippedSlot);

    // ── Idempotency check — cache lookup first, before any validation ──
    {
        AttackResultPacket cached;
        if (lookupCachedAttackResult(shooter, req, cached))
        {
            Debug::log(Debug::Category::Weapons, "[ATTACK CACHE HIT] playerId=%u requestId=%u spawnGen=%u accepted=%d reason=%d\n",
                       shooter.id, req->requestId, req->spawnGeneration, (int)cached.accepted, (int)cached.reason);
            serverSendToPlayer(sock, shooter, &cached, sizeof(cached));
            return;
        }
    }

    // ── Resolve weapon definition ──────────────────────────────────────
    const std::string* wepId = weaponIdForDefNetworkId(req->weaponDefNetworkId);
    if (!wepId)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK] playerId=%u requestId=%u unknown weaponDefNetworkId=%u\n",
                   shooter.id, req->requestId, req->weaponDefNetworkId);
        sendAttackResult(sock, shooter, req, tick, false, 7, 0, -1, -1, 0, 0);
        return;
    }

    const WeaponDefinition* def = WeaponRegistry::instance().get(*wepId);
    if (!def)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK] playerId=%u requestId=%u weaponId=%s not in registry\n",
                   shooter.id, req->requestId, wepId->c_str());
        sendAttackResult(sock, shooter, req, tick, false, 7, 0, -1, -1, 0, 0);
        return;
    }

    // ── Validate spawn generation ──────────────────────────────────────
    if (req->spawnGeneration == 0 || req->spawnGeneration != shooter.spawnGeneration)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK REJECT] playerId=%u requestId=%u stale spawnGeneration req=%u cur=%u\n",
                   shooter.id, req->requestId, req->spawnGeneration, shooter.spawnGeneration);
        sendAttackResult(sock, shooter, req, tick, false, 8, 0, -1, -1, 0, 0);
        return;
    }

    // ── Validate equipped slot ─────────────────────────────────────────
    if (req->equippedSlot != shooter.equippedSlot)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK REJECT] playerId=%u requestId=%u slot mismatch req=%d cur=%d\n",
                   shooter.id, req->requestId, req->equippedSlot, shooter.equippedSlot);
        sendAttackResult(sock, shooter, req, tick, false, 4, 0, -1, -1, 0, 0);
        return;
    }

    // ── Dead check ────────────────────────────────────────────────────
    if (shooter.dead)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK REJECT] playerId=%u requestId=%u dead\n",
                   shooter.id, req->requestId);
        sendAttackResult(sock, shooter, req, tick, false, 2, 0, -1, -1, 0, 0);
        return;
    }

    // ── Resolve authoritative weapon runtime ──────────────────────────
    auto rtIt = shooter.weaponRuntimes.find(*wepId);
    if (rtIt == shooter.weaponRuntimes.end() || !rtIt->second.initialized)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK REJECT] playerId=%u requestId=%u weapon runtime not initialized\n",
                   shooter.id, req->requestId);
        sendAttackResult(sock, shooter, req, tick, false, 7, 0, -1, -1, 0, 0);
        return;
    }

    ServerPlayer::ServerWeaponRuntime& rt = rtIt->second;

    // ── Ammo check ────────────────────────────────────────────────────
    if (rt.magazineAmmo <= 0)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK REJECT] playerId=%u requestId=%u out of ammo ammo=%d\n",
                   shooter.id, req->requestId, rt.magazineAmmo);
        sendAttackResult(sock, shooter, req, tick, false, 3, 0,
                         rt.magazineAmmo, rt.reserveAmmo,
                         rt.nextAllowedFireTick, rt.stateRevision);
        return;
    }

    // ── Cooldown check (tick-based) ────────────────────────────────────
    constexpr uint64_t COOLDOWN_GRACE_TICKS = 2;
    if (tick + COOLDOWN_GRACE_TICKS < rt.nextAllowedFireTick)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK REJECT] playerId=%u requestId=%u cooldown tick=%u nextAllowed=%llu\n",
                   shooter.id, req->requestId, tick, (unsigned long long)rt.nextAllowedFireTick);
        sendAttackResult(sock, shooter, req, tick, false, 1, 0,
                         rt.magazineAmmo, rt.reserveAmmo,
                         rt.nextAllowedFireTick, rt.stateRevision);
        return;
    }

    // ── Idempotency check using (playerId, spawnGeneration, requestId) ─
    // For now, delegate to existing projectile handler which has its own cache.
    // The weapon runtime state is read AFTER the cache check to avoid
    // mutating state for duplicate requests.

    // ── Dispatch by fire type ─────────────────────────────────────────
    // Currently only projectile weapons are supported via the migration bridge.
    // Hitscan and melee return explicit unsupported.
    if (def->behaviorType == WeaponBehaviorType::Hitscan)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK] playerId=%u requestId=%u hitscan not yet migrated\n",
                   shooter.id, req->requestId);
        sendAttackResult(sock, shooter, req, tick, false, 9, 0,
                         rt.magazineAmmo, rt.reserveAmmo,
                         rt.nextAllowedFireTick, rt.stateRevision);
        return;
    }

    if (def->behaviorType == WeaponBehaviorType::Swordsword ||
        def->behaviorType == WeaponBehaviorType::Hafs)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK] playerId=%u requestId=%u melee not yet migrated\n",
                   shooter.id, req->requestId);
        sendAttackResult(sock, shooter, req, tick, false, 9, 0,
                         rt.magazineAmmo, rt.reserveAmmo,
                         rt.nextAllowedFireTick, rt.stateRevision);
        return;
    }

    // ── Projectile weapons (grenade, rocket) — delegate to existing handler ──
    if (def->behaviorType == WeaponBehaviorType::RocketLauncher ||
        def->behaviorType == WeaponBehaviorType::GrenadeLauncher)
    {
        glm::vec3 origin(req->aimOriginX, req->aimOriginY, req->aimOriginZ);
        glm::vec3 direction(req->aimDirX, req->aimDirY, req->aimDirZ);
        direction = glm::normalize(direction);

        uint8_t netWeapon = networkWeaponTypeForDefinition(*def);
        if (netWeapon == NETWORK_WEAPON_NONE)
        {
            sendAttackResult(sock, shooter, req, tick, false, 7, 0,
                             rt.magazineAmmo, rt.reserveAmmo,
                             rt.nextAllowedFireTick, rt.stateRevision);
            return;
        }

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

        // The projectile handler already sends its own result (ProjectileFireResultPacket).
        // The client may handle both the old result type and eventually the new one.
        // For now, we rely on the old result path for client reconciliation.
        return;
    }

    // ── Unknown behavior type ─────────────────────────────────────────
    Debug::log(Debug::Category::Weapons, "[ATTACK REJECT] playerId=%u requestId=%u unknown behaviorType=%d\n",
               shooter.id, req->requestId, (int)def->behaviorType);
    sendAttackResult(sock, shooter, req, tick, false, 9, 0,
                     rt.magazineAmmo, rt.reserveAmmo,
                     rt.nextAllowedFireTick, rt.stateRevision);
}

} // namespace MimitaNet
