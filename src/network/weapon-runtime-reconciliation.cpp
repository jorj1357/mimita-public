#include "weapon-runtime-reconciliation.h"

#include <cstdio>
#include <algorithm>

#include "combat/weapon-registry.h"
#include "combat/weapon-runtime.h"
#include "combat/weapon-types.h"
#include "entities/player.h"
#include "network/multiplayer-context.h"
#include "network/net_common.h"
#include "network/network-weapons.h"
#include "network/simulation-constants.h"
#include "debug/debug-log.h"

namespace MimitaNet {

bool reconcileAuthoritativeWeaponRuntime(
    MultiplayerContext& ctx,
    Player& player,
    uint16_t weaponDefNetworkId,
    int magazineAmmo,
    int reserveAmmo,
    uint64_t nextAllowedFireTick,
    bool reloading,
    uint64_t reloadCompleteTick,
    uint32_t stateRevision,
    uint32_t spawnGeneration,
    const char* source,
    bool applyAmmo)
{
    // ── 1. Validate weapon ID before any mutation ─────────────────────
    const std::string* weaponId = weaponIdForDefNetworkId(weaponDefNetworkId);
    if (!weaponId)
    {
        printf("[RECONCILE] weaponDefNetworkId=%u unknown — no runtime created\n",
               weaponDefNetworkId);
        return false;
    }

    const WeaponDefinition* def = WeaponRegistry::instance().get(*weaponId);
    if (!def)
    {
        printf("[RECONCILE] weaponDefNetworkId=%u id=%s not in registry — no runtime created\n",
               weaponDefNetworkId, weaponId->c_str());
        return false;
    }

    // ── 2. Find or initialize runtime ─────────────────────────────────
    auto rtIt = player.weaponRuntimes.find(*weaponId);
    if (rtIt == player.weaponRuntimes.end())
    {
        WeaponRuntime rt;
        WeaponRuntimeHelper::initRuntime(rt, *def);
        player.weaponRuntimes[*weaponId] = rt;
        rtIt = player.weaponRuntimes.find(*weaponId);
    }

    WeaponRuntime& rt = rtIt->second;

    // ── 3. Spawn generation ordering ──────────────────────────────────
    // If the incoming generation is zero, it carries no lifecycle signal
    // and must not overwrite a known nonzero generation.
    if (spawnGeneration == 0 && rt.authoritativeSpawnGeneration != 0)
    {
        printf("[RECONCILE] spawnGen=0 cannot overwrite gen %u — ignoring field\n",
               rt.authoritativeSpawnGeneration);
        // Continue with existing spawn generation for staleness checks.
    }

    if (spawnGeneration != 0 && spawnGeneration < rt.authoritativeSpawnGeneration)
    {
        printf("[RECONCILE] stale spawnGen %u < %u\n",
               spawnGeneration, rt.authoritativeSpawnGeneration);
        return false;
    }

    // New spawn generation resets revision baseline.
    if (spawnGeneration > rt.authoritativeSpawnGeneration)
    {
        if (spawnGeneration != 0)
            rt.authoritativeSpawnGeneration = spawnGeneration;
        rt.authoritativeStateRevision = 0;
    }

    // ── 4. State revision ordering ────────────────────────────────────
    if (stateRevision < rt.authoritativeStateRevision)
    {
        printf("[RECONCILE] stale revision %u < %u\n",
               stateRevision, rt.authoritativeStateRevision);
        return false;
    }

    if (stateRevision == rt.authoritativeStateRevision)
    {
        printf("[RECONCILE] duplicate revision %u — idempotent\n", stateRevision);
        return true;
    }

    // ── 5. Ammo validation ────────────────────────────────────────────
    if (magazineAmmo < 0 || reserveAmmo < 0)
    {
        printf("[RECONCILE] negative ammo mag=%d res=%d\n", magazineAmmo, reserveAmmo);
        return false;
    }

    // ── 6. Apply authoritative values ─────────────────────────────────
    // Ammo is client-authoritative: when applyAmmo is false the server's
    // counter must not overwrite the client's predicted clip/reload. The
    // cooldown and reload-timer sync below still applies either way.
    if (applyAmmo)
    {
        rt.currentAmmo = magazineAmmo;
        rt.reserveAmmo = reserveAmmo;
    }
    rt.isReloading = reloading;

    // ── 7. Tick → seconds conversion (shared GAMEPLAY_SIMULATION_HZ) ──
    // When no valid server tick exists, preserve the predicted timer.
    // isReloading=false always zeros the timer (authoritative override).
    uint32_t estimatedServerTick = ctx.latestServerTick;
    bool hasServerTick = estimatedServerTick > 0;

    if (hasServerTick && nextAllowedFireTick > 0)
    {
        uint64_t rem = nextAllowedFireTick > estimatedServerTick
            ? nextAllowedFireTick - estimatedServerTick : 0;
        rt.fireCooldown = std::max(0.0f, (float)rem / static_cast<float>(GAMEPLAY_SIMULATION_HZ));
    }

    if (reloading && reloadCompleteTick > 0)
    {
        if (hasServerTick)
        {
            uint64_t rem = reloadCompleteTick > estimatedServerTick
                ? reloadCompleteTick - estimatedServerTick : 0;
            rt.reloadTimer = std::max(0.0f, (float)rem / static_cast<float>(GAMEPLAY_SIMULATION_HZ));
        }
    }
    else if (!reloading)
    {
        rt.reloadTimer = 0.0f;
    }

    // ── 8. Record metadata ────────────────────────────────────────────
    rt.authoritativeStateRevision = stateRevision;
    if (spawnGeneration != 0)
        rt.authoritativeSpawnGeneration = spawnGeneration;

    printf("[RECONCILE] weapon=%s mag=%d res=%d reload=%d cd=%.2fs rev=%u spawnGen=%u src=%s\n",
           weaponId->c_str(), rt.currentAmmo, rt.reserveAmmo,
           (int)rt.isReloading, rt.fireCooldown,
           stateRevision, rt.authoritativeSpawnGeneration, source);

    return true;
}

void sendReloadRequestForWeapon(MultiplayerContext& ctx,
                                const std::string& weaponId)
{
    if (!ctx.active || ctx.localPlayerId == 0)
        return;
    const uint16_t netId = weaponDefNetworkIdFor(weaponId);
    if (netId == 0)
        return;

    ReloadRequestPacket req{};
    req.header.type = PACKET_RELOAD_REQUEST;
    req.header.tick = ctx.tick;
    req.header.playerId = ctx.localPlayerId;
    req.requestId = ctx.nextActionRequestId++;
    if (ctx.nextActionRequestId == 0) ctx.nextActionRequestId = 1;
    req.spawnGeneration = ctx.lastKnownSpawnGeneration;
    req.weaponDefNetworkId = netId;
    mpSendPacket(ctx, &req, sizeof(req));
    ctx.pendingReloadRequests[req.requestId] = {
        req.requestId, req.spawnGeneration, netId, nowMs()
    };
    Debug::log(Debug::Category::Weapons,
               "[RELOAD REQUEST SEND] playerId=%u requestId=%u weapon=%s pending=%zu\n",
               ctx.localPlayerId, req.requestId, weaponId.c_str(),
               ctx.pendingReloadRequests.size());
}

} // namespace MimitaNet
