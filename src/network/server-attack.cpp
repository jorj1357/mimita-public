// 07 21 2026, 21 30
/* purpose
* Handles authoritative generic AttackRequest validation and execution dispatch.
* Routes hitscan, physical-contact, and projectile weapon definitions through one request path.
* Owns attack idempotency, ammo/cooldown mutation, and server damage decisions for migrated weapons.
* Does NOT trust client target, damage, death, health, or projectile hit outcomes.
* Does NOT implement packet polling, client prediction, rendering, or audio presentation.
* Does NOT own projectile simulation internals, render correction, or legacy direct fire packets.
*/

#include "network/server.h"
#include "network/packets.h"
#include "network/network-weapons.h"
#include "network/disagreement-visuals.h"
#include "combat/weapon-execution.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-types.h"
#include "combat/weapon-fire.h"
#include "combat/weapon-runtime.h"
#include "debug/debug-log.h"

#include <cmath>
#include <vector>

namespace MimitaNet {
namespace {

static bool finiteVec3(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

static glm::vec3 normalizedOrZero(const glm::vec3& v)
{
    if (!finiteVec3(v) || glm::length(v) <= 0.0001f)
        return glm::vec3(0.0f);
    return glm::normalize(v);
}

static uint64_t cooldownTickFor(const WeaponDefinition& def, uint32_t tick)
{
    if (def.fireDelay <= 0.0f)
        return tick;
    return (uint64_t)tick + (uint64_t)std::ceil(def.fireDelay * SERVER_TICK_RATE);
}

static void startServerSwordAttack(ServerPlayer& attacker,
                                   const WeaponDefinition& def,
                                   uint8_t attackVariant)
{
    const bool lunge = attackVariant == 2;
    const float slashWindup = WeaponExecution::paramOr(def, "slashWindupTime", 0.08f);
    const float slashActive = WeaponExecution::paramOr(def, "slashActiveTime", 0.15f);
    const float slashRecover = WeaponExecution::paramOr(def, "slashRecoverTime", 0.10f);
    const float lungeWindup = WeaponExecution::paramOr(def, "lungeWindupTime", 0.10f);
    const float lungeActive = WeaponExecution::paramOr(def, "lungeActiveTime", 0.20f);
    const float lungeRecover = WeaponExecution::paramOr(def, "lungeRecoverTime", 0.12f);
    const float fallbackCooldown = lunge
        ? lungeWindup + lungeActive + lungeRecover + 0.05f
        : slashWindup + slashActive + slashRecover + 0.05f;
    const float cooldown = lunge
        ? WeaponExecution::paramOr(def, "lungeCooldown", fallbackCooldown)
        : WeaponExecution::paramOr(def, "slashCooldown", fallbackCooldown);

    attacker.swordswordState = SwordswordState{};
    attacker.swordswordState.state = lunge
        ? SwordswordState::AttackState::LungeWindup
        : SwordswordState::AttackState::SlashWindup;
    attacker.swordswordState.stateTimer = 0.0f;
    attacker.swordswordState.animTimer = 0.0f;
    attacker.meleeCooldownTimer = std::max(0.0f, cooldown);
    attacker.hasLastPhysicalWeaponShape = false;
}

} // namespace

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
    uint16_t weaponDefNetworkId = 0;
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
    slot.weaponDefNetworkId = req->weaponDefNetworkId;
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
            out.weaponDefNetworkId = c.weaponDefNetworkId;
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
    result.weaponDefNetworkId = req->weaponDefNetworkId;

    // Cache the result so retries are idempotent
    cacheAttackResult(player, req, accepted, reason, projectileId,
        magazineAmmo, reserveAmmo, nextAllowedFireTick, stateRevision);

    serverSendToPlayer(sock, player, &result, sizeof(result));
}

// ── Broadcast a disagreement when an attack is rejected ──────────────
static void emitAttackRejection(
    SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    uint32_t tick,
    uint64_t& totalPacketsOut,
    DisagreementRetransmitState* retransmitState,
    const ServerPlayer& shooter,
    uint32_t requestId,
    const char* description)
{
    if (!retransmitState)
        return;
    if (!shouldEmitDisagreement(retransmitState->rateLimit, tick,
                                disagreementMinTicks()))
        return;
    const glm::vec3 at = shooter.pos + glm::vec3(0.0f, 0.0f, 1.2f);
    sendDisagreementToAll(sock, players, DISAGREEMENT_INVALID_STATE,
                          retransmitState->nextEventId++, requestId,
                          shooter.id, 0u, at, glm::vec3(0.0f), description,
                          tick, totalPacketsOut, retransmitState);
}

// ── Generic attack validation + dispatch ─────────────────────────────
void handleAttackRequest(
    SOCKET sock,
    const sockaddr_in& from,
    const char* buffer,
    int bytes,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs,
    std::unordered_map<uint32_t, ServerProjectile>& projectiles,
    uint32_t& nextProjectileId,
    const HeadlessWorld& world,
    uint32_t tick,
    uint64_t& totalPacketsOut,
    DisagreementRetransmitState* retransmitState)
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

    // ── Spawn state check — not Active yet ────────────────────────────
    if (shooter.spawnState != ServerPlayer::Active)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK REJECT] playerId=%u requestId=%u spawnState not Active — awaiting spawn ack\n",
                   shooter.id, req->requestId);
        emitAttackRejection(sock, players, tick, totalPacketsOut, retransmitState,
                            shooter, req->requestId, "NOT SPAWNED");
        sendAttackResult(sock, shooter, req, tick, false, 8, 0, -1, -1, 0, 0);
        return;
    }

    // ── Resolve weapon definition ──────────────────────────────────────
    const std::string* wepId = weaponIdForDefNetworkId(req->weaponDefNetworkId);
    if (!wepId)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK] playerId=%u requestId=%u unknown weaponDefNetworkId=%u\n",
                   shooter.id, req->requestId, req->weaponDefNetworkId);
        emitAttackRejection(sock, players, tick, totalPacketsOut, retransmitState,
                            shooter, req->requestId, "UNKNOWN WEAPON");
        sendAttackResult(sock, shooter, req, tick, false, 7, 0, -1, -1, 0, 0);
        return;
    }

    const WeaponDefinition* def = WeaponRegistry::instance().get(*wepId);
    if (!def)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK] playerId=%u requestId=%u weaponId=%s not in registry\n",
                   shooter.id, req->requestId, wepId->c_str());
        emitAttackRejection(sock, players, tick, totalPacketsOut, retransmitState,
                            shooter, req->requestId, "UNKNOWN WEAPON");
        sendAttackResult(sock, shooter, req, tick, false, 7, 0, -1, -1, 0, 0);
        return;
    }

    // ── Validate spawn generation ──────────────────────────────────────
    if (req->spawnGeneration == 0 || req->spawnGeneration != shooter.spawnGeneration)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK REJECT] playerId=%u requestId=%u stale spawnGeneration req=%u cur=%u\n",
                   shooter.id, req->requestId, req->spawnGeneration, shooter.spawnGeneration);
        emitAttackRejection(sock, players, tick, totalPacketsOut, retransmitState,
                            shooter, req->requestId, "STALE SPAWN GENERATION");
        sendAttackResult(sock, shooter, req, tick, false, 8, 0, -1, -1, 0, 0);
        return;
    }

    // ── Dead check ────────────────────────────────────────────────────
    if (shooter.dead)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK REJECT] playerId=%u requestId=%u dead\n",
                   shooter.id, req->requestId);
        emitAttackRejection(sock, players, tick, totalPacketsOut, retransmitState,
                            shooter, req->requestId, "PLAYER DEAD");
        sendAttackResult(sock, shooter, req, tick, false, 2, 0, -1, -1, 0, 0);
        return;
    }

    // ── Resolve authoritative weapon runtime ──────────────────────────
    // If the client is using a weapon the server hasn't granted yet (e.g. a
    // restricted weapon from a custom loadout), initialize its runtime on
    // demand instead of rejecting. Damage, ammo, and cooldown stay
    // server-authoritative; this only stops refusing a weapon the player has.
    auto rtIt = shooter.weaponRuntimes.find(*wepId);
    if (rtIt == shooter.weaponRuntimes.end() || !rtIt->second.initialized)
    {
        ServerPlayer::ServerWeaponRuntime fresh;
        fresh.magazineAmmo = def->magazineSize;
        fresh.reserveAmmo = initialReserveAmmoForDefinition(*def);
        fresh.nextAllowedFireTick = 0;
        fresh.reloading = false;
        fresh.reloadCompleteTick = 0;
        fresh.stateRevision = 0;
        fresh.initialized = true;
        shooter.weaponRuntimes[*wepId] = fresh;

        bool alreadyOwned = false;
        for (const std::string& owned : shooter.ownedWeaponIds)
        {
            if (owned == *wepId)
            {
                alreadyOwned = true;
                break;
            }
        }
        if (!alreadyOwned)
            shooter.ownedWeaponIds.push_back(*wepId);

        rtIt = shooter.weaponRuntimes.find(*wepId);
        Debug::log(Debug::Category::Weapons,
                   "[ATTACK RUNTIME LAZY-GRANT] playerId=%u weapon=%s granted on first use\n",
                   shooter.id, wepId->c_str());
    }

    ServerPlayer::ServerWeaponRuntime& rt = rtIt->second;

    // ── Validate or reconcile equipped slot ───────────────────────────
    if (req->equippedSlot != def->slot)
    {
        Debug::log(Debug::Category::Weapons,
                   "[ATTACK REJECT] playerId=%u requestId=%u request slot does not match weapon req=%d def=%d\n",
                   shooter.id, req->requestId, req->equippedSlot, def->slot);
        emitAttackRejection(sock, players, tick, totalPacketsOut, retransmitState,
                            shooter, req->requestId, "SLOT MISMATCH");
        sendAttackResult(sock, shooter, req, tick, false, 4, 0, -1, -1, 0, 0);
        return;
    }
    if (req->equippedSlot != shooter.equippedSlot)
    {
        Debug::log(Debug::Category::Weapons,
                   "[ATTACK EQUIP RECONCILE] playerId=%u requestId=%u oldSlot=%d newSlot=%d weapon=%s\n",
                   shooter.id, req->requestId, shooter.equippedSlot,
                   req->equippedSlot, def->id.c_str());
        shooter.equippedSlot = req->equippedSlot;
    }

    const bool consumesAmmo = def->executionType != WeaponExecutionType::PhysicalContact &&
        def->magazineSize > 0;

    // ── Ammo check ────────────────────────────────────────────────────
    if (consumesAmmo && rt.magazineAmmo <= 0)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK REJECT] playerId=%u requestId=%u out of ammo ammo=%d\n",
                   shooter.id, req->requestId, rt.magazineAmmo);
        emitAttackRejection(sock, players, tick, totalPacketsOut, retransmitState,
                            shooter, req->requestId, "NO AMMO");
        sendAttackResult(sock, shooter, req, tick, false, 3, 0,
                         rt.magazineAmmo, rt.reserveAmmo,
                         rt.nextAllowedFireTick, rt.stateRevision);
        return;
    }

    // ── Cooldown check (tick-based) ────────────────────────────────────
    constexpr uint64_t COOLDOWN_GRACE_TICKS = 2;
    if (def->executionType != WeaponExecutionType::PhysicalContact &&
        tick + COOLDOWN_GRACE_TICKS < rt.nextAllowedFireTick)
    {
        Debug::log(Debug::Category::Weapons, "[ATTACK REJECT] playerId=%u requestId=%u cooldown tick=%u nextAllowed=%llu\n",
                   shooter.id, req->requestId, tick, (unsigned long long)rt.nextAllowedFireTick);
        emitAttackRejection(sock, players, tick, totalPacketsOut, retransmitState,
                            shooter, req->requestId, "COOLDOWN");
        sendAttackResult(sock, shooter, req, tick, false, 1, 0,
                         rt.magazineAmmo, rt.reserveAmmo,
                         rt.nextAllowedFireTick, rt.stateRevision);
        return;
    }

    // ── Idempotency check using (playerId, spawnGeneration, requestId) ─
    // For now, delegate to existing projectile handler which has its own cache.
    // The weapon runtime state is read AFTER the cache check to avoid
    // mutating state for duplicate requests.

    // ── Dispatch by execution family ──────────────────────────────────
    if (def->executionType == WeaponExecutionType::Hitscan)
    {
        const glm::vec3 reqOrigin(req->muzzlePosX, req->muzzlePosY, req->muzzlePosZ);
        const glm::vec3 fallbackOrigin(req->aimOriginX, req->aimOriginY, req->aimOriginZ);
        glm::vec3 origin = finiteVec3(reqOrigin) ? reqOrigin : fallbackOrigin;
        glm::vec3 direction = normalizedOrZero(
            glm::vec3(req->aimDirX, req->aimDirY, req->aimDirZ));
        if (!finiteVec3(origin) || glm::length(direction) <= 0.0001f ||
            glm::length(origin - shooter.pos) > 12.0f)
        {
            Debug::log(Debug::Category::Weapons,
                "[ATTACK REJECT] playerId=%u requestId=%u invalid hitscan geometry\n",
                shooter.id, req->requestId);
            emitAttackRejection(sock, players, tick, totalPacketsOut, retransmitState,
                                shooter, req->requestId, "INVALID GEOMETRY");
            sendAttackResult(sock, shooter, req, tick, false, 5, 0,
                             rt.magazineAmmo, rt.reserveAmmo,
                             rt.nextAllowedFireTick, rt.stateRevision);
            return;
        }

        const float maxRange = WeaponExecution::paramOr(*def, "range",
            WeaponExecution::paramOr(*def, "maxRange", WeaponExecution::DEFAULT_HITSCAN_RANGE));
        glm::vec3 worldHit;
        glm::vec3 worldNormal;
        float worldBlockDistance = maxRange;
        if (serverRaycastWorld(origin, direction, maxRange, world, worldHit, worldNormal))
            worldBlockDistance = glm::length(worldHit - origin);

        // Lag compensation: validate the trace at the tick the attacker was
        // actually looking at (their fire-time snapshot minus the remote
        // interpolation buffer), so shots land where the attacker saw them.
        const uint32_t rewindTick =
            estimateServerRewindTick(shooter, req->clientSimulationTick, tick);

        std::vector<WeaponExecution::PlayerTarget> targets;
        targets.reserve(players.size() + npcs.size());
        for (const auto& targetEntry : players)
        {
            const ServerPlayer& target = targetEntry.second;
            if (target.id == shooter.id || target.dead ||
                target.spawnState != ServerPlayer::Active)
                continue;
            WeaponExecution::PlayerTarget targetDesc;
            targetDesc.playerId = target.id;
            targetDesc.spawnGeneration = target.spawnGeneration;
            glm::vec3 rewoundPos;
            if (getPositionAtTick(target, rewindTick, rewoundPos))
                targetDesc.position = rewoundPos;
            else
                targetDesc.position = target.pos;
            targetDesc.radius = PLAYER_RADIUS;
            targetDesc.height = PLAYER_HEIGHT;
            targetDesc.dead = target.dead;
            targets.push_back(targetDesc);
        }
        // Also include NPCs as trace targets, validated at the pose the
        // attacker actually saw. The client fires at the NPC it renders
        // (rewound to their fire-time snapshot tick), not at the NPC's
        // current authoritative position, so rewind the NPC like players.
        for (const auto& npcEntry : npcs)
        {
            const ServerNpc& npc = npcEntry.second;
            if (npc.health <= 0)
                continue;
            glm::vec3 tracePos = npc.pos;
            glm::vec3 rewoundPos;
            if (getNpcPositionAtTick(npc, rewindTick, rewoundPos))
                tracePos = rewoundPos;
            WeaponExecution::PlayerTarget targetDesc;
            targetDesc.playerId = npc.entityId; // use entityId as pseudo-playerId
            targetDesc.spawnGeneration = 0;
            targetDesc.position = tracePos;
            targetDesc.radius = PLAYER_RADIUS;
            targetDesc.height = PLAYER_HEIGHT;
            targetDesc.dead = false;
            targets.push_back(targetDesc);

            // Debug: surface the visible-pose vs current-pose transform
            // mismatch that caused moving-target misses. One aggregate line
            // per second, never per-frame or per-shot spam.
            static uint64_t lastNpcRewindLog = 0;
            const uint64_t nowRewind = nowMs();
            if (nowRewind - lastNpcRewindLog >= 1000)
            {
                lastNpcRewindLog = nowRewind;
                const float drift = glm::length(tracePos - npc.pos);
                if (drift > 0.05f)
                    Debug::warn(Debug::Category::NpcCombat,
                        "[NPC REWIND] npc=%u rewindTick=%u currentTick=%u "
                        "rewound=(%.2f,%.2f,%.2f) current=(%.2f,%.2f,%.2f) drift=%.2f\n",
                        npc.entityId, rewindTick, tick,
                        tracePos.x, tracePos.y, tracePos.z,
                        npc.pos.x, npc.pos.y, npc.pos.z, drift);
            }
        }

        WeaponExecution::HitscanTraceConfig traceConfig;
        traceConfig.maxRange = maxRange;
        traceConfig.damage = std::max(1.0f, def->damage);
        traceConfig.headshotMultiplier = std::max(1.0f, def->headshotMultiplier);
        traceConfig.pelletCount = std::max(1, def->pelletCount);
        traceConfig.spreadDegrees = def->spread;
        traceConfig.deterministicSeed = req->deterministicSeed;
        traceConfig.worldBlockDistance = worldBlockDistance;
        traceConfig.knockbackPerDamage = WeaponExecution::paramOr(*def, "knockbackPerDamage", 0.08f);
        WeaponExecution::HitscanTraceResult trace =
            WeaponExecution::traceHitscan(*def, origin, direction, traceConfig, targets);

        rt.magazineAmmo--;
        rt.nextAllowedFireTick = cooldownTickFor(*def, tick);
        rt.reloading = false;
        rt.stateRevision++;

        const uint8_t netWeapon = networkWeaponTypeForDefinition(*def);
        for (const WeaponExecution::HitscanDamageAggregate& aggregate : trace.aggregates)
        {
            // Check if target is an NPC (NPC entityIds start at 1000+)
            auto npcIt = npcs.find(aggregate.targetPlayerId);
            if (npcIt != npcs.end())
            {
                ServerNpc& npcTarget = npcIt->second;
                if (npcTarget.health > 0)
                {
                    npcTarget.health -= aggregate.damage;
                    npcTarget.knockbackImpulse += aggregate.knockback;
                }
                const bool killed = npcTarget.health <= 0;
                if (killed)
                {
                    npcTarget.health = 0;
                    printf("%s [SERVER NPC KILL] shooter=%u npcId=%u name=\"%s\"\n",
                           serverTimestamp(), shooter.id,
                           npcTarget.entityId, npcTarget.name.c_str());
                    // Award kill credit to shooter and heal to full
                    auto attacker = players.find(shooter.id);
                    if (attacker != players.end())
                    {
                        attacker->second.kills += 1;
                        attacker->second.health = 100;
                    }
                    // Do NOT erase: syncServerNpcDamageToNpc marks the real NPC
                    // dead and respawnServerNpc re-admits it after the delay.
                }
                broadcastNpcDamageEvent(
                    sock, players, tick, totalPacketsOut, shooter.id, npcTarget,
                    aggregate.damage, killed,
                    origin, aggregate.hitPosition, direction, aggregate.hitNormal,
                    netWeapon);
                continue;
            }
            auto targetIt = players.find(aggregate.targetPlayerId);
            if (targetIt == players.end() ||
                targetIt->second.spawnGeneration != aggregate.targetSpawnGeneration)
                continue;
            ServerPlayer& target = targetIt->second;
            ServerDamageResult dmgResult = applyServerDamage(
                players, target, shooter.id, aggregate.damage,
                aggregate.knockback, ServerDamageSource::Hitscan);
            queueServerDamageConfirmedEvent(
                sock, players, tick, totalPacketsOut, shooter.id, target,
                aggregate.damage, dmgResult,
                aggregate.hitPosition, aggregate.hitNormal, aggregate.knockback,
                ServerDamageSource::Hitscan, netWeapon, req->requestId);
        }

        Debug::log(Debug::Category::Weapons,
            "[ATTACK HITSCAN ACCEPT] playerId=%u requestId=%u weapon=%s pellets=%d targets=%zu ammo=%d/%d stateRev=%u\n",
            shooter.id, req->requestId, def->id.c_str(), trace.pelletCount,
            trace.aggregates.size(), rt.magazineAmmo, rt.reserveAmmo,
            rt.stateRevision);

        Debug::logThrottled(Debug::Category::Weapons, "attack-rewind", 1.0,
            "[ATTACK REWIND] playerId=%u requestId=%u fireSnapshotTick=%u "
            "serverTick=%u rewindTick=%u delayTicks=%u\n",
            shooter.id, req->requestId, req->clientSimulationTick,
            tick, rewindTick, REWIND_INTERP_DELAY_TICKS);

        // ── Broadcast shot visuals to all players ──────────────────────
        if (trace.pelletCount <= 1)
        {
            // Single-pellet (revolver): broadcast ShotEventPacket
            glm::vec3 hitPos = origin + direction * traceConfig.maxRange;
            glm::vec3 hitNml = -direction;
            uint32_t hitTarget = 0;
            if (!trace.aggregates.empty())
            {
                hitPos = trace.aggregates[0].hitPosition;
                hitNml = trace.aggregates[0].hitNormal;
                hitTarget = trace.aggregates[0].targetPlayerId;
            }
            else if (glm::length(worldHit - origin) < traceConfig.maxRange)
            {
                hitPos = worldHit;
                hitNml = worldNormal;
            }

            uint16_t effectFlags = SHOT_EFFECT_MUZZLE | SHOT_EFFECT_TRACER |
                SHOT_EFFECT_SHOOT_SOUND | SHOT_EFFECT_WEAPON_TRIGGER;
            uint8_t impactType = SHOT_IMPACT_NONE;
            if (hitTarget != 0)
            {
                impactType = SHOT_IMPACT_ENTITY;
                effectFlags |= SHOT_EFFECT_ENTITY_IMPACT | SHOT_EFFECT_BLOOD | SHOT_EFFECT_HIT_SOUND;
            }
            else if (glm::length(hitPos - origin) < traceConfig.maxRange - 0.1f)
            {
                impactType = SHOT_IMPACT_WORLD;
                effectFlags |= SHOT_EFFECT_WORLD_IMPACT | SHOT_EFFECT_DEBRIS | SHOT_EFFECT_HIT_SOUND;
            }

            ShotEventPacket shotEvent{};
            shotEvent.header.type = PACKET_SHOT_EVENT;
            shotEvent.header.tick = tick;
            shotEvent.header.playerId = shooter.id;
            shotEvent.shotSerial = req->requestId;
            shotEvent.clientTimeMs = req->clientSimulationTick;
            shotEvent.shooterPlayerId = shooter.id;
            shotEvent.targetPlayerId = hitTarget;
            shotEvent.weapon = netWeapon;
            shotEvent.impactType = impactType;
            shotEvent.effectFlags = effectFlags;
            shotEvent.originX = origin.x;
            shotEvent.originY = origin.y;
            shotEvent.originZ = origin.z;
            shotEvent.hitX = hitPos.x;
            shotEvent.hitY = hitPos.y;
            shotEvent.hitZ = hitPos.z;
            shotEvent.dirX = direction.x;
            shotEvent.dirY = direction.y;
            shotEvent.dirZ = direction.z;
            shotEvent.normalX = hitNml.x;
            shotEvent.normalY = hitNml.y;
            shotEvent.normalZ = hitNml.z;

            for (const auto& pe : players)
            {
                if (pe.second.transport)
                    pe.second.transport->send(&shotEvent, sizeof(shotEvent));
                else
                    sendto(sock, (const char*)&shotEvent, sizeof(shotEvent), 0,
                           (sockaddr*)&pe.second.addr,
                           sizeof(pe.second.addr));
                ++totalPacketsOut;
            }

            Debug::log(Debug::Category::Weapons,
                       "[WEAPON_EVENT_BROADCAST] shooter=%u requestId=%u weapon=%s "
                       "impact=%u target=%u recipients=%zu\n",
                       shooter.id, req->requestId, def->id.c_str(),
                       (unsigned)impactType, hitTarget, players.size());
        }
        else
        {
            // Multi-pellet (shotgun/AA12): broadcast PelletBlastEventPacket
            glm::vec3 pelletDirs[MAX_PELLETS_PER_BLAST]{};
            int pelletCount = WeaponExecution::buildPelletDirections(
                *def, direction, req->deterministicSeed,
                pelletDirs, MAX_PELLETS_PER_BLAST);

            PelletBlastEventPacket blastEvent{};
            blastEvent.header.type = PACKET_PELLET_BLAST_EVENT;
            blastEvent.header.tick = tick;
            blastEvent.shooterPlayerId = shooter.id;
            blastEvent.shotSerial = req->requestId;
            blastEvent.clientTimeMs = req->clientSimulationTick;
            blastEvent.spreadSeed = req->deterministicSeed;
            blastEvent.originX = origin.x;
            blastEvent.originY = origin.y;
            blastEvent.originZ = origin.z;
            blastEvent.baseDirX = direction.x;
            blastEvent.baseDirY = direction.y;
            blastEvent.baseDirZ = direction.z;
            blastEvent.weapon = netWeapon;
            blastEvent.pelletCount = (uint8_t)std::min(pelletCount, (int)MAX_NETWORK_PELLETS);

            for (int i = 0; i < pelletCount && i < MAX_NETWORK_PELLETS; ++i)
            {
                NetworkPelletResult& r = blastEvent.pellets[i];
                r.pelletIndex = (uint8_t)i;

                if (trace.pellets[i].hit && trace.pellets[i].targetPlayerId != 0)
                {
                    r.hitX = trace.pellets[i].hitPosition.x;
                    r.hitY = trace.pellets[i].hitPosition.y;
                    r.hitZ = trace.pellets[i].hitPosition.z;
                    r.normalX = trace.pellets[i].hitNormal.x;
                    r.normalY = trace.pellets[i].hitNormal.y;
                    r.normalZ = trace.pellets[i].hitNormal.z;
                    r.targetPlayerId = trace.pellets[i].targetPlayerId;
                    r.impactType = PELLET_IMPACT_PLAYER;
                    r.bodyPart = trace.pellets[i].headshot ? 0 : 1;
                }
                else
                {
                    glm::vec3 pelletEnd = origin + pelletDirs[i] * traceConfig.worldBlockDistance;
                    r.hitX = pelletEnd.x;
                    r.hitY = pelletEnd.y;
                    r.hitZ = pelletEnd.z;
                    r.normalX = -pelletDirs[i].x;
                    r.normalY = -pelletDirs[i].y;
                    r.normalZ = -pelletDirs[i].z;
                    r.targetPlayerId = 0;
                    r.impactType = PELLET_IMPACT_WORLD;
                }
            }

            blastEvent.targetCount = 0;
            for (const auto& agg : trace.aggregates)
            {
                if (blastEvent.targetCount >= MAX_PELLET_BLAST_TARGETS)
                    break;
                PelletBlastTargetResult& t = blastEvent.targets[blastEvent.targetCount++];
                t.targetPlayerId = agg.targetPlayerId;
                t.totalDamage = (int16_t)agg.damage;
                t.knockX = 0;
                t.knockY = 0;
                t.knockZ = 0;
                t.pelletsHit = (uint8_t)agg.pelletHits;
                auto vit = players.find(agg.targetPlayerId);
                t.healthAfter = vit != players.end() ? (int16_t)vit->second.health : 0;
                t.killed = 0;
            }

            for (const auto& pe : players)
            {
                if (pe.second.transport)
                    pe.second.transport->send(&blastEvent, sizeof(blastEvent));
                else
                    sendto(sock, (const char*)&blastEvent, sizeof(blastEvent), 0,
                           (sockaddr*)&pe.second.addr,
                           sizeof(pe.second.addr));
                ++totalPacketsOut;
            }
        }

        sendAttackResult(sock, shooter, req, tick, true, 0, 0,
                         rt.magazineAmmo, rt.reserveAmmo,
                         rt.nextAllowedFireTick, rt.stateRevision);
        return;
    }

    if (def->executionType == WeaponExecutionType::PhysicalContact)
    {
        if (def->behaviorType == WeaponBehaviorType::Swordsword)
        {
            if (shooter.meleeCooldownTimer > 0.0f)
            {
                sendAttackResult(sock, shooter, req, tick, false, 1, 0,
                                 rt.magazineAmmo, rt.reserveAmmo,
                                 rt.nextAllowedFireTick, rt.stateRevision);
                return;
            }
            shooter.lastMeleeAttackSerial = req->requestId;
            startServerSwordAttack(shooter, *def, req->attackVariant);
        }
        rt.stateRevision++;
        Debug::log(Debug::Category::Weapons,
            "[ATTACK PHYSICAL ACCEPT] playerId=%u requestId=%u weapon=%s variant=%u stateRev=%u\n",
            shooter.id, req->requestId, def->id.c_str(),
            (unsigned)req->attackVariant, rt.stateRevision);
        sendAttackResult(sock, shooter, req, tick, true, 0, 0,
                         rt.magazineAmmo, rt.reserveAmmo,
                         rt.nextAllowedFireTick, rt.stateRevision);
        return;
    }

    // ── Projectile weapons (grenade, rocket) ─────────────────────────
    if (def->executionType == WeaponExecutionType::Projectile &&
        (def->behaviorType == WeaponBehaviorType::RocketLauncher ||
         def->behaviorType == WeaponBehaviorType::GrenadeLauncher))
    {
        glm::vec3 origin(req->muzzlePosX, req->muzzlePosY, req->muzzlePosZ);
        if (!finiteVec3(origin))
            origin = glm::vec3(req->aimOriginX, req->aimOriginY, req->aimOriginZ);
        glm::vec3 direction(req->aimDirX, req->aimDirY, req->aimDirZ);
        direction = normalizedOrZero(direction);
        if (glm::length(direction) <= 0.0001f)
        {
            sendAttackResult(sock, shooter, req, tick, false, 5, 0,
                             rt.magazineAmmo, rt.reserveAmmo,
                             rt.nextAllowedFireTick, rt.stateRevision);
            return;
        }

        ServerProjectileAttackResult projectileResult =
            handleGenericProjectileAttack(
                sock, players, npcs, projectiles, nextProjectileId,
                shooter, *def, req->requestId, origin, direction,
                tick, totalPacketsOut);
        sendAttackResult(sock, shooter, req, tick,
                         projectileResult.accepted,
                         projectileResult.reason,
                         projectileResult.projectileId,
                         projectileResult.magazineAmmo,
                         projectileResult.reserveAmmo,
                         projectileResult.nextAllowedFireTick,
                         projectileResult.stateRevision);
        Debug::log(Debug::Category::Weapons,
            "[ATTACK PROJECTILE %s] playerId=%u requestId=%u weapon=%s projectileId=%u ammo=%d/%d stateRev=%u\n",
            projectileResult.accepted ? "ACCEPT" : "REJECT",
            shooter.id, req->requestId, def->id.c_str(),
            projectileResult.projectileId,
            projectileResult.magazineAmmo,
            projectileResult.reserveAmmo,
            projectileResult.stateRevision);
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
