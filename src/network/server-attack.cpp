// 08 31 2026, 17 14
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
#include "network/server-duel.h"
#include "network/disagreement-visuals.h"
#include "combat/weapon-execution.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-types.h"
#include "combat/weapon-fire.h"
#include "combat/weapon-runtime.h"
#include "config/networking-config.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"

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

// Reconstruct body-part hitboxes from the standard player template at the
// rewound pose (position + yaw). The template stores default-pose offsets from
// the body root at yaw 0, so rotating by the target's yaw places each part
// exactly where the model is. Same shape the client renders — no capsule.
static void fillTargetBodyParts(WeaponExecution::PlayerTarget& targetDesc,
                                const glm::vec3& pos, float yaw)
{
    if (const auto* tpl = standardPlayerBodyTemplate())
    {
        const float c = std::cos(yaw);
        const float s = std::sin(yaw);
        targetDesc.bodyParts.reserve(tpl->size());
        for (const auto& t : *tpl)
        {
            const glm::vec3 off(t.offset.x * c - t.offset.y * s,
                                t.offset.x * s + t.offset.y * c,
                                t.offset.z);
            WeaponExecution::PlayerTarget::BodyPartBox box;
            box.center = pos + off;
            box.half = t.half;
            box.bodyPart = (WeaponExecution::HitBodyPart)t.bodyPart;
            targetDesc.bodyParts.push_back(box);
        }
    }
}

// Does the claimed hit point land inside any reconstructed body-part box
// (expanded by tolerance)? Fills claimPart from the actual part if the client
// didn't specify one.
static bool claimedHitInBodyParts(const glm::vec3& claimedHit,
                                  const glm::vec3& pos, float yaw,
                                  float tolerance, uint8_t& claimPart)
{
    if (const auto* tpl = standardPlayerBodyTemplate())
    {
        const float c = std::cos(yaw);
        const float s = std::sin(yaw);
        for (const auto& t : *tpl)
        {
            const glm::vec3 off(t.offset.x * c - t.offset.y * s,
                                t.offset.x * s + t.offset.y * c,
                                t.offset.z);
            const glm::vec3 ctr = pos + off;
            const glm::vec3 half = t.half + glm::vec3(tolerance);
            if (claimedHit.x >= ctr.x - half.x && claimedHit.x <= ctr.x + half.x &&
                claimedHit.y >= ctr.y - half.y && claimedHit.y <= ctr.y + half.y &&
                claimedHit.z >= ctr.z - half.z && claimedHit.z <= ctr.z + half.z)
            {
                if (claimPart == 0 || claimPart == 2)
                    claimPart = t.bodyPart == 0 ? 2
                        : (t.bodyPart == 1 ? 1 : 3);
                return true;
            }
        }
    }
    return false;
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
    uint8_t hitVerdict = 0;
    bool valid = false;
};
static CachedAttackResult s_attackCache[64];
static uint8_t s_attackCacheNext = 0;

static void cacheAttackResult(const ServerPlayer& player, const AttackRequestPacket* req,
    bool accepted, uint8_t reason, uint32_t projectileId,
    int32_t magazineAmmo, int32_t reserveAmmo,
    uint64_t nextAllowedFireTick, uint32_t stateRevision,
    uint8_t hitVerdict)
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
    slot.hitVerdict = hitVerdict;
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
            out.hitVerdict = c.hitVerdict;
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
    uint64_t nextAllowedFireTick, uint32_t stateRevision,
    uint8_t hitVerdict = 0)
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
    result.hitVerdict = hitVerdict;

    // Cache the result so retries are idempotent
    cacheAttackResult(player, req, accepted, reason, projectileId,
        magazineAmmo, reserveAmmo, nextAllowedFireTick, stateRevision, hitVerdict);

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

    // ── Per-tick incoming packet flood guard ───────────────────────
    if (++shooter.attackPktsThisTick > ServerPlayer::MAX_ATTACK_PKTS_PER_TICK)
        return;

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

    if (!serverCommunityWeaponAllowed(def->id))
    {
        Debug::log(Debug::Category::Weapons,
            "[COMMUNITY WEAPON REJECT] playerId=%u requestId=%u weapon=%s reason=weapon-set\n",
            shooter.id, req->requestId, def->id.c_str());
        emitAttackRejection(sock, players, tick, totalPacketsOut, retransmitState,
                            shooter, req->requestId, "WEAPON SET DISABLED");
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

    // ── Ammo is client-authoritative ──────────────────────────────────
    // The client owns its clip (decrement + local reload). The server no
    // longer rejects shots for ammo — it only rate-limits via cooldown.
    // The server-side magazine counter stays informational (never below 0).
    const bool consumesAmmo = def->executionType != WeaponExecutionType::PhysicalContact &&
        def->magazineSize > 0;

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

    // ── Per-tick shot rate limit ───────────────────────────────────
    if (def->executionType == WeaponExecutionType::Hitscan &&
        shooter.shotsThisTick >= ServerPlayer::MAX_SHOTS_PER_TICK)
    {
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
        // Allow the muzzle position to be ahead of the server's record of the
        // shooter by the distance the player could have traveled during their
        // round-trip latency.  Without this, shots are falsely rejected on
        // high-latency connections (e.g. badconn 8) because the client fires
        // from a position the server has not accepted yet.
        const float pingAllowance = (float)shooter.pingMs / 1000.0f * 200.0f;
        const float originTolerance = 12.0f + pingAllowance;
        if (!finiteVec3(origin) || glm::length(direction) <= 0.0001f ||
            glm::length(origin - shooter.pos) > originTolerance)
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
        const float beamThickness = std::max(0.0f, def->beamThickness);
        const float beamWorldThickness = std::max(0.0f, def->beamWorldThickness);
        glm::vec3 worldHit;
        glm::vec3 worldNormal;
        float worldBlockDistance = maxRange;
        if (beamWorldThickness > 0.0f)
        {
            // Thick world beam: swept-sphere world trace so the beam is blocked
            // by a wall at the same distance the client's swept beam would be.
            float sweptDist = 0.0f;
            if (serverSweptSphereWorld(origin, direction, maxRange, beamWorldThickness,
                                       world, sweptDist, worldNormal))
            {
                worldBlockDistance = sweptDist;
                worldHit = origin + direction * sweptDist;
            }
        }
        else
        {
            // Thin world rays (default): precise aim, so a shotgun pellet
            // pattern at the aim direction is never clipped by wall edges.
            if (serverRaycastWorld(origin, direction, maxRange, world, worldHit, worldNormal))
                worldBlockDistance = glm::length(worldHit - origin);
        }

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
            float rewoundYaw = target.yaw;
            if (getPlayerPoseAtTick(target, rewindTick, rewoundPos, rewoundYaw))
                targetDesc.position = rewoundPos;
            else
                targetDesc.position = target.pos;
            targetDesc.radius = PLAYER_RADIUS;
            targetDesc.height = PLAYER_HEIGHT;
            targetDesc.dead = target.dead;
            // Reconstruct the victim's real body-part hitboxes (head/torso/
            // arms/legs) at the rewound pose + rewound yaw from the standard
            // body template — never an invisible capsule.
            fillTargetBodyParts(targetDesc, targetDesc.position, rewoundYaw);
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
            float rewoundYaw = npc.yaw;
            if (getNpcPoseAtTick(npc, rewindTick, rewoundPos, rewoundYaw))
                tracePos = rewoundPos;
            WeaponExecution::PlayerTarget targetDesc;
            targetDesc.playerId = npc.entityId; // use entityId as pseudo-playerId
            targetDesc.spawnGeneration = 0;
            targetDesc.position = tracePos;
            targetDesc.radius = PLAYER_RADIUS;
            targetDesc.height = PLAYER_HEIGHT;
            targetDesc.dead = false;
            // Reconstruct the NPC's body-part hitboxes at the rewound pose +
            // rewound yaw (the facing the attacker actually saw) — no capsule.
            fillTargetBodyParts(targetDesc, targetDesc.position, rewoundYaw);
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
                const float yawDrift = glm::degrees(std::fabs(
                    std::fmod(std::fabs(rewoundYaw - npc.yaw), 6.2831853f)));
                if (drift > 0.05f || yawDrift > 5.0f)
                    Debug::warn(Debug::Category::NpcCombat,
                        "[NPC REWIND] npc=%u rewindTick=%u currentTick=%u "
                        "rewound=(%.2f,%.2f,%.2f) current=(%.2f,%.2f,%.2f) drift=%.2f "
                        "rewoundYaw=%.1f currentYaw=%.1f yawDrift=%.1fdeg\n",
                        npc.entityId, rewindTick, tick,
                        tracePos.x, tracePos.y, tracePos.z,
                        npc.pos.x, npc.pos.y, npc.pos.z, drift,
                        glm::degrees(rewoundYaw), glm::degrees(npc.yaw), yawDrift);
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
        traceConfig.knockbackPerDamage = def->victimKnockbackPerDamage;
        traceConfig.distanceFalloffStart = WeaponExecution::paramOr(*def, "distanceFalloffStart", 0.0f);
        traceConfig.minDamageFraction = WeaponExecution::paramOr(*def, "minDamageFraction", 0.05f);
        traceConfig.falloffExponent = WeaponExecution::paramOr(*def, "falloffExponent", 1.0f);
        traceConfig.limbDamageMultiplier = WeaponExecution::paramOr(*def, "limbDamageMultiplier", 0.75f);
        traceConfig.beamThickness = beamThickness;
        traceConfig.beamWorldThickness = beamWorldThickness;
        WeaponExecution::HitscanTraceResult trace =
            WeaponExecution::traceHitscan(*def, origin, direction, traceConfig, targets);

        // ── Client hit-claim acceptance ("shoot what I saw") ─────────────
        // The re-trace above is authoritative, but under jitter / rewind-pose
        // mismatch a shot that visually connected on the shooter's rendered
        // body can miss the rewound pose. When the client claimed a hit on a
        // specific target, accept it if the claimed hit point lies inside that
        // target's rewound collision volume (body parts for NPCs, capsule +
        // tolerance for players) and is not wall-occluded — so what the shooter
        // saw is what takes damage.
        if (req->claimedTargetId != 0)
        {
            const glm::vec3 claimedHit(req->claimedHitX, req->claimedHitY, req->claimedHitZ);
            const glm::vec3 claimedDir = claimedHit - origin;
            const float claimedDist = glm::length(claimedDir);
            bool alreadyConfirmed = false;
            for (const auto& agg : trace.aggregates)
            {
                if (agg.targetPlayerId == req->claimedTargetId)
                {
                    alreadyConfirmed = true;
                    break;
                }
            }
            if (!alreadyConfirmed && claimedDist > 0.001f &&
                claimedDist <= maxRange)
            {
                // World occlusion: reuse the main trace's worldBlockDistance
                // instead of a second full raycast. The main trace already
                // found the nearest world hit along this direction.
                bool occluded = false;
                if (worldBlockDistance < claimedDist - 0.1f)
                    occluded = true;

                if (!occluded)
                {
                    // Claim acceptance tolerance: base rewind tolerance plus a
                    // lag allowance so a hit that connects on the target's
                    // RENDERED body registers even when motion-filter lag puts
                    // the server's rewind pose slightly ahead of what the
                    // shooter saw ("shoot what I see").
                    const NetworkingConfigData& netCfg =
                        NetworkingConfig::instance().data();
                    const float tolerance =
                        std::max(0.0f, netCfg.remotePlayers.rewindHitTolerance) +
                        std::max(0.0f, netCfg.remotePlayers.claimLagAllowance);
                    bool claimAccepted = false;
                    uint8_t claimPart = req->claimedBodyPart;
                    glm::vec3 rewoundTargetPos{0.0f}; // for the reject diagnostic
                    uint32_t claimedSpawnGen = 0;
                    auto npcClaimIt = npcs.find(req->claimedTargetId);
                    if (npcClaimIt != npcs.end())
                    {
                        const ServerNpc& npc = npcClaimIt->second;
                        if (npc.health > 0)
                        {
                            // Validate the claimed hit against the NPC's real
                            // body-part hitboxes reconstructed at the rewound
                            // pose + rewound yaw — never a capsule.
                            glm::vec3 rewoundPos = npc.pos;
                            float rewoundYaw = npc.yaw;
                            getNpcPoseAtTick(npc, rewindTick, rewoundPos, rewoundYaw);
                            rewoundTargetPos = rewoundPos;
                            if (claimedHitInBodyParts(claimedHit, rewoundPos, rewoundYaw,
                                                      tolerance, claimPart))
                                claimAccepted = true;
                        }
                    }
                    else
                    {
                        auto playerClaimIt = players.find(req->claimedTargetId);
                        if (playerClaimIt != players.end() &&
                            !playerClaimIt->second.dead &&
                            playerClaimIt->second.spawnState == ServerPlayer::Active)
                        {
                            claimedSpawnGen = playerClaimIt->second.spawnGeneration;
                            glm::vec3 rewoundPos = playerClaimIt->second.pos;
                            float rewoundYaw = playerClaimIt->second.yaw;
                            getPlayerPoseAtTick(playerClaimIt->second, rewindTick,
                                                rewoundPos, rewoundYaw);
                            rewoundTargetPos = rewoundPos;
                            // Validate the claimed hit against the victim's
                            // reconstructed body-part hitboxes (same template as
                            // the re-trace) — never a capsule.
                            if (claimedHitInBodyParts(claimedHit, rewoundPos, rewoundYaw,
                                                      tolerance, claimPart))
                                claimAccepted = true;
                        }
                    }

                    if (claimAccepted)
                    {
                        // Damage for the claimed hit using the claimed body
                        // part + range falloff, mirroring the client model.
                        const std::string claimBodyPart =
                            claimPart == 1 ? "head" : (claimPart == 3 ? "leg" : "torso");
                        const float dmgF = (float)WeaponExecution::computeHitscanDamage(
                            *def, claimBodyPart, claimedDist, 1.0f);
                        WeaponExecution::HitscanDamageAggregate agg;
                        agg.targetPlayerId = req->claimedTargetId;
                        agg.targetSpawnGeneration = claimedSpawnGen;
                        agg.damage = std::max(1, (int)std::round(dmgF));
                        agg.pelletHits = 1;
                        agg.hitPosition = claimedHit;
                        agg.hitNormal = glm::vec3(0.0f, 0.0f, 1.0f);
                        agg.knockback = claimedDir / claimedDist *
                            (dmgF * traceConfig.knockbackPerDamage);
                        agg.headshot = claimPart == 1;
                        trace.aggregates.push_back(agg);
                        Debug::log(Debug::Category::Weapons,
                            "[ATTACK CLAIM ACCEPT] playerId=%u requestId=%u "
                            "claimedTarget=%u part=%u dist=%.2f damage=%d\n",
                            shooter.id, req->requestId, req->claimedTargetId,
                            claimPart, claimedDist, agg.damage);
                    }
                    else
                    {
                        const float offsetFromRewound = glm::length(
                            claimedHit - rewoundTargetPos);
                        Debug::log(Debug::Category::Weapons,
                            "[ATTACK CLAIM REJECT] playerId=%u requestId=%u "
                            "claimedTarget=%u claimedHit=(%.2f,%.2f,%.2f) "
                            "rewoundTarget=(%.2f,%.2f,%.2f) offset=%.2f tolerance=%.2f "
                            "rewindTick=%u dist=%.2f reason=not-in-volume\n",
                            shooter.id, req->requestId, req->claimedTargetId,
                            claimedHit.x, claimedHit.y, claimedHit.z,
                            rewoundTargetPos.x, rewoundTargetPos.y, rewoundTargetPos.z,
                            offsetFromRewound, tolerance,
                            rewindTick, claimedDist);
                    }
                }
                else
                {
                    Debug::log(Debug::Category::Weapons,
                        "[ATTACK CLAIM REJECT] playerId=%u requestId=%u "
                        "claimedTarget=%u reason=world-occluded\n",
                        shooter.id, req->requestId, req->claimedTargetId);
                }
            }
        }

        if (rt.magazineAmmo > 0)
            rt.magazineAmmo--;
        rt.nextAllowedFireTick = cooldownTickFor(*def, tick);
        rt.reloading = false;
        rt.stateRevision++;
        shooter.shotsThisTick++;

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
                        attacker->second.health = serverMaxHp();
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
            shotEvent.eventId = nextReliableGameplayEventId();
            shotEvent.shotSerial = req->requestId;
            shotEvent.clientTimeMs = req->clientSimulationTick;
            shotEvent.shooterPlayerId = shooter.id;
            shotEvent.targetPlayerId = hitTarget;
            shotEvent.lastServerTick = tick;
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
            // Full-beam endpoint so the tracer can continue past the first hit.
            const glm::vec3 beamEnd = origin + direction * traceConfig.maxRange;
            shotEvent.beamEndX = beamEnd.x;
            shotEvent.beamEndY = beamEnd.y;
            shotEvent.beamEndZ = beamEnd.z;
            // Carry the real damage/health so the victim's damage number shows
            // the actual amount (the reliable DamageConfirmed event is the
            // source of truth for HP; this is presentation).
            if (hitTarget != 0 && !trace.aggregates.empty())
            {
                const auto& agg = trace.aggregates[0];
                shotEvent.damage = agg.damage;
                shotEvent.damageConfirmed = 1;
                auto targetIt = players.find(agg.targetPlayerId);
                if (targetIt != players.end())
                {
                    shotEvent.targetHealth = targetIt->second.health;
                    shotEvent.killed = targetIt->second.health <= 0 ? 1 : 0;
                }
            }

            // Unreliable broadcast: shot visuals are fire-and-forget. The
            // reliable DamageConfirmed event below is the source of truth for
            // HP; a dropped visual just means one fewer tracer, not lost damage.
            for (const auto& pe : players)
            {
                if (pe.second.transport)
                    pe.second.transport->send(&shotEvent, sizeof(shotEvent));
                else
                    sendto(sock, (const char*)&shotEvent, sizeof(shotEvent), 0,
                           (sockaddr*)&pe.second.addr, sizeof(pe.second.addr));
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
            blastEvent.eventId = nextReliableGameplayEventId();
            blastEvent.shooterPlayerId = shooter.id;
            blastEvent.shotSerial = req->requestId;
            blastEvent.clientTimeMs = req->clientSimulationTick;
            blastEvent.lastServerTick = tick;
            blastEvent.spreadSeed = req->deterministicSeed;
            blastEvent.originX = origin.x;
            blastEvent.originY = origin.y;
            blastEvent.originZ = origin.z;
            blastEvent.baseDirX = direction.x;
            blastEvent.baseDirY = direction.y;
            blastEvent.baseDirZ = direction.z;
            blastEvent.weapon = netWeapon;
            blastEvent.pelletCount = (uint8_t)std::min(pelletCount, (int)MAX_NETWORK_PELLETS);
            blastEvent.maxRange = traceConfig.maxRange;
            {
                const glm::vec3 beamEnd = origin + direction * traceConfig.maxRange;
                blastEvent.beamEndX = beamEnd.x;
                blastEvent.beamEndY = beamEnd.y;
                blastEvent.beamEndZ = beamEnd.z;
            }

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
                t.targetSpawnGeneration = vit != players.end() ? vit->second.spawnGeneration : 0;
            }

            // Unreliable broadcast: pellet visuals are fire-and-forget. The
            // reliable DamageConfirmed events below are the source of truth for
            // HP; a dropped visual just means fewer tracers, not lost damage.
            for (const auto& pe : players)
            {
                if (pe.second.transport)
                    pe.second.transport->send(&blastEvent, sizeof(blastEvent));
                else
                    sendto(sock, (const char*)&blastEvent, sizeof(blastEvent), 0,
                           (sockaddr*)&pe.second.addr, sizeof(pe.second.addr));
                ++totalPacketsOut;
            }
        }

        uint8_t hitVerdict = HIT_VERDICT_MISS;
        if (!trace.aggregates.empty())
        {
            bool hitClaimed = false;
            for (const auto& agg : trace.aggregates)
            {
                if (req->claimedTargetId != 0 && agg.targetPlayerId == req->claimedTargetId)
                {
                    hitClaimed = true;
                    break;
                }
            }
            hitVerdict = hitClaimed ? HIT_VERDICT_HIT_CLAIMED_TARGET
                                    : HIT_VERDICT_HIT_OTHER_TARGET;
        }
        {
            int totalDmg = 0;
            for (const auto& agg : trace.aggregates)
                totalDmg += agg.damage;
            char msg[512];
            std::snprintf(msg, sizeof(msg),
                          "player=%u weapon=%s claimed=%u pellets=%d damage=%d verdict=%u origin=(%.2f,%.2f,%.2f) dir=(%.3f,%.3f,%.3f)",
                          shooter.id, def->id.c_str(), req->claimedTargetId,
                          trace.pelletCount, totalDmg, (unsigned)hitVerdict,
                          origin.x, origin.y, origin.z,
                          direction.x, direction.y, direction.z);
            ::logStructured(::StructuredCategory::Network, ::StructuredLevel::Important,
                            "ATTACK_HITSCAN_ACCEPT",
                            "ATTACK_" + std::to_string(req->requestId),
                            "authoritative hitscan trace result", msg);
        }
        sendAttackResult(sock, shooter, req, tick, true, 0, 0,
                         rt.magazineAmmo, rt.reserveAmmo,
                         rt.nextAllowedFireTick, rt.stateRevision,
                         hitVerdict);
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
        else if (def->behaviorType == WeaponBehaviorType::QuickHit)
        {
            // Start server-side quick hit attack
            shooter.quickHitState.active = true;
            shooter.quickHitState.activeTicksRemaining =
                (uint32_t)WeaponExecution::paramOr(*def, "activeHitboxTicks", 30.0f);
            shooter.quickHitState.attackSequenceId++;
            if (shooter.quickHitState.attackSequenceId == 0)
                shooter.quickHitState.attackSequenceId = 1;
            shooter.quickHitState.hasPreviousCapsule = false;
            shooter.quickHitState.hitCooldowns.clear();
            shooter.hasLastPhysicalWeaponShape = false;

            Debug::log(Debug::Category::Weapons,
                "[QUICK HIT SERVER] playerId=%u requestId=%u ticks=%u seq=%u\n",
                shooter.id, req->requestId,
                shooter.quickHitState.activeTicksRemaining,
                shooter.quickHitState.attackSequenceId);
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
                req->clientSimulationTick,
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
