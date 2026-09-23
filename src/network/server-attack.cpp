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
#include "network/server-gamemode.h"
#include "network/server-damage-policy.h"
#include "ecs/actor-entities.h"
#include "network/server-context.h"
#include "network/server-hitscan-outcome.h"
#include "network/server-hitscan-targets.h"
#include "network/server-hot-tool-state.h"
#include "network/server-weapon-state.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/relationship-store.h"
#include "live-code/live-behavior.h"
#include "network/disagreement-visuals.h"
#include "combat/weapon-execution.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-types.h"
#include "combat/weapon-fire.h"
#include "combat/weapon-runtime.h"
#include "config/networking-config.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "persistence/persistence-emit.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-attack-gates.h"
#include "hot-reload/hot-attack-claim.h"

#include <cmath>
#include <vector>

namespace MimitaNet {
namespace {

// Resolve the active generation's attack-gate policy through the one generic
// doorway. Never cached across a generation swap. Null when no hot provider.
static GameAttackGatesFn hotAttackGatesPolicy()
{
    void* callable = MimitaRuntime::GenericRuntime::instance().capability(
        GAME_CAP_ATTACK_GATES);
    return callable ? reinterpret_cast<GameAttackGatesFn>(callable) : nullptr;
}

static void runAttackGates(GameAttackGatesV1& gates)
{
    gates.structSize = sizeof(GameAttackGatesV1);
    GameAttackGatesFn policy = hotAttackGatesPolicy();
    if (policy)
        policy(nullptr, &gates);
    else
        HotAttackGatesImpl::evaluate(gates);
}

static void runAttackClaim(GameAttackClaimV1& claim)
{
    claim.structSize = sizeof(GameAttackClaimV1);
    auto policy = reinterpret_cast<GameAttackClaimFn>(
        MimitaRuntime::GenericRuntime::instance().capability(GAME_CAP_ATTACK_CLAIM));
    if (policy)
        policy(nullptr, &claim);
    else
        HotAttackClaimImpl::evaluate(claim);
}

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

// Builds the lag-compensated target list exactly as the cold hitscan trace
// expects, and records the ECS entity for each target so the same rewound
// hitboxes can be published to the hot behavior.
static void buildRewoundHitscanTargets(
    const ServerPlayer& shooter,
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    const std::unordered_map<uint32_t, ServerNpc>& npcs,
    uint32_t rewindTick,
    uint32_t currentTick,
    std::vector<WeaponExecution::PlayerTarget>& outTargets,
    std::vector<std::uint64_t>& outEntities)
{
    outTargets.clear();
    outEntities.clear();
    outTargets.reserve(players.size() + npcs.size());
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
        // Reconstruct the victim's real body-part hitboxes (head/torso/arms/
        // legs) at the rewound pose + rewound yaw from the standard body
        // template — never an invisible capsule.
        fillTargetBodyParts(targetDesc, targetDesc.position, rewoundYaw);
        outTargets.push_back(targetDesc);
        outEntities.push_back(Ecs::raw(
            Ecs::ensure(EntityRealm::Server, EntityDomain::Player, target.id)));
    }
    // Also include NPCs as trace targets, validated at the pose the attacker
    // actually saw (rewound like players).
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
        fillTargetBodyParts(targetDesc, targetDesc.position, rewoundYaw);
        outTargets.push_back(targetDesc);
        outEntities.push_back(Ecs::raw(
            Ecs::ensure(EntityRealm::Server, EntityDomain::Npc, npc.entityId)));

        // Debug: surface the visible-pose vs current-pose transform mismatch.
        // One aggregate line per second, never per-frame or per-shot spam.
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
                    npc.entityId, rewindTick, currentTick,
                    tracePos.x, tracePos.y, tracePos.z,
                    npc.pos.x, npc.pos.y, npc.pos.z, drift,
                    glm::degrees(rewoundYaw), glm::degrees(npc.yaw), yawDrift);
        }
    }
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

    // ── Hot attack-routing policy (full hot validation seam) ──────────
    // The cold server has parsed the request and resolved the definition; hot
    // code now owns the routing decision (accept/reject, reason, reported
    // ammo/cooldown, claim verdict). When no hot behavior handles it, the cold
    // validation below runs unchanged, so this is a safe opt-in seam.
    {
        AttackPolicyV1 policy{};
        policy.shooterEntity = Ecs::raw(
            Ecs::ensure(EntityRealm::Server, EntityDomain::Player, shooter.id));
        policy.shooterPlayerId = shooter.id;
        policy.spawnGeneration = req->spawnGeneration;
        policy.spawnStateActive = shooter.spawnState == ServerPlayer::Active ? 1u : 0u;
        policy.shooterDead = shooter.dead ? 1u : 0u;
        policy.weaponDefNetworkId = req->weaponDefNetworkId;
        policy.weaponNetworkId = networkWeaponTypeForDefinition(*def);
        policy.executionType = (std::uint32_t)def->executionType;
        policy.equippedSlot = (std::uint32_t)req->equippedSlot;
        const int hotLogicalSlot = serverCommunityWeaponLogicalSlot(def->id);
        policy.expectedSlot = (std::uint32_t)(hotLogicalSlot > 0 ? hotLogicalSlot : def->slot);
        policy.communityAllowed = serverCommunityWeaponAllowed(def->id) ? 1u : 0u;
        policy.shotsThisTick = shooter.shotsThisTick;
        policy.maxShotsPerTick = ServerPlayer::MAX_SHOTS_PER_TICK;
        policy.requestId = req->requestId;
        policy.clientSimulationTick = req->clientSimulationTick;
        policy.claimedTargetId = req->claimedTargetId;
        policy.origin[0] = req->muzzlePosX;
        policy.origin[1] = req->muzzlePosY;
        policy.origin[2] = req->muzzlePosZ;
        policy.direction[0] = req->aimDirX;
        policy.direction[1] = req->aimDirY;
        policy.direction[2] = req->aimDirZ;
        policy.shooterPos[0] = shooter.pos.x;
        policy.shooterPos[1] = shooter.pos.y;
        policy.shooterPos[2] = shooter.pos.z;
        const float hotPingAllowance = (float)shooter.pingMs / 1000.0f * 200.0f;
        policy.originTolerance = 12.0f + hotPingAllowance;
        policy.tick = tick;
        policy.hasDefinition = def ? 1u : 0u;
        // Default outputs reflect the cold policy so an unhandled event leaves
        // the cold path untouched.
        policy.accept = 1u;
        policy.hitVerdict = HIT_VERDICT_MISS;
        policy.magazineAmmo = -1;
        policy.reserveAmmo = -1;

        if (LiveBehavior::dispatchAttackPolicy(policy, tick) && policy.handled)
        {
            if (!policy.accept)
            {
                emitAttackRejection(sock, players, tick, totalPacketsOut, retransmitState,
                                    shooter, req->requestId,
                                    policy.reasonText[0] ? policy.reasonText : "HOT REJECT");
                sendAttackResult(sock, shooter, req, tick, false, (uint8_t)policy.reason,
                                 policy.projectileId,
                                 policy.magazineAmmo, policy.reserveAmmo,
                                 policy.nextAllowedFireTick, policy.stateRevision,
                                 (uint8_t)policy.hitVerdict);
                return;
            }
            if (policy.suppressColdFire)
            {
                sendAttackResult(sock, shooter, req, tick, true, (uint8_t)policy.reason,
                                 policy.projectileId,
                                 policy.magazineAmmo, policy.reserveAmmo,
                                 policy.nextAllowedFireTick, policy.stateRevision,
                                 (uint8_t)policy.hitVerdict);
                return;
            }
            // Hot accepted but left execution to the cold family dispatch.
        }
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
    // Migrated weapons: the authoritative ammo/cooldown/reload lives on the
    // tool entity's component; this legacy view is refreshed from it.
    serverWeaponStateLoad(shooter, *wepId);

    // ── Validate or reconcile equipped slot ───────────────────────────
    const int logicalSlot = serverCommunityWeaponLogicalSlot(def->id);
    const int expectedSlot = logicalSlot > 0 ? logicalSlot : def->slot;
    if (req->equippedSlot != expectedSlot)
    {
        Debug::log(Debug::Category::Weapons,
                   "[ATTACK REJECT] playerId=%u requestId=%u request slot does not match weapon req=%d def=%d\n",
                   shooter.id, req->requestId, req->equippedSlot, expectedSlot);
        emitAttackRejection(sock, players, tick, totalPacketsOut, retransmitState,
                            shooter, req->requestId, "SLOT MISMATCH");
        sendAttackResult(sock, shooter, req, tick, false, 4, 0, -1, -1, 0, 0);
        return;
    }
    const int nativeSlot = serverCommunityWeaponNativeSlot(req->equippedSlot);
    if (nativeSlot != shooter.equippedSlot)
    {
        Debug::log(Debug::Category::Weapons,
                   "[ATTACK EQUIP RECONCILE] playerId=%u requestId=%u oldSlot=%d newSlot=%d weapon=%s\n",
                   shooter.id, req->requestId, shooter.equippedSlot,
                   nativeSlot, def->id.c_str());
        shooter.equippedSlot = nativeSlot;
    }

    // ── Ammo is client-authoritative ──────────────────────────────────
    // The client owns its clip (decrement + local reload). The server no
    // longer rejects shots for ammo — it only rate-limits via cooldown.
    // The server-side magazine counter stays informational (never below 0).
    const bool consumesAmmo = def->executionType != WeaponExecutionType::PhysicalContact &&
        def->magazineSize > 0;

    // ── Attack gates: cooldown grace + per-tick shot rate ─────────────
    // A tool with hot per-instance state owns its own cooldown; the hot
    // behavior enforces it and declines when still cooling down. Do not also
    // rate-limit it through the legacy tick counter (that would be a second
    // owner of the same concept). The thresholds and decisions are hot
    // (net.attack-gates).
    const bool hotStateOwns = serverHotToolStateHas(shooter.equippedToolEntity);
    GameAttackGatesV1 gates{};
    gates.hotStateOwns = hotStateOwns ? 1u : 0u;
    gates.isPhysicalContact =
        def->executionType == WeaponExecutionType::PhysicalContact ? 1u : 0u;
    gates.isHitscan = def->executionType == WeaponExecutionType::Hitscan ? 1u : 0u;
    gates.tick = tick;
    gates.nextAllowedFireTick = rt.nextAllowedFireTick;
    gates.cooldownGraceTicks = 2;
    gates.shotsThisTick = shooter.shotsThisTick;
    gates.maxShotsPerTick = ServerPlayer::MAX_SHOTS_PER_TICK;
    runAttackGates(gates);
    if (gates.cooldownReject)
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
    if (gates.rateLimitReject)
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

    // ── Lag-compensated hitscan targets (shared by cold trace and hot) ─
    // Built once here so the hot behavior can validate against the SAME
    // rewound body-part hitboxes the cold trace uses, published through the
    // generic dynamic-component layer.
    const bool hitscanFamily = def->executionType == WeaponExecutionType::Hitscan;
    std::vector<WeaponExecution::PlayerTarget> hitscanTargets;
    std::vector<std::uint64_t> hitscanTargetEntities;
    uint32_t hitscanRewindTick = 0;
    if (hitscanFamily)
    {
        hitscanRewindTick =
            estimateServerRewindTick(shooter, req->clientSimulationTick, tick);
        buildRewoundHitscanTargets(shooter, players, npcs, hitscanRewindTick, tick,
                                   hitscanTargets, hitscanTargetEntities);
        publishHitscanTargets(hitscanTargetEntities, hitscanTargets);
    }

    // ── Generic tool-use fact (phase-0 hot-first gate) ────────────────
    // Offered to hot code for EVERY execution family. A definition opts in by
    // setting TOOL_FLAG_OWNS_EXECUTION; until then the hot router declines and
    // the cold family dispatch below stays authoritative. When hot owns the use
    // it has already applied the authoritative consequence, so report accepted.
    {
        ToolUsePolicyV1 use{};
        use.ownerId = shooter.id;
        use.userEntity = static_cast<std::uint64_t>(
            Ecs::ensure(EntityRealm::Server, EntityDomain::Player, shooter.id));
        use.toolEntity = shooter.equippedToolEntity;
        if (use.toolEntity == 0)
            use.toolEntity = serverWeaponToolEntity(shooter, *wepId, false);
        use.toolNetworkId = req->weaponDefNetworkId;
        // Recipe/behavior key: the definition id hash. The hot router also
        // accepts the numeric family id as a fallback.
        use.toolId = gameHash(def->id.c_str());
        use.kind = 0;
        use.tick = tick;
        use.baseFire = 1;
        use.outFire = 1;
        use.ammoCost = consumesAmmo ? 1u : 0u;
        use.origin[0] = req->muzzlePosX;
        use.origin[1] = req->muzzlePosY;
        use.origin[2] = req->muzzlePosZ;
        use.direction[0] = req->aimDirX;
        use.direction[1] = req->aimDirY;
        use.direction[2] = req->aimDirZ;
        // Generic prediction key: the client's request id, so the authoritative
        // entity a hot tool creates can be linked back to the prediction.
        use.predictionKey = req->requestId;
        use.claimedTargetId = req->claimedTargetId;
        use.clientSimulationTick = req->clientSimulationTick;
        LiveBehavior::dispatchToolUse(use, tick);
        if (use.handled && use.outFire == 0)
        {
            // Hot owns this use. Its per-instance tool state is the single owner
            // of ammo/cooldown/reload: report it and do NOT write the legacy
            // component (that would be a second owner of the same concept).
            ToolInstanceStateV1 hotState{};
            if (serverHotToolStateRead(use.toolEntity, hotState))
            {
                const std::uint64_t nextAllowedTick = tick + (std::uint64_t)std::ceil(
                    std::max(0.0f, hotState.cooldownRemaining) *
                    (double)SERVER_TICK_RATE);
                sendAttackResult(sock, shooter, req, tick, true, 0, 0,
                                 hotState.currentAmmo, hotState.reserveAmmo,
                                 nextAllowedTick, hotState.stateVersion);
            }
            else
            {
                // Behavior claimed without tool state (e.g. a stateless tool):
                // report the legacy view unchanged; never invent state here.
                sendAttackResult(sock, shooter, req, tick, true, 0, 0,
                                 rt.magazineAmmo, rt.reserveAmmo,
                                 rt.nextAllowedFireTick, rt.stateRevision);
            }
            Debug::log(Debug::Category::Weapons,
                "[ATTACK HOT ACCEPT] playerId=%u weapon=%s ownedByHot=1 ammo=%d/%d\n",
                shooter.id, def->id.c_str(),
                hotState.currentAmmo, hotState.reserveAmmo);
            if (hitscanFamily)
                clearHitscanTargets(hitscanTargetEntities);
            return;
        }
    }
    if (hitscanFamily)
        clearHitscanTargets(hitscanTargetEntities);

    // ── Dispatch by execution family ──────────────────────────────────
    if (def->executionType == WeaponExecutionType::Hitscan)
    {
        const glm::vec3 reqOrigin(req->muzzlePosX, req->muzzlePosY, req->muzzlePosZ);
        const glm::vec3 fallbackOrigin(req->aimOriginX, req->aimOriginY, req->aimOriginZ);
        glm::vec3 origin = finiteVec3(reqOrigin) ? reqOrigin : fallbackOrigin;
        glm::vec3 direction = normalizedOrZero(
            glm::vec3(req->aimDirX, req->aimDirY, req->aimDirZ));
        // The muzzle may lead the server's record of the shooter by the distance
        // the player could travel during their round-trip latency; the tolerance
        // and the geometry decision are hot (net.attack-gates). The cooldown and
        // shot-rate gates are suppressed here (already applied above).
        GameAttackGatesV1 geometry{};
        geometry.hotStateOwns = 1u;
        geometry.isHitscan = 1u;
        geometry.shotsThisTick = 0u;
        geometry.maxShotsPerTick = 1u;
        geometry.pingMs = (float)shooter.pingMs;
        for (int i = 0; i < 3; ++i)
        {
            geometry.shooterPos[i] = shooter.pos[i];
            geometry.origin[i] = origin[i];
            geometry.direction[i] = direction[i];
        }
        runAttackGates(geometry);
        if (geometry.geometryReject)
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

        // Lag-compensated targets were built once before the hot dispatch so
        // the cold trace and the hot behavior validate the same rewound pose.
        const uint32_t rewindTick = hitscanRewindTick;
        std::vector<WeaponExecution::PlayerTarget>& targets = hitscanTargets;

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
            // Structural claim eligibility (distance, prior hit, world
            // occlusion) and the acceptance tolerance are hot (net.attack-claim).
            const NetworkingConfigData& netCfg = NetworkingConfig::instance().data();
            GameAttackClaimV1 claim{};
            claim.claimedTargetId = req->claimedTargetId;
            claim.alreadyConfirmed = alreadyConfirmed ? 1u : 0u;
            claim.claimedDistance = claimedDist;
            claim.maxRange = maxRange;
            claim.worldBlockDistance = worldBlockDistance;
            claim.rewindHitTolerance = netCfg.remotePlayers.rewindHitTolerance;
            claim.claimLagAllowance = netCfg.remotePlayers.claimLagAllowance;
            runAttackClaim(claim);
            if (claim.eligible)
            {
                // Occlusion was already excluded by the hot eligibility gate.
                bool occluded = false;
                if (!occluded)
                {
                    const float tolerance = claim.tolerance;
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
        // Persist the authoritative result back onto the tool entity.
        serverWeaponStateStore(shooter, *wepId);

        const uint8_t hitVerdict = serverResolveHitscanOutcome(
            sock, players, npcs, shooter, *def, trace,
            origin, direction, worldHit, worldNormal,
            traceConfig.maxRange, traceConfig.worldBlockDistance,
            req->requestId, req->clientSimulationTick, req->claimedTargetId,
            tick, totalPacketsOut);

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
         def->behaviorType == WeaponBehaviorType::GrenadeLauncher ||
         def->behaviorType == WeaponBehaviorType::Grenade))
    {
        glm::vec3 direction(req->aimDirX, req->aimDirY, req->aimDirZ);
        direction = normalizedOrZero(direction);
        if (glm::length(direction) <= 0.0001f)
        {
            sendAttackResult(sock, shooter, req, tick, false, 5, 0,
                             rt.magazineAmmo, rt.reserveAmmo,
                             rt.nextAllowedFireTick, rt.stateRevision);
            return;
        }

        // The phase-0 gate above already offered this use to hot; reaching here
        // means the definition has not opted into hot execution. The kernel
        // container spawn is retired, so reject rather than revive a second path.
        sendAttackResult(sock, shooter, req, tick, false, 7, 0,
                         rt.magazineAmmo, rt.reserveAmmo,
                         rt.nextAllowedFireTick, rt.stateRevision);
        Debug::log(Debug::Category::Weapons,
            "[ATTACK PROJECTILE REJECT] playerId=%u requestId=%u weapon=%s reason=cold-projectile-unsupported ammo=%d/%d stateRev=%u\n",
            shooter.id, req->requestId, def->id.c_str(),
            rt.magazineAmmo, rt.reserveAmmo, rt.stateRevision);
        return;
    }

    // ── Unknown behavior type ─────────────────────────────────────────
    Debug::log(Debug::Category::Weapons, "[ATTACK REJECT] playerId=%u requestId=%u unknown behaviorType=%d\n",
               shooter.id, req->requestId, (int)def->behaviorType);
    sendAttackResult(sock, shooter, req, tick, false, 9, 0,
                     rt.magazineAmmo, rt.reserveAmmo,
                     rt.nextAllowedFireTick, rt.stateRevision);
}

// ── Held-fire intent ────────────────────────────────────────────────
void handleFireIntentPacket(SOCKET, const char* buffer, int bytes,
                            std::unordered_map<uint32_t, ServerPlayer>& players,
                            uint32_t tick)
{
    if (bytes < (int)sizeof(FireIntentPacket))
        return;
    const FireIntentPacket* req = reinterpret_cast<const FireIntentPacket*>(buffer);
    auto it = players.find(req->header.playerId);
    if (it == players.end())
        return;
    ServerPlayer& player = it->second;
    if (player.dead)
        return;

    // Generic runtime tool: lazily create/link the equipped tool entity on the
    // first use. No registered network weapon is required.
    if (req->toolId != 0 && player.equippedToolEntity == 0)
    {
        const EntityId playerEntity =
            Ecs::ensure(EntityRealm::Server, EntityDomain::Player, player.id);
        const EntityId toolEntity =
            EntityRegistry::instance().createGeneric(EntityRealm::Server);
        const std::uint64_t id = req->toolId;
        MimitaRuntime::DynamicComponentStore::instance().write(
            toolEntity, gameHash("EquippedTool"), &id, sizeof(id));
        MimitaRuntime::RelationshipStore::instance().add(
            gameHash("relationship.owns-tool"), playerEntity, toolEntity, req->toolId);
        player.equippedToolEntity = static_cast<std::uint64_t>(toolEntity);
        player.runtimeToolId = req->toolId;
        Debug::log(Debug::Category::Weapons,
            "[TOOL EQUIP] player=%u id=%llu entity=%llu (from action intent)\n",
            player.id, (unsigned long long)req->toolId,
            (unsigned long long)toolEntity);
    }

    if (req->action == FIRE_INTENT_STOP)
    {
        player.heldFire.active = false;
        return;
    }

    HeldFireState& held = player.heldFire;
    if (!held.active || held.intentId != req->intentId)
    {
        held = HeldFireState{};
        held.active = true;
        held.intentId = req->intentId;
        held.startTick = req->startTick ? req->startTick : tick;
        held.lastEmitTick = tick - 1;  // allow an emission this tick
    }
    held.weaponDefNetworkId = req->weaponDefNetworkId;
    held.toolId = req->toolId;
    held.attackVariant = req->attackVariant;
    held.deterministicSeed = req->deterministicSeed;
    held.origin = glm::vec3(req->originX, req->originY, req->originZ);
    held.direction = glm::vec3(req->dirX, req->dirY, req->dirZ);
    held.reportedCount = req->count;
    held.lastHeartbeatTick = tick;
}

void tickHeldFireIntents(
    SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs,
    std::unordered_map<uint32_t, ServerProjectile>& projectiles,
    uint32_t& nextProjectileId,
    uint32_t tick,
    uint64_t& totalPacketsOut)
{
    for (auto& entry : players)
    {
        ServerPlayer& player = entry.second;
        HeldFireState& held = player.heldFire;
        if (!held.active)
            continue;
        if (player.dead || tick <= held.lastEmitTick)
            continue;

        // Generic runtime tool path: no registered network weapon is required.
        // The server resolves the equipped tool entity and dispatches the
        // generic action; the hot behavior owns the authoritative consequence.
        if (held.toolId != 0)
        {
            ToolUsePolicyV1 use{};
            use.ownerId = player.id;
            use.userEntity = static_cast<std::uint64_t>(
                Ecs::ensure(EntityRealm::Server, EntityDomain::Player, player.id));
            use.toolEntity = player.equippedToolEntity;
            use.toolId = held.toolId;
            use.toolNetworkId = held.weaponDefNetworkId;
            use.kind = 0;
            use.tick = tick;
            use.baseFire = 1;
            use.outFire = 1;
            use.ammoCost = 0;
            use.origin[0] = held.origin.x; use.origin[1] = held.origin.y; use.origin[2] = held.origin.z;
            use.direction[0] = held.direction.x; use.direction[1] = held.direction.y; use.direction[2] = held.direction.z;
            use.predictionKey = held.intentId;
            LiveBehavior::dispatchToolUse(use, tick);
            held.lastEmitTick = tick;
            continue;
        }

        const std::string* weaponId =
            weaponIdForDefNetworkId(held.weaponDefNetworkId);
        if (!weaponId)
        {
            held.active = false;
            continue;
        }
        const WeaponDefinition* def = WeaponRegistry::instance().get(*weaponId);
        if (!def)
        {
            held.active = false;
            continue;
        }
        auto rtIt = player.weaponRuntimes.find(*weaponId);
        if (rtIt == player.weaponRuntimes.end() || !rtIt->second.initialized)
        {
            held.active = false;
            continue;
        }
        if (rtIt->second.magazineAmmo <= 0)
        {
            held.active = false;
            continue;
        }

        // Generic tool-use fact: a hot behavior may own this held use.
        ToolUsePolicyV1 use{};
        use.ownerId = player.id;
        use.toolNetworkId = held.weaponDefNetworkId;
        use.toolId = networkWeaponTypeForDefinition(*def);
        use.kind = 0;
        use.tick = tick;
        use.baseFire = 1;
        use.outFire = 1;
        use.ammoCost = 1;
        {
            const glm::vec3 useOrigin = player.pos + glm::vec3(0.0f, 0.0f, 0.8f);
            const glm::vec3 useDir = glm::length(held.direction) > 0.001f
                ? glm::normalize(held.direction) : glm::vec3(1.0f, 0.0f, 0.0f);
            use.origin[0] = useOrigin.x; use.origin[1] = useOrigin.y; use.origin[2] = useOrigin.z;
            use.direction[0] = useDir.x; use.direction[1] = useDir.y; use.direction[2] = useDir.z;
        }
        use.predictionKey = held.intentId;
        LiveBehavior::dispatchToolUse(use, tick);
        if (use.handled && use.outFire == 0)
        {
            held.lastEmitTick = tick;
            continue;
        }

        // Hot fire-intent policy: one decision per held tick.
        FireIntentPolicyV1 policy{};
        policy.entity = player.id;
        policy.weaponNetworkId = held.weaponDefNetworkId;
        policy.tick = tick;
        policy.baseFire = 1;
        policy.outFire = 1;
        policy.ammoCost = 1;
        LiveBehavior::dispatchFireIntent(policy, tick);
        if (policy.handled && policy.outFire == 0)
        {
            held.lastEmitTick = tick;
            continue;
        }

        glm::vec3 direction = glm::length(held.direction) > 0.001f
            ? glm::normalize(held.direction) : glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 origin = player.pos + glm::vec3(0.0f, 0.0f, 0.8f);
        const uint32_t requestId = held.intentId * 100000u + held.emittedCount + 1u;
        // Held fire enforces at most one projectile per tick itself.
        rtIt->second.nextAllowedFireTick = tick;

        held.lastEmitTick = tick;
        // Projectile tools are edge-triggered through the generic action seam.
        // Never revive the removed kernel-container fallback for held intent.
        held.active = false;
    }
}

// ── Generic item containment / equip state ─────────────────────────────
// Actor --contains-item--> Item entity and Actor --equips-item--> Item entity.
// The SAME item EntityId survives inventory -> equip -> drop -> pickup ->
// re-equip; behavior bindings fire on each lifecycle event. No weapon maps.
namespace {
const std::uint64_t kRelContainsItem = gameHash("relationship.contains-item");
const std::uint64_t kRelEquipsItem = gameHash("relationship.equips-item");
const std::uint64_t kCompEquippedTool = gameHash("EquippedTool");

EntityId actorEntityForPlayer(uint32_t playerId)
{
    return Ecs::ensure(EntityRealm::Server, EntityDomain::Player, playerId);
}

void fireItemBinding(EntityId itemEntity, const char* eventName)
{
    ServerContextV1* context = activeServerContext();
    const std::uint64_t tick = (context && context->tick) ? *context->tick : 0;
    LiveBehavior::runBehaviorBindings(
        static_cast<std::uint64_t>(itemEntity),
        static_cast<std::uint32_t>(gameHash(eventName)), nullptr, 0, tick);
}
} // namespace

bool serverItemContains(std::uint32_t playerId, std::uint64_t itemEntity)
{
    return MimitaRuntime::RelationshipStore::instance().has(
        kRelContainsItem, static_cast<EntityId>(actorEntityForPlayer(playerId)),
        static_cast<EntityId>(itemEntity));
}

bool serverItemEquip(std::uint32_t playerId, std::uint64_t itemEntity)
{
    const EntityId actor = actorEntityForPlayer(playerId);
    const EntityId item = static_cast<EntityId>(itemEntity);
    if (item == kInvalidEntityId || !EntityRegistry::instance().alive(item))
        return false;
    auto& rel = MimitaRuntime::RelationshipStore::instance();
    if (!rel.has(kRelContainsItem, actor, item))
        rel.add(kRelContainsItem, actor, item, 0);
    // One equipped item per actor: clear any previous equip edge.
    std::uint64_t previous[8] = {0};
    const std::size_t count = rel.query(kRelEquipsItem, actor, previous, nullptr, 8);
    for (std::size_t i = 0; i < count; ++i)
        rel.remove(kRelEquipsItem, actor, previous[i]);
    rel.add(kRelEquipsItem, actor, item, 0);
    fireItemBinding(item, "on.equip");
    return true;
}

bool serverItemUnequip(std::uint32_t playerId)
{
    const EntityId actor = actorEntityForPlayer(playerId);
    auto& rel = MimitaRuntime::RelationshipStore::instance();
    std::uint64_t equipped[8] = {0};
    const std::size_t count = rel.query(kRelEquipsItem, actor, equipped, nullptr, 8);
    for (std::size_t i = 0; i < count; ++i) {
        rel.remove(kRelEquipsItem, actor, equipped[i]);
        fireItemBinding(equipped[i], "on.unequip");
    }
    return count > 0;
}

bool serverItemDrop(std::uint32_t playerId, std::uint64_t itemEntity)
{
    const EntityId actor = actorEntityForPlayer(playerId);
    const EntityId item = static_cast<EntityId>(itemEntity);
    auto& rel = MimitaRuntime::RelationshipStore::instance();
    rel.remove(kRelEquipsItem, actor, item);
    rel.remove(kRelContainsItem, actor, item);
    fireItemBinding(item, "on.drop");
    return true;
}

bool serverItemPickup(std::uint32_t playerId, std::uint64_t itemEntity)
{
    const EntityId actor = actorEntityForPlayer(playerId);
    const EntityId item = static_cast<EntityId>(itemEntity);
    if (item == kInvalidEntityId || !EntityRegistry::instance().alive(item))
        return false;
    MimitaRuntime::RelationshipStore::instance().add(kRelContainsItem, actor, item, 0);
    fireItemBinding(item, "on.pickup");
    return true;
}

// Generic runtime-tool equip. No WeaponRegistry def or NETWORK_WEAPON_* id is
// required: the tool becomes an entity the player owns and equips, and its
// behavior is resolved generically at use time. This is the equip/containment
// state that the legacy weapon slot maps are migrating to.
bool serverEquipRuntimeTool(std::unordered_map<uint32_t, ServerPlayer>& players,
                            uint32_t playerId, const std::string& toolName)
{
    auto it = players.find(playerId);
    if (it == players.end() || toolName.empty())
        return false;
    ServerPlayer& player = it->second;
    const std::uint64_t toolId = gameHash(toolName.c_str());
    const EntityId playerEntity =
        Ecs::ensure(EntityRealm::Server, EntityDomain::Player, player.id);

    EntityId toolEntity = static_cast<EntityId>(player.equippedToolEntity);
    if (toolEntity == kInvalidEntityId ||
        !EntityRegistry::instance().alive(toolEntity)) {
        toolEntity = EntityRegistry::instance().createGeneric(EntityRealm::Server);
        // Persist the runtime tool identity as generic component state.
        const std::uint64_t id = toolId;
        MimitaRuntime::DynamicComponentStore::instance().write(
            toolEntity, gameHash("EquippedTool"), &id, sizeof(id));
    }
    player.runtimeToolId = toolId;
    player.equippedToolEntity = static_cast<std::uint64_t>(toolEntity);
    MimitaRuntime::RelationshipStore::instance().add(
        kRelContainsItem, playerEntity, toolEntity, toolId);
    serverItemEquip(playerId, static_cast<std::uint64_t>(toolEntity));
    Debug::log(Debug::Category::Weapons,
        "[TOOL EQUIP] player=%u tool=%s id=%llu entity=%llu\n",
        player.id, toolName.c_str(), (unsigned long long)toolId,
        (unsigned long long)toolEntity);
    return true;
}

} // namespace MimitaNet
