// 2026-10-17
/* purpose
* Implements the local client Melee action driver. See melee-client.h.
*/

#include "melee-client.h"

#include "camera.h"
#include "entities/player.h"
#include "entities/animation-movement-state.h"
#include "combat/weapon-system.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-melee-config.h"
#include "combat/weapon-attachment.h"
#include "combat/weapon-collision-config.h"
#include "combat/weapon-execution.h"
#include "combat/weapon-types.h"
#include "effects/hit-effects.h"
#include "ui/hitmarker.h"
#include "network/multiplayer-context.h"
#include "network/packets.h"
#include "network/network-weapons.h"
#include "network/community-server-config.h"
#include "audio/audio.h"
#include "debug/structured-log.h"
#include "debug/debug-visuals.h"
#include "physics/config.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>

namespace MimitaMelee {
namespace {

using WeaponData::MeleeAction;
using WeaponData::MeleeActorState;
using WeaponData::MeleeButton;
using WeaponData::MeleeModifier;
using WeaponData::MeleeMoveSet;

// Test/dev forced action (agent.* commands) and limb-pose tracking for logging.
std::string g_forceActionId;
bool g_debugCrouch = false;
uint32_t g_lastChainsawRevTick = 0;
std::unordered_map<std::string, glm::vec3> g_lastPartPos;
std::unordered_map<std::string, glm::vec3> g_lastPartTip;
std::string g_lastPhaseKey;

struct ClientActionState {
    bool active = false;
    std::string definitionId;
    std::string actionId;         // current action id (may be the double-click upgrade)
    std::string basePrimaryId;    // base primary action id for the double-click window
    uint32_t startSimTick = 0;
    AnimationMovementState startMovementState = AnimationMovementState::Idle;
    bool startMovementPressed = false;
    int startHp = 100;
    float startPercent = 0.0f;
    float startHurtTimer = 0.0f;
    float startDashTimer = 0.0f;
    float startDownDashTimer = 0.0f;
    float startKnockbackTimer = 0.0f;
    float startRespawnTimer = 0.0f;
    bool startFreezeActive = false;
    bool startDeathActive = false;
    int64_t lastPrimarySimTick = -1000000;
    int64_t lastSecondarySimTick = -1000000;
    std::string baseSecondaryId;
    glm::vec3 forward{1.0f, 0.0f, 0.0f};
    uint32_t lastAdvanceSimTick = 0;
    bool hasLastAdvance = false;
    // Per-tick impulse guard: the last phase tick processed.
    int lastImpulsePhaseTick = -1;
    int lastImpulsePhaseIndex = -1; // 0=windup 1=active 2=recovery
    int currentPhaseIndex = -1;
    int currentPhaseTick = -1;
    // Snapshot of the action data at start (retains the starting revision).
    WeaponData::MeleeAction actionSnapshot;
};

ClientActionState g_state;
bool g_shieldHeld = false;
bool g_debugShield = false;
ClientInput g_pendingInput;
bool g_primaryHeld = false, g_secondaryHeld = false;

// ── Client melee contact claims ────────────────────────────────────────
// The client detects the same physical blade contact it renders and sends a
// batched claim. The server still owns final damage but accepts a contact when
// the claimed blade is within a lenient radius of the rewound target.
struct PendingMeleeClaim {
    uint32_t targetId = 0;
    bool targetIsNpc = false;
    uint32_t contactTick = 0;
    uint32_t contactId = 0;
    glm::vec3 hitPosition{0.0f};
    glm::vec3 direction{0.0f, 0.0f, 1.0f};
    glm::vec3 boxCenter{0.0f};
    glm::vec3 boxHalf{0.0f};
    glm::vec3 boxAxes[3] = {glm::vec3(1,0,0), glm::vec3(0,1,0), glm::vec3(0,0,1)};
    float impactSpeed = 0.0f;
    float impactForce = 0.0f;
    float impactDirectness = 0.0f;
    uint8_t flags = 0;
};

std::vector<PendingMeleeClaim> g_pendingMeleeClaims;
WeaponExecution::PhysicalContactShape g_lastClientMeleeShape;
std::string g_lastClientMeleeWeapon;
uint32_t g_nextMeleeContactId = 1;
uint32_t g_lastMeleeContactSimTick = 0;
uint32_t g_lastMeleeClaimFlushTick = 0;

bool isClaimMeleeWeapon(const WeaponDefinition& def)
{
    return def.meleeContact.enabled;
}

struct ClientDamageBox {
    glm::vec3 center{0.0f};
    glm::vec3 half{0.1f};
    glm::vec3 axes[3] = {glm::vec3(1,0,0), glm::vec3(0,1,0), glm::vec3(0,0,1)};
};

bool obbSphereOverlap(const ClientDamageBox& box,
                      const glm::vec3& sphereCenter,
                      float sphereRadius)
{
    const glm::vec3 delta = sphereCenter - box.center;
    const float x = glm::clamp(glm::dot(delta, box.axes[0]), -box.half.x, box.half.x);
    const float y = glm::clamp(glm::dot(delta, box.axes[1]), -box.half.y, box.half.y);
    const float z = glm::clamp(glm::dot(delta, box.axes[2]), -box.half.z, box.half.z);
    const glm::vec3 closest = box.center + box.axes[0] * x + box.axes[1] * y + box.axes[2] * z;
    return glm::length(sphereCenter - closest) <= sphereRadius;
}

std::vector<ClientDamageBox> clientCollisionDamageBoxes(const Player& player,
                                                        const WeaponDefinition& def)
{
    std::vector<ClientDamageBox> out;
    const WeaponCollisionEntry* entry = WeaponCollisionJsonConfig::instance().get(def.id);
    if (!entry || entry->source != "boxes" || entry->boxes.empty())
        return out;
    auto buildRot = [](const glm::vec3& rotDeg) {
        glm::mat4 rot(1.0f);
        rot = glm::rotate(rot, glm::radians(rotDeg.x), glm::vec3(1,0,0));
        rot = glm::rotate(rot, glm::radians(rotDeg.y), glm::vec3(0,1,0));
        rot = glm::rotate(rot, glm::radians(rotDeg.z), glm::vec3(0,0,1));
        return rot;
    };
    for (const WeaponCollisionBoxConfig& box : entry->boxes)
    {
        if (!box.enabled)
            continue;
        const glm::mat4 rot = buildRot(box.rotationDegrees);
        const glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), box.scale);
        const glm::mat4 world = player.weaponCollisionWorld * rot * scaleMat;
        ClientDamageBox outBox;
        outBox.center = glm::vec3(world * glm::vec4(box.center, 1.0f));
        for (int axis = 0; axis < 3; ++axis)
        {
            const glm::vec3 column(world[axis]);
            const float length = glm::length(column);
            outBox.axes[axis] = length > 0.0001f ? column / length
                                                 : (axis == 0 ? glm::vec3(1,0,0)
                                                    : (axis == 1 ? glm::vec3(0,1,0)
                                                                 : glm::vec3(0,0,1)));
            (&outBox.half.x)[axis] = std::max(0.01f,
                (&box.halfSize.x)[axis] * std::max(length, 0.001f));
        }
        out.push_back(outBox);
    }
    return out;
}

std::vector<ClientDamageBox> g_lastClientDamageBoxes;
std::string g_lastClientDamageBoxWeapon;

WeaponExecution::HitBodyPart bodyPartFromName(const std::string& name)
{
    if (name == "head") return WeaponExecution::HitBodyPart::Head;
    if (name == "leftLeg" || name == "rightLeg")
        return WeaponExecution::HitBodyPart::Leg;
    return WeaponExecution::HitBodyPart::Torso;
}

WeaponExecution::PlayerTarget makeClientTarget(const Player& player,
                                               uint32_t id,
                                               bool npc,
                                               uint32_t generation)
{
    WeaponExecution::PlayerTarget target;
    target.playerId = id;
    target.spawnGeneration = npc ? generation : player.spawnGeneration;
    target.position = player.pos;
    target.radius = 0.65f;
    target.height = 1.8f;
    target.dead = player.dead;
    for (const PhysicalBodyPart& part : player.physicalBody.parts)
    {
        glm::vec3 low(std::numeric_limits<float>::max());
        glm::vec3 high = -low;
        for (int corner = 0; corner < 8; ++corner)
        {
            const glm::vec3 local(
                (corner & 1) ? part.collider.localMax.x : part.collider.localMin.x,
                (corner & 2) ? part.collider.localMax.y : part.collider.localMin.y,
                (corner & 4) ? part.collider.localMax.z : part.collider.localMin.z);
            const glm::vec3 world = glm::vec3(part.worldTransform * glm::vec4(local, 1.0f));
            low = glm::min(low, world);
            high = glm::max(high, world);
        }
        if (glm::any(glm::greaterThan(low, high)))
            continue;
        WeaponExecution::PlayerTarget::BodyPartBox box;
        box.center = (low + high) * 0.5f;
        box.half = (high - low) * 0.5f;
        box.bodyPart = bodyPartFromName(part.name);
        target.bodyParts.push_back(box);
    }
    return target;
}

bool clientMeleeShape(const Player& player,
                      const WeaponDefinition& def,
                      const MeleeAction* action,
                      WeaponExecution::PhysicalContactShape& shape)
{
    glm::vec3 actionRotation(0.0f);
    glm::vec3 actionOffset(0.0f);
    if (action && action->hasWeaponPose)
    {
        actionRotation = action->weaponPoseRotation;
        actionOffset = action->weaponPoseOffset;
    }
    for (const PhysicalBodyPart& part : player.physicalBody.parts)
    {
        if (part.name != "rightArm")
            continue;
        WeaponAttachmentPose attachment;
        if (!resolveWeaponAttachment(def, part, player.sizeScale, attachment,
                                     actionRotation, actionOffset))
            return false;
        shape.kind = WeaponExecution::PhysicalShapeKind::Capsule;
        shape.currentA = attachment.bladeBase;
        shape.currentB = attachment.bladeTip;
        shape.radius = attachment.bladeRadius;
        shape.previousA = shape.currentA;
        shape.previousB = shape.currentB;
        return true;
    }
    return false;
}

float clientContactDamage(const WeaponDefinition& def,
                          const MeleeAction* action,
                          bool actionActive,
                          float force,
                          float revMultiplier)
{
    float minDamage = WeaponExecution::paramOr(def, "contactMinDamage", 1.0f);
    float maxDamage = WeaponExecution::paramOr(def, "contactMaxDamage", (float)def.damage);
    if (action && actionActive)
    {
        if (action->contactMinDamage >= 0.0f) minDamage = action->contactMinDamage;
        if (action->contactMaxDamage >= 0.0f) maxDamage = action->contactMaxDamage;
        minDamage *= std::max(0.0f, action->contactDamageMultiplier);
        maxDamage *= std::max(0.0f, action->contactDamageMultiplier);
    }
    minDamage *= revMultiplier;
    maxDamage *= revMultiplier;
    return std::max(1.0f, std::round(glm::mix(minDamage, maxDamage,
        glm::clamp(force, 0.0f, 1.0f)) *
        std::max(0.0f, def.meleeContact.damageScale)));
}

void flushMeleeContactClaims(MimitaNet::MultiplayerContext& ctx)
{
    while (!g_pendingMeleeClaims.empty())
    {
        MimitaNet::SpyKnifeContactBatchPacket batch{};
        batch.header.type = MimitaNet::PACKET_NPC_DAMAGE_REQUEST;
        batch.header.tick = ctx.tick;
        batch.header.playerId = ctx.localPlayerId;
        batch.attackerId = ctx.localPlayerId;
        batch.attackerSpawnGeneration = ctx.lastKnownSpawnGeneration;
        batch.batchId = g_nextMeleeContactId;
        const size_t count = std::min<size_t>(
            MimitaNet::SPYKNIFE_CONTACT_BATCH_MAX, g_pendingMeleeClaims.size());
        batch.contactCount = static_cast<uint8_t>(count);
        for (size_t i = 0; i < count; ++i)
        {
            const PendingMeleeClaim& claim = g_pendingMeleeClaims[i];
            auto& out = batch.contacts[i];
            out.targetId = claim.targetId;
            out.contactTick = claim.contactTick;
            out.contactId = claim.contactId;
            out.targetIsNpc = claim.targetIsNpc ? 1 : 0;
            out.isBackstab = 0;
            out.impactSpeed = claim.impactSpeed;
            out.impactForce = claim.impactForce;
            out.impactDirectness = claim.impactDirectness;
            out.reserved = claim.flags;
            out.hitX = claim.hitPosition.x;
            out.hitY = claim.hitPosition.y;
            out.hitZ = claim.hitPosition.z;
            out.dirX = claim.direction.x;
            out.dirY = claim.direction.y;
            out.dirZ = claim.direction.z;
            out.boxCenterX = claim.boxCenter.x;
            out.boxCenterY = claim.boxCenter.y;
            out.boxCenterZ = claim.boxCenter.z;
            out.boxHalfX = claim.boxHalf.x;
            out.boxHalfY = claim.boxHalf.y;
            out.boxHalfZ = claim.boxHalf.z;
            out.boxAxis0X = claim.boxAxes[0].x;
            out.boxAxis0Y = claim.boxAxes[0].y;
            out.boxAxis0Z = claim.boxAxes[0].z;
            out.boxAxis1X = claim.boxAxes[1].x;
            out.boxAxis1Y = claim.boxAxes[1].y;
            out.boxAxis1Z = claim.boxAxes[1].z;
            out.boxAxis2X = claim.boxAxes[2].x;
            out.boxAxis2Y = claim.boxAxes[2].y;
            out.boxAxis2Z = claim.boxAxes[2].z;
        }
        MimitaNet::mpSendPacket(ctx, &batch, sizeof(batch));
        g_pendingMeleeClaims.erase(g_pendingMeleeClaims.begin(),
                                   g_pendingMeleeClaims.begin() + count);
    }
}

void tickClientMeleeContactClaims(Player& player,
                                  WeaponSystem& weapons,
                                  MimitaNet::MultiplayerContext& ctx,
                                  const MeleeMoveSet& moveSet,
                                  uint32_t simTick)
{
    if (!ctx.active || !ctx.gameplayActive || !ctx.localPlayerId)
        return;
    const WeaponDefinition* def = weapons.getCurrentDef(player);
    if (!def || !isClaimMeleeWeapon(*def))
    {
        g_lastClientMeleeShape = WeaponExecution::PhysicalContactShape{};
        g_lastClientMeleeWeapon.clear();
        return;
    }
    if (!g_state.active && g_lastClientMeleeShape.radius > 0.0f && g_pendingMeleeClaims.empty())
        return;
    if (simTick == g_lastMeleeContactSimTick)
        return;
    g_lastMeleeContactSimTick = simTick;

    const MeleeAction* action = g_state.active ? &g_state.actionSnapshot : nullptr;
    WeaponExecution::PhysicalContactShape shape;
    if (!clientMeleeShape(player, *def, action, shape))
        return;
    if (g_lastClientMeleeWeapon == def->id && g_lastClientMeleeShape.radius > 0.0f)
    {
        shape.previousA = g_lastClientMeleeShape.currentA;
        shape.previousB = g_lastClientMeleeShape.currentB;
    }
    g_lastClientMeleeShape = shape;
    g_lastClientMeleeWeapon = def->id;

    const bool actionActive = action && g_state.currentPhaseIndex == 1;
    float revMultiplier = 1.0f;
    if (def->id == "chainsaw" && action)
    {
        const float revMax = WeaponExecution::paramOr(*def, "revDamageMultiplier", 1.0f);
        const float revDecay = WeaponExecution::paramOr(*def, "revDecayTicks", 1.0f);
        if (revMax > 1.0f && revDecay > 0.0f)
        {
            const float t = glm::clamp((float)(simTick - g_state.startSimTick) / revDecay,
                                       0.0f, 1.0f);
            revMultiplier = 1.0f + (revMax - 1.0f) * (1.0f - t);
        }
    }

    const glm::vec3 blade = shape.currentB - shape.currentA;
    const float bladeLength = glm::length(blade);
    glm::vec3 bladeDir = bladeLength > 0.0001f ? blade / bladeLength : glm::vec3(0, 0, 1);
    glm::vec3 axis1 = glm::cross(bladeDir, glm::vec3(0, 0, 1));
    if (glm::length(axis1) < 0.001f)
        axis1 = glm::cross(bladeDir, glm::vec3(1, 0, 0));
    axis1 = glm::normalize(axis1);
    const glm::vec3 axis2 = glm::normalize(glm::cross(bladeDir, axis1));
    const glm::vec3 boxCenter = (shape.currentA + shape.currentB) * 0.5f;
    const glm::vec3 boxHalf(std::max(0.01f, bladeLength * 0.5f),
                            std::max(0.01f, shape.radius),
                            std::max(0.01f, shape.radius));
    const bool useCollisionBoxes = def->meleeContact.useWeaponCollisionHitbox;
    std::vector<ClientDamageBox> damageBoxes =
        useCollisionBoxes ? clientCollisionDamageBoxes(player, *def)
                          : std::vector<ClientDamageBox>{};
    const std::vector<ClientDamageBox> previousDamageBoxes =
        (g_lastClientDamageBoxWeapon == def->id &&
         g_lastClientDamageBoxes.size() == damageBoxes.size())
            ? g_lastClientDamageBoxes : damageBoxes;

    auto tryTarget = [&](const Player& remote, uint32_t id, bool npc, uint32_t generation) {
        if (remote.dead || remote.currentHp <= 0)
            return;
        Player& mutableRemote = const_cast<Player&>(remote);
        if (!mutableRemote.physicalBody.parts.empty())
            mutableRemote.updateModelWorldTransforms();
        WeaponExecution::PlayerTarget target = makeClientTarget(remote, id, npc, generation);
        WeaponExecution::PhysicalContactHit hit;
        glm::vec3 claimCenter = boxCenter;
        glm::vec3 claimHalf = boxHalf;
        glm::vec3 claimAxes[3] = {bladeDir, axis1, axis2};
        glm::vec3 volumeVelocity =
            (shape.currentB - shape.previousB) / (1.0f / 60.0f);
        bool hitFound = false;
        if (!damageBoxes.empty())
        {
            const glm::vec3 targetCenter = remote.pos + glm::vec3(0, 0, 0.9f);
            for (size_t boxIndex = 0; boxIndex < damageBoxes.size() && !hitFound;
                 ++boxIndex)
            {
                const ClientDamageBox& box = damageBoxes[boxIndex];
                const ClientDamageBox& previous = previousDamageBoxes[boxIndex];
                for (int step = 0; step < 4 && !hitFound; ++step)
                {
                    ClientDamageBox swept = box;
                    swept.center = glm::mix(previous.center, box.center,
                                            (float)step / 3.0f);
                    if (!target.bodyParts.empty())
                    {
                        for (const auto& part : target.bodyParts)
                        {
                            if (!obbSphereOverlap(swept, part.center,
                                                  glm::length(part.half) * 1.1f))
                                continue;
                            hitFound = true;
                            hit.hitPosition = part.center;
                            hit.normal = glm::length(part.center - swept.center) > 0.001f
                                ? glm::normalize(part.center - swept.center)
                                : glm::vec3(0, 0, 1);
                            break;
                        }
                    }
                    else if (obbSphereOverlap(swept, targetCenter, target.radius))
                    {
                        hitFound = true;
                        hit.hitPosition = targetCenter;
                        hit.normal = glm::length(targetCenter - swept.center) > 0.001f
                            ? glm::normalize(targetCenter - swept.center)
                            : glm::vec3(0, 0, 1);
                    }
                    if (hitFound)
                    {
                        claimCenter = swept.center;
                        claimHalf = box.half;
                        claimAxes[0] = box.axes[0];
                        claimAxes[1] = box.axes[1];
                        claimAxes[2] = box.axes[2];
                        volumeVelocity = (box.center - previous.center) /
                            (1.0f / 60.0f);
                    }
                }
            }
            if (!hitFound)
                return;
            hit.hit = true;
            hit.targetPlayerId = id;
            hit.targetSpawnGeneration = target.spawnGeneration;
        }
        else if (!WeaponExecution::testPhysicalContact(shape, target, hit))
        {
            // Lenient fallback for replicas whose limb boxes are missing or a
            // frame behind: treat the target root capsule as the contact body.
            const glm::vec3 targetCenter = remote.pos + glm::vec3(0, 0, 0.9f);
            const glm::vec3 ab = shape.currentB - shape.currentA;
            const float abLenSq = glm::dot(ab, ab);
            const float t = abLenSq > 0.000001f
                ? glm::clamp(glm::dot(targetCenter - shape.currentA, ab) / abLenSq,
                             0.0f, 1.0f)
                : 0.0f;
            const glm::vec3 closest = shape.currentA + ab * t;
            const float reach = glm::length(targetCenter - closest);
            if (reach > target.radius + shape.radius + 0.5f)
                return;
            hit.hit = true;
            hit.hitPosition = closest;
            hit.normal = glm::length(targetCenter - closest) > 0.001f
                ? glm::normalize(targetCenter - closest) : glm::vec3(0, 0, 1);
            hit.targetPlayerId = id;
            hit.targetSpawnGeneration = target.spawnGeneration;
        }
        const glm::vec3 relative = volumeVelocity - remote.vel;
        const float speed = glm::length(relative);
        const glm::vec3 toTarget = remote.pos - hit.hitPosition;
        const glm::vec3 normal = glm::length(toTarget) > 0.001f
            ? glm::normalize(toTarget) : glm::vec3(0, 0, 1);
        const float forceRaw = std::max(0.0f, glm::dot(relative, normal));
        const float maxForce = std::max(0.001f,
            action && action->contactMaxForceSpeed >= 0.0f
                ? action->contactMaxForceSpeed
                : WeaponExecution::paramOr(*def, "contactMaxForceSpeed", 22.0f));
        const float directness = speed > 0.001f
            ? glm::clamp(forceRaw / speed, 0.0f, 1.0f) : 0.0f;
        const float force = glm::clamp(speed / maxForce, 0.0f, 1.0f) * directness;
        const int damage = (int)clientContactDamage(*def, action, actionActive,
                                                    force, revMultiplier);

        MimitaNet::mpApplyPredictedDamage(ctx, id, damage, npc);

        HitEvent ev;
        ev.position = hit.hitPosition;
        ev.normal = normal;
        ev.direction = bladeDir;
        ev.hitEntity = true;
        ev.damage = damage;
        ev.attacker = player.username;
        ev.victim = remote.username;
        ev.weaponSource = def->id;
        HitEffects::onHit(ev);
        hitmarker(damage);

        PendingMeleeClaim claim;
        claim.targetId = id;
        claim.targetIsNpc = npc;
        claim.contactTick = ctx.latestServerTick != 0
            ? ctx.latestServerTick : ctx.latestLocalSnapshotTick;
        claim.contactId = g_nextMeleeContactId++;
        claim.hitPosition = hit.hitPosition;
        claim.direction = normal;
        claim.boxCenter = claimCenter;
        claim.boxHalf = claimHalf;
        claim.boxAxes[0] = claimAxes[0];
        claim.boxAxes[1] = claimAxes[1];
        claim.boxAxes[2] = claimAxes[2];
        claim.impactSpeed = speed;
        claim.impactForce = forceRaw;
        claim.impactDirectness = directness;
        claim.flags = damageBoxes.empty() ? 0 : 1;
        g_pendingMeleeClaims.push_back(claim);

        StructuredLogger::instance().writeEvent(
            StructuredCategory::Weapons, StructuredLevel::Important,
            "melee.client.contact_claim", def->id,
            "client physical blade contact claim queued", simTick,
            nlohmann::json{{"weapon", def->id},
                           {"target", id},
                           {"target_is_npc", npc},
                           {"target_generation", target.spawnGeneration},
                           {"contact_id", claim.contactId},
                           {"contact_tick", claim.contactTick},
                           {"damage_predicted", damage},
                           {"blade_base", {shape.currentA.x, shape.currentA.y,
                                           shape.currentA.z}},
                           {"blade_tip", {shape.currentB.x, shape.currentB.y,
                                          shape.currentB.z}},
                           {"blade_radius", shape.radius},
                           {"weapon_collision_hitbox", !damageBoxes.empty()},
                           {"box_center", {claimCenter.x, claimCenter.y, claimCenter.z}},
                           {"box_half", {claimHalf.x, claimHalf.y, claimHalf.z}},
                           {"hit", {hit.hitPosition.x, hit.hitPosition.y,
                                    hit.hitPosition.z}},
                           {"speed", speed},
                           {"force", force},
                           {"directness", directness}},
            __FILE__, __LINE__, __FUNCTION__);
    };

    for (const auto& entry : ctx.remotePlayers)
        tryTarget(entry.second, entry.first, false, entry.second.spawnGeneration);
    for (const auto& entry : ctx.remoteNpcs)
        tryTarget(entry.second, entry.first, true, entry.second.networkTransformEpoch);

    g_lastClientDamageBoxes = damageBoxes;
    g_lastClientDamageBoxWeapon = def->id;

    const uint32_t batchTicks = (uint32_t)std::max(1,
        def->meleeContact.claimBatchTicks);
    if (!g_pendingMeleeClaims.empty() &&
        simTick - g_lastMeleeClaimFlushTick >= batchTicks)
    {
        g_lastMeleeClaimFlushTick = simTick;
        flushMeleeContactClaims(ctx);
    }
}

void resetClientMeleeClaims()
{
    g_pendingMeleeClaims.clear();
    g_lastClientMeleeShape = WeaponExecution::PhysicalContactShape{};
    g_lastClientMeleeWeapon.clear();
    g_lastClientDamageBoxes.clear();
    g_lastClientDamageBoxWeapon.clear();
    g_lastMeleeContactSimTick = 0;
    g_lastMeleeClaimFlushTick = 0;
}

int actionRequestSlot(const WeaponDefinition& def, const MimitaNet::MultiplayerContext& ctx)
{
    if (ctx.communityWeaponSetId > 0) {
        auto& config = MimitaNet::CommunityServerConfig::instance();
        if (config.weaponSets().empty()) config.load();
        const int logical = config.slotForWeapon(ctx.communityWeaponSetId, def.id);
        if (logical > 0) return logical;
    }
    return def.slot;
}

glm::vec3 horizontalForward(const glm::vec3& raw)
{
    glm::vec3 forward(raw.x, raw.y, 0.0f);
    if (!std::isfinite(forward.x) || !std::isfinite(forward.y) ||
        glm::length(forward) < 0.001f)
        return glm::vec3(1.0f, 0.0f, 0.0f);
    return glm::normalize(forward);
}

AnimationMovementState movementState(const Player& player, bool movementPressed)
{
    const float speed = glm::length(glm::vec2(player.vel.x, player.vel.y));
    return resolveAnimationMovementState(speed, player.vel.z,
        player.ground.stableOnGround, player.crouching, movementPressed);
}

const MeleeMoveSet* equippedMoveSet(const Player& player, WeaponSystem& weapons)
{
    const WeaponDefinition* def = weapons.getCurrentDef(player);
    if (!def)
        return nullptr;
    return WeaponData::MeleeMoveSetRegistry::instance().get(def->id);
}

const MeleeAction* resolveAction(const MeleeMoveSet& moveSet,
                                 MeleeButton button,
                                 MeleeActorState state,
                                 MeleeModifier modifier)
{
    const MeleeAction* best = nullptr;
    for (const MeleeAction& a : moveSet.actions) {
        if (a.tapCount != 1) continue;
        if (a.button != button)
            continue;
        if (a.actorState != MeleeActorState::Any && a.actorState != state)
            continue;
        if (a.modifier != modifier)
            continue;
        if (!best || a.priority > best->priority)
            best = &a;
    }
    return best;
}

glm::vec3 impulseWorld(const glm::vec3& forward, const WeaponData::MeleeImpulse& imp)
{
    const glm::vec3 up(0.0f, 0.0f, 1.0f);
    glm::vec3 right = glm::cross(forward, up);
    if (glm::length(right) < 0.001f)
        right = glm::vec3(0.0f, 1.0f, 0.0f);
    right = glm::normalize(right);
    return forward * imp.forward + right * imp.right + up * imp.up;
}

// One Melee-move sound channel per actor: retriggering cuts the previous Melee
// sound on that actor and starts the new one from the beginning. Music and
// unrelated sounds continue. The action definition is the sole sound owner.
void playMeleeSound(uint32_t actorId, const std::string& name, float pitch, float volume,
                    const glm::vec3& position, const glm::vec3& offset)
{
    if (name.empty() || actorId == 0)
        return;
    AudioEvent event{name, AudioCategory::Impacts, true, position + offset,
                     volume, pitch, 40.0f, audioOwnerKey(actorId, "melee"),
                     0.0f, 0.0f, true};
    AudioManager::instance().play(event);
}

// Logs the world position of each hitbox-attached body part so limb motion is
// measurable in events.jsonl (spec: hitbox follows the animated part).
void logMeleeLimbPose(Player& player, const MeleeAction& action, const char* event,
                      const char* phase, uint32_t simTick)
{
    nlohmann::json parts = nlohmann::json::array();
    for (const WeaponData::MeleeHitbox& hb : action.hitboxes) {
        for (const PhysicalBodyPart& p : player.physicalBody.parts) {
            if (p.name != hb.attachedPart)
                continue;
            const glm::vec3 wp(p.worldTransform[3][0], p.worldTransform[3][1], p.worldTransform[3][2]);
            // Limb tip vertex (collider max) in world space — moves under rotation.
            const glm::vec3 localTip(
                (p.collider.localMin.x + p.collider.localMax.x) * 0.5f,
                (p.collider.localMin.y + p.collider.localMax.y) * 0.5f,
                p.collider.localMax.z);
            const glm::vec3 tip = glm::vec3(p.worldTransform * glm::vec4(localTip, 1.0f));
            const glm::vec3 rotDeg = glm::degrees(glm::eulerAngles(glm::quat_cast(glm::mat3(p.worldTransform))));
            auto it = g_lastPartPos.find(p.name);
            const glm::vec3 prev = it != g_lastPartPos.end() ? it->second : wp;
            const glm::vec3 delta = wp - prev;
            auto tipIt = g_lastPartTip.find(p.name);
            const glm::vec3 prevTip = tipIt != g_lastPartTip.end() ? tipIt->second : tip;
            const glm::vec3 tipDelta = tip - prevTip;
            g_lastPartPos[p.name] = wp;
            g_lastPartTip[p.name] = tip;
            parts.push_back({{"part", p.name},
                             {"world", {wp.x, wp.y, wp.z}},
                             {"pivot_delta", {delta.x, delta.y, delta.z}},
                             {"tip_world", {tip.x, tip.y, tip.z}},
                             {"tip_delta", {tipDelta.x, tipDelta.y, tipDelta.z}},
                             {"tip_moved_m", glm::length(tipDelta)},
                             {"world_rot_deg", {rotDeg.x, rotDeg.y, rotDeg.z}}});
        }
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Avatar, StructuredLevel::Important,
        event, action.id, "melee limb pose sample", simTick,
        {{"action", action.id}, {"phase", phase}, {"parts", parts}});
}

void startAction(Player& player,
                 MimitaNet::MultiplayerContext& ctx,
                 const WeaponDefinition& def,
                 const MeleeAction& action,
                 int actionIndex,
                 const glm::vec3& cameraFront,
                 uint32_t simTick,
                 bool movementPressed,
                 bool isPrimary,
                 const std::string& basePrimaryId,
                 const char* source)
{
    g_state.active = true;
    g_state.definitionId = def.id;
    g_state.actionId = action.id;
    g_state.actionSnapshot = action;
    g_state.startSimTick = simTick;
    g_state.startMovementState = movementState(player, movementPressed);
    g_state.startMovementPressed = movementPressed;
    g_state.startHp = player.currentHp;
    g_state.startPercent = player.percent;
    g_state.startHurtTimer = player.hurtTimer;
    g_state.startDashTimer = player.dashAnimTimer;
    g_state.startDownDashTimer = player.downDashAnimTimer;
    g_state.startKnockbackTimer = player.knockbackAnimTimer;
    g_state.startRespawnTimer = player.respawnAnimTimer;
    g_state.startFreezeActive = player.freeze.freezeActive;
    g_state.startDeathActive = player.authoredDeathActive;
    g_state.forward = horizontalForward(cameraFront);
    if (def.id == "chainsaw")
        g_lastChainsawRevTick = simTick;
    g_state.lastAdvanceSimTick = simTick;
    g_state.hasLastAdvance = true;
    g_state.lastImpulsePhaseTick = -1;
    g_state.lastImpulsePhaseIndex = -1;
    g_lastPartPos.clear();
    g_lastPartTip.clear();
    g_lastPhaseKey.clear();
    if (isPrimary) {
        g_state.lastPrimarySimTick = action.tapCount == 2 ? -1000000 : (int64_t)simTick;
        g_state.basePrimaryId = basePrimaryId;
    } else if (action.button == MeleeButton::Secondary) {
        g_state.lastSecondarySimTick = action.tapCount == 2 ? -1000000 : (int64_t)simTick;
        g_state.baseSecondaryId = basePrimaryId;
    }

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Avatar, StructuredLevel::Important,
        "melee.client.action", action.id, "melee action started locally", simTick,
        {{"action", action.id}, {"index", actionIndex}, {"source", source},
         {"weapon", def.id}, {"actor", ctx.localPlayerId}, {"actor_type", "player"},
         {"forward", {g_state.forward.x, g_state.forward.y, g_state.forward.z}},
         {"windup_ticks", action.windup.ticks},
         {"active_ticks", action.active.ticks},
         {"recovery_ticks", action.recovery.ticks},
         {"movement_state", animationMovementStateName(g_state.startMovementState)}});
    logMeleeLimbPose(player, action, "melee.client.limb", "start", simTick);

    // Send the intent to the server through the generic AttackRequest path. The
    // variant carries the action index; the reserved bytes carry the move-set
    // revision so the server can align revisions before accepting the action.
    const uint16_t netId = MimitaNet::weaponDefNetworkIdFor(def.id);
    uint16_t revision = 0;
    if (const MeleeMoveSet* ms = WeaponData::MeleeMoveSetRegistry::instance().get(def.id))
        revision = (uint16_t)(ms->revision & 0xffff);
    if (netId != 0) {
        MimitaNet::mpSendAttackRequest(
            ctx, netId, (int16_t)actionRequestSlot(def, ctx),
            player.pos, g_state.forward, player.pos,
            (uint8_t)std::clamp(actionIndex, 0, 255),
            0u, glm::vec3(0.0f), 0, revision);
    }

    // Action start sound plays at action tick 0 and cuts the prior Melee sound.
    playMeleeSound(ctx.localPlayerId, action.onStartSound, action.onStartPitch,
                   action.onStartVolume, player.pos, action.onStartOffset);
}

void endAction(Player& player)
{
    g_state.active = false;
    g_state.actionId.clear();
    player.weaponPoseActionState.clear();
    player.clearOneShot();
    player.proceduralMeleePoseActive = false;
}

void cancelAction(Player& player, MimitaNet::MultiplayerContext& ctx,
                  WeaponSystem& weapons, uint32_t simTick,
                  const char* reason, const char* nextState)
{
    if (!g_state.active) return;
    const std::string actionId = g_state.actionId;
    const AnimationMovementState previousState = g_state.startMovementState;
    const WeaponDefinition* def = weapons.getCurrentDef(player);
    const uint16_t netId = def ? MimitaNet::weaponDefNetworkIdFor(def->id) : 0;
    if (ctx.active && ctx.gameplayActive && def && netId != 0) {
        uint16_t revision = 0;
        if (const MeleeMoveSet* moveSet =
                WeaponData::MeleeMoveSetRegistry::instance().get(def->id))
            revision = (uint16_t)(moveSet->revision & 0xffff);
        MimitaNet::mpSendAttackRequest(ctx, netId,
            (int16_t)actionRequestSlot(*def, ctx), player.pos,
            glm::vec3(1.0f, 0.0f, 0.0f), player.pos,
            255, 0u, glm::vec3(0.0f), 0, revision);
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Weapons, StructuredLevel::Important,
        "melee.client.action.cancel", actionId,
        "a newer actor state interrupted the active melee action", simTick,
        {{"action", actionId},
         {"from_movement_state", animationMovementStateName(previousState)},
         {"to_state", nextState}, {"reason", reason},
         {"server_cancel_requested", ctx.active && ctx.gameplayActive && netId != 0}});
    endAction(player);
}

} // namespace

bool hasEquippedMoveSet(const Player& player, WeaponSystem& weapons)
{
    return equippedMoveSet(player, weapons) != nullptr;
}

bool shieldHeld()
{
    return g_shieldHeld;
}

void reset()
{
    g_state = ClientActionState{};
    resetClientMeleeClaims();
    g_shieldHeld = false;
    g_debugShield = false;
    g_forceActionId.clear();
    g_lastChainsawRevTick = 0;
    g_pendingInput = ClientInput{};
    g_primaryHeld = g_secondaryHeld = false;
    g_lastPartPos.clear();
    g_lastPartTip.clear();
    g_lastPhaseKey.clear();
}

void drawActiveHitbox(const Player& player, const Camera& camera)
{
    if (!g_state.active || g_state.currentPhaseIndex != 1 ||
        g_state.currentPhaseTick < 0)
        return;

    const MeleeAction& action = g_state.actionSnapshot;
    for (const WeaponData::MeleeHitbox& authoredHitbox : action.hitboxes) {
        if (g_state.currentPhaseTick < authoredHitbox.activeStartTick ||
            g_state.currentPhaseTick > authoredHitbox.activeEndTick)
            continue;
        const WeaponData::MeleeHitbox hitbox =
            WeaponData::resolveMeleeHitboxForTick(authoredHitbox, g_state.currentPhaseTick);
        if (hitbox.shape != "sphere") continue;

        for (const PhysicalBodyPart& part : player.physicalBody.parts) {
            if (part.name != hitbox.attachedPart)
                continue;
            const glm::vec3 center = glm::vec3(
                part.worldTransform * glm::vec4(hitbox.localOffset, 1.0f));
            const float radius = std::max(0.01f, hitbox.size.x);
            const glm::vec4 color{1.0f, 0.0f, 0.0f, 0.5f};
            DebugVis::drawFilledSphere(camera, center, radius, color);
            DebugVis::drawWireSphere(camera, center, radius, {1.0f, 0.0f, 0.0f, 0.9f});
            break;
        }
    }
}

void debugRequestAction(const std::string& actionId)
{
    g_forceActionId = actionId;
}

void debugSetCrouch(bool crouch)
{
    g_debugCrouch = crouch;
}

bool debugCrouchHeld()
{
    return g_debugCrouch;
}

void debugSetShield(bool shield)
{
    g_debugShield = shield;
}

bool debugShieldHeld()
{
    return g_debugShield;
}

void requestButton(bool secondary, bool held)
{
    bool& previous = secondary ? g_secondaryHeld : g_primaryHeld;
    if (held && !previous) {
        if (secondary) g_pendingInput.secondaryPressed = true;
        else g_pendingInput.primaryPressed = true;
    }
    previous = held;
}

void clientTick(Player& player,
                const Camera& camera,
                WeaponSystem& weapons,
                MimitaNet::MultiplayerContext& ctx,
                const ClientInput& rawInput,
                uint32_t simTick)
{
    ClientInput input = rawInput;
    input.primaryPressed = input.primaryPressed || g_pendingInput.primaryPressed;
    input.secondaryPressed = input.secondaryPressed || g_pendingInput.secondaryPressed;
    input.secondaryHeld = input.secondaryHeld || g_secondaryHeld;
    g_pendingInput = ClientInput{};
    const MeleeMoveSet* moveSet = equippedMoveSet(player, weapons);
    if (!moveSet || player.dead) {
        if (g_state.active)
            endAction(player);
        g_shieldHeld = false;
        g_state = ClientActionState{};
        return;
    }
    if (!g_state.definitionId.empty() && g_state.definitionId != moveSet->definitionId) {
        endAction(player);
        g_state = ClientActionState{};
        resetClientMeleeClaims();
    }
    g_shieldHeld = !moveSet->animationOnly && (input.shieldHeld || g_debugShield);
    // Publish crouch intent so the animation state machine can select the
    // authored `crouch` state clip on the grounded fighter.
    player.crouching = input.crouchHeld || g_debugCrouch;

    if (const WeaponDefinition* equipped = weapons.getCurrentDef(player)) {
        if (equipped->id == "chainsaw") {
            float rev = 1.0f;
            const float decayTicks = std::max(1.0f,
                equipped->customParams.count("revDecayTicks")
                    ? equipped->customParams.at("revDecayTicks") : 60.0f);
            const float maxRev = std::max(1.0f,
                equipped->customParams.count("revDamageMultiplier")
                    ? equipped->customParams.at("revDamageMultiplier") : 2.0f);
            if (g_lastChainsawRevTick != 0 && simTick >= g_lastChainsawRevTick) {
                const float t = std::clamp(
                    (float)(simTick - g_lastChainsawRevTick) / decayTicks, 0.0f, 1.0f);
                rev = 1.0f + (maxRev - 1.0f) * (1.0f - t);
            }
            auto rtIt = player.weaponRuntimes.find(equipped->id);
            if (rtIt != player.weaponRuntimes.end())
                rtIt->second.customFloats["chainsawRevMultiplier"] = rev;
        }
    }

    const MeleeActorState state = player.ground.stableOnGround
        ? MeleeActorState::Grounded : MeleeActorState::Airborne;
    const MeleeModifier modifier = moveSet->animationOnly ? MeleeModifier::None : input.crouchHeld
        ? MeleeModifier::Crouch
        : (input.jumpHeld ? MeleeModifier::Jump : MeleeModifier::None);

    // A held shield suppresses attacks until released (Phase 5 owns the shield).
    const bool wantsAttack = !g_shieldHeld &&
        (input.primaryPressed || input.secondaryPressed || input.grabPressed);

    // Test/dev forced action (agent.melee): start the named action directly.
    if (!g_forceActionId.empty()) {
        const std::string forcedId = g_forceActionId;
        g_forceActionId.clear();
        if (const MeleeAction* forced = moveSet->find(forcedId)) {
            int index = 0;
            for (size_t i = 0; i < moveSet->actions.size(); ++i)
                if (moveSet->actions[i].id == forced->id) { index = (int)i; break; }
            const WeaponDefinition* def = weapons.getCurrentDef(player);
            if (def)
                startAction(player, ctx, *def, *forced, index, camera.front, simTick,
                            input.movementPressed,
                            false, forcedId, "agent");
        }
    }

    if (wantsAttack) {
        MeleeButton button = MeleeButton::Primary;
        bool isPrimary = false;
        if (input.secondaryPressed)
            button = MeleeButton::Secondary;
        else if (input.grabPressed)
            button = MeleeButton::Grab;
        else
            isPrimary = true;

        const MeleeAction* action = resolveAction(*moveSet, button, state, modifier);
        std::string baseId = action ? action->id : std::string();

        // Both buttons use this shared fixed-tick double-click resolver.
        const int64_t lastPress = isPrimary ? g_state.lastPrimarySimTick : g_state.lastSecondarySimTick;
        const std::string& previousBase = isPrimary ? g_state.basePrimaryId : g_state.baseSecondaryId;
        if (action && !action->doubleClickAction.empty() && lastPress >= 0 &&
            (int64_t)simTick - lastPress <= action->doubleClickWindowTicks &&
            previousBase == baseId) {
            if (const MeleeAction* dbl = moveSet->find(action->doubleClickAction))
                action = dbl;
        }

        if (action) {
            int index = 0;
            for (size_t i = 0; i < moveSet->actions.size(); ++i)
                if (moveSet->actions[i].id == action->id) { index = (int)i; break; }
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Avatar, StructuredLevel::Important,
                "melee.client.input", action->id, "melee input resolved", simTick,
                {{"button", (int)button}, {"modifier", (int)modifier},
                 {"grounded", player.ground.stableOnGround},
                 {"on_ground", player.ground.onGround},
                 {"stable_on_ground", player.ground.stableOnGround},
                 {"action", action->id},
                 {"tap_count", action->tapCount}, {"secondary_held", input.secondaryHeld},
                 {"animation_only", moveSet->animationOnly},
                 {"jump_held", input.jumpHeld}, {"crouch_held", input.crouchHeld},
                 {"shield_held", g_shieldHeld}});
            const WeaponDefinition* def = weapons.getCurrentDef(player);
            if (def)
                startAction(player, ctx, *def, *action, index, camera.front, simTick,
                            input.movementPressed,
                            isPrimary, baseId, "input");
        }
    }

    // Held shield: cancel any attack, hold the shield pose (Phase 5 server owns
    // the durability/decay/break; the client only predicts the pose).
    if (g_shieldHeld) {
        if (g_state.active)
            cancelAction(player, ctx, weapons, simTick, "shield_raised", "shield");
        if (const MeleeAction* shieldAction = moveSet->find("shield")) {
            if (!shieldAction->animationClipId.empty()) {
                // A held shield stays on the clip's authored active pose for as
                // long as Q is held; its timeline still owns the boundary.
                player.playOneShotAtTick(shieldAction->animationClipId,
                                         shieldAction->windup.ticks);
            } else {
                const std::string& clip = !shieldAction->windup.clipId.empty()
                    ? shieldAction->windup.clipId : shieldAction->active.clipId;
                if (!clip.empty())
                    player.playOneShotPhase(clip, 0, 1);
            }
        }
        return;
    }

    if (g_state.active) {
        const AnimationMovementState currentState = movementState(player, input.movementPressed);
        const char* reason = nullptr;
        const char* nextState = animationMovementStateName(currentState);
        if (player.dead || player.authoredDeathActive) {
            reason = "death";
            nextState = "death";
        } else if (player.currentHp < g_state.startHp ||
                   player.percent > g_state.startPercent + 0.001f ||
                   (player.hurtTimer > g_state.startHurtTimer + 0.02f)) {
            reason = "hurt";
            nextState = "hurt";
        } else if (player.dashAnimTimer > g_state.startDashTimer + 0.02f ||
                   player.dash.didDash) {
            reason = "dash";
            nextState = "dash";
        } else if (player.downDashAnimTimer > g_state.startDownDashTimer + 0.02f ||
                   player.dash.didDownDash) {
            reason = "down_dash";
            nextState = "downDash";
        } else if ((player.freeze.freezeActive && !g_state.startFreezeActive) ||
                   player.freeze.didFreeze) {
            reason = "freeze";
            nextState = "freeze";
        } else if (player.knockbackAnimTimer > g_state.startKnockbackTimer + 0.02f) {
            reason = "knockback";
            nextState = "knockback";
        } else if (player.respawnAnimTimer > g_state.startRespawnTimer + 0.02f) {
            reason = "respawn";
            nextState = "respawn";
        } else if (player.jump.didGroundJump || player.jump.didAirJump) {
            reason = "jump";
            nextState = "jump";
        } else if (input.movementPressed && !g_state.startMovementPressed) {
            // A new movement input cancels the attack. Continuing movement that
            // was already held when the attack started, or crossing a walk/run
            // speed threshold, must not overwrite the swing mid-action.
            reason = "movement_started";
            nextState = animationMovementStateName(currentState);
        }
        if (reason) {
            cancelAction(player, ctx, weapons, simTick, reason, nextState);
            return;
        }
    }

    if (!g_state.active)
        return;

    // Use the snapshot taken at start so a mid-action hot reload does not change
    // this action's phases.
    const MeleeAction* action = &g_state.actionSnapshot;
    if (action->holdWhileButton && action->button == MeleeButton::Secondary &&
        !input.secondaryHeld && g_forceActionId.empty()) {
        StructuredLogger::instance().writeEvent(StructuredCategory::Weapons, StructuredLevel::Important,
            "weapon.action.release", action->id, "held weapon pose released", simTick,
            {{"weapon", moveSet->definitionId}, {"action", action->id}, {"blocking", false}});
        const WeaponDefinition* def = weapons.getCurrentDef(player);
        if (def && moveSet->animationOnly && ctx.active && ctx.gameplayActive)
            MimitaNet::mpSendAttackRequest(ctx, MimitaNet::weaponDefNetworkIdFor(def->id),
                (int16_t)actionRequestSlot(*def, ctx), player.pos, camera.front, player.pos,
                255, 0u, glm::vec3(0), 0, (uint16_t)(moveSet->revision & 0xffff));
        endAction(player);
        return;
    }

    // Advance the phase machine only on a new fixed tick.
    if (g_state.hasLastAdvance && simTick == g_state.lastAdvanceSimTick) {
        // Same tick: still sample the current pose so the first frame shows it.
    } else {
        g_state.hasLastAdvance = true;
        g_state.lastAdvanceSimTick = simTick;
    }

    const int windup = action->windup.ticks;
    const int active = action->active.ticks;
    const int recovery = action->recovery.ticks;
    const int total = action->totalTicks();
    const int elapsed = action->holdWhileButton
        ? std::min((int)(simTick - g_state.startSimTick), total - 1)
        : (int)(simTick - g_state.startSimTick);
    if (elapsed >= total) {
        endAction(player);
        return;
    }

    int phaseIndex = 0;          // 0 windup, 1 active, 2 recovery
    int phaseTick = elapsed;
    int phaseTicks = windup;
    std::string clip = action->animationClipId.empty()
        ? action->windup.clipId : action->animationClipId;
    const char* phaseName = "windup";
    if (elapsed >= windup + active) {
        phaseIndex = 2;
        phaseTick = elapsed - windup - active;
        phaseTicks = recovery;
        if (action->animationClipId.empty()) clip = action->recovery.clipId;
        phaseName = "recovery";
    } else if (elapsed >= windup) {
        phaseIndex = 1;
        phaseTick = elapsed - windup;
        phaseTicks = active;
        if (action->animationClipId.empty()) clip = action->active.clipId;
        phaseName = "active";
    }
    g_state.currentPhaseIndex = phaseIndex;
    g_state.currentPhaseTick = phaseTick;

    if (moveSet->definitionId == "swordsword" ||
        moveSet->definitionId == "chainsaw") {
        player.weaponPoseActionState = action->id;
    } else if (moveSet->definitionId == "mace") {
        player.weaponPoseActionState = action->id + "_" + phaseName;
    } else {
        player.weaponPoseActionState.clear();
    }

    if (!action->animationClipId.empty()) {
        player.playOneShotAtTick(action->animationClipId, elapsed);
        player.proceduralMeleePoseActive = false;
    } else if (!clip.empty()) {
        player.playOneShotPhase(clip, phaseTick, phaseTicks);
        player.proceduralMeleePoseActive = false;
    } else {
        WeaponData::MeleeProceduralPose pose;
        if (WeaponData::resolveMeleeProceduralPose(*action, phaseName,
                phaseTick, phaseTicks, pose)) {
            player.proceduralMeleeRightArmRotation = pose.rightArmRotation;
            player.proceduralMeleeRightArmTranslation = pose.rightArmTranslation;
            player.proceduralMeleeTorsoRotation = pose.torsoRotation;
            player.proceduralMeleePoseActive = true;
        } else {
            player.proceduralMeleePoseActive = false;
        }
    }

    // Log each phase transition with the limb pose so animation motion and the
    // attached hitbox path are verifiable in events.jsonl.
    {
        const std::string phaseKey =
            action->id + ":" + std::to_string(phaseIndex);
        if (phaseKey != g_lastPhaseKey) {
            g_lastPhaseKey = phaseKey;
            nlohmann::json phaseData = {
                {"action", action->id}, {"phase", phaseName},
                {"phase_index", phaseIndex}, {"phase_tick", phaseTick},
                {"phase_ticks", phaseTicks}, {"clip", clip}};
            if (!action->animationClipId.empty()) {
                const int sourceStart = action->animationSourceTicks[phaseIndex];
                const int sourceEnd = action->animationSourceTicks[phaseIndex + 1];
                phaseData["runtime_clip_start_tick"] = elapsed - phaseTick;
                phaseData["source_clip_start_tick"] = sourceStart;
                phaseData["source_clip_end_tick"] = sourceEnd;
                phaseData["source_clip_span_ticks"] = sourceEnd - sourceStart;
                phaseData["phase_speed_ratio"] = phaseTicks > 0
                    ? (float)(sourceEnd - sourceStart) / (float)phaseTicks : 0.0f;
            }
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Avatar, StructuredLevel::Important,
                "melee.client.phase", action->id, "melee phase entered", simTick,
                phaseData);
            logMeleeLimbPose(player, *action, "melee.client.limb", phaseName, simTick);
        }
    }

    // Phase impulses: fire each authored event once, when its phase-local tick
    // is reached or passed.
    for (const WeaponData::MeleeImpulse& imp : action->impulses) {
        if (imp.phase != phaseName)
            continue;
        if (imp.tick > phaseTick)
            continue;
        // Only fire events reached since the previously processed phase tick.
        const bool samePhase = g_state.lastImpulsePhaseIndex == phaseIndex;
        if (samePhase && imp.tick <= g_state.lastImpulsePhaseTick)
            continue;
        const glm::vec3 impulse = impulseWorld(g_state.forward, imp);
        if (glm::length(impulse) > 0.001f)
            player.externalImpulse += impulse;
    }

    // Phase audio events: play once when the phase-local tick is reached.
    for (const WeaponData::MeleeAudioEvent& ev : action->audioEvents) {
        if (ev.phase != phaseName || ev.tick > phaseTick)
            continue;
        const bool samePhase = g_state.lastImpulsePhaseIndex == phaseIndex;
        if (samePhase && ev.tick <= g_state.lastImpulsePhaseTick)
            continue;
        playMeleeSound(ctx.localPlayerId, ev.soundId, ev.pitch, ev.volume,
                       player.pos, ev.offset);
    }

    g_state.lastImpulsePhaseIndex = phaseIndex;
    g_state.lastImpulsePhaseTick = phaseTick;
    tickClientMeleeContactClaims(player, weapons, ctx, *moveSet, simTick);
}

bool currentActionActive()
{
    return g_state.active;
}

const std::string& currentActionDefinitionId()
{
    return g_state.definitionId;
}

const std::string& currentActionId()
{
    return g_state.actionId;
}

uint32_t currentActionStartTick()
{
    return g_state.startSimTick;
}

int currentActionPhaseTick()
{
    return g_state.currentPhaseTick;
}

uint32_t currentActionRevision()
{
    const auto* set = WeaponData::MeleeMoveSetRegistry::instance().get(g_state.definitionId);
    return set ? set->revision : 0;
}

} // namespace MimitaMelee
