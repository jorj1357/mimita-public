// 2026-09-27
/* purpose
* Implement the one generic moving physical entity and its small owner, plus the
* actor-vs-entity contact pass that reuses the single actor-triangle contact
* routine. Proves moving-support carry and velocity inheritance deterministically.
* Does NOT render, play effects, send packets, or own gameplay rules.
* Does NOT delete or replace any existing collision owner.
*/

#include "physics/physical-entity.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glad/glad.h>

#include "physics/config.h"
#include "physics/movement/actor-triangle-solver.h"
#include "physics/movement/collision-aabb-tree.h"
#include "physics/movement/physics-collision-shared.h"
#include "config/material-config.h"
#include "debug/crash-handler.h"
#include "impact/destructible-geometry.h"
#include "impact/destructible-render.h"
#include "impact/destructible-world-config.h"
#include "impact/impact-system.h"
#include "world/world.h"
#include "entities/player.h"
#include "map/map-loader-collision.h"
#include "debug/debug-visuals.h"
#include "camera.h"
#include "renderer/renderer.h"
#include "render/render-world.h"
#include "world/texture-store.h"
#include "map/map_common.h"

extern Renderer* gRenderer;

namespace {

AABB entityWorldAABB(const PhysicalEntity& e);
bool aabbOverlapsPadded(const AABB& a, const AABB& b, float pad);
void resolveEntityContacts(std::vector<PhysicalEntity>& entities);

void refreshMassProperties(PhysicalEntity& e)
{
    // Once material has been removed, the box formulas no longer describe the
    // body. Mass, center of mass, and inertia come from the cached cut surface
    // (integrated once per geometry revision by DestructibleGeometrySystem), so
    // this per-tick path stays a read, not an integration.
    const MimitaImpact::DestructibleGeometry& d = e.destructible;
    if (d.enabled && d.remainingVolume > 0.0001f &&
        (d.geometryRevision > 0 || d.massFromMesh))
    {
        e.mass = std::max(e.density * d.remainingVolume, 0.0001f);
        e.centerOfMass = d.massCenterOfMass;
        e.inertia = (e.mass / d.remainingVolume) * d.unitInertiaDiagonal;
        e.inertia = glm::max(e.inertia, glm::vec3(0.0001f));
        e.inverseInertia = 1.0f / e.inertia;
        return;
    }

    e.centerOfMass = glm::vec3(0.0f);
    const glm::vec3 dimensions = glm::max(e.halfExtents * 2.0f,
                                          glm::vec3(0.001f));
    const float m = std::max(e.mass, 0.0001f);
    e.inertia = glm::vec3(
        m * (dimensions.y * dimensions.y + dimensions.z * dimensions.z) / 12.0f,
        m * (dimensions.x * dimensions.x + dimensions.z * dimensions.z) / 12.0f,
        m * (dimensions.x * dimensions.x + dimensions.y * dimensions.y) / 12.0f);
    e.inertia = glm::max(e.inertia, glm::vec3(0.0001f));
    e.inverseInertia = 1.0f / e.inertia;
}

glm::vec3 inverseInertiaWorld(const PhysicalEntity& e, const glm::vec3& v)
{
    const glm::vec3 local = glm::inverse(e.orientation) * v;
    return e.orientation * (local * e.inverseInertia);
}

void applyImpulseAtPoint(PhysicalEntity& e, const glm::vec3& impulse,
                         const glm::vec3& point, bool wake = true)
{
    if (e.motion != PhysicalEntityMotion::Dynamic || e.mass <= 0.0f)
        return;
    const glm::vec3 center = glm::vec3(e.transform[3]) + e.orientation * e.centerOfMass;
    e.velocity += impulse / e.mass;
    e.angularVelocity += inverseInertiaWorld(e, glm::cross(point - center, impulse));
    if (wake)
    {
        e.sleeping = false;
        e.sleepTicks = 0;
        e.sleepAnchorValid = false;
    }
}

void resolveWorldContactVelocity(PhysicalEntity& e,
                                 const RecoveryContact& contact)
{
    if (e.motion != PhysicalEntityMotion::Dynamic || e.mass <= 0.0f)
        return;
    glm::vec3 normal = contact.responseNormal;
    if (glm::dot(normal, normal) <= 1e-8f)
        normal = contact.normal;
    normal = glm::normalize(normal);

    const glm::vec3 center = glm::vec3(e.transform[3]) +
                             e.orientation * e.centerOfMass;
    const glm::vec3 r = contact.point - center;
    const glm::vec3 pointVelocity = e.velocity + glm::cross(e.angularVelocity, r);
    const glm::vec3 rCrossNormal = glm::cross(r, normal);
    const float normalMass = 1.0f / e.mass +
        glm::dot(glm::cross(inverseInertiaWorld(e, rCrossNormal), r), normal);
    const float normalSpeed = glm::dot(pointVelocity, normal);
    float normalImpulse = 0.0f;
    if (normalSpeed < 0.0f && normalMass > 1e-6f)
    {
        // Below the configured minimum bounce speed, do not bounce: the object
        // projects and settles instead of jittering on a curved/edge contact.
        const MimitaImpact::DestructibleWorldConfig& config =
            MimitaImpact::DestructibleWorldConfig::instance();
        const float restitution =
            std::fabs(normalSpeed) < config.objectMinBounceSpeed()
                ? 0.0f : e.restitution;
        normalImpulse = -(1.0f + restitution) * normalSpeed / normalMass;
        applyImpulseAtPoint(e, normal * normalImpulse, contact.point, false);
    }

    const glm::vec3 postNormalVelocity = e.velocity + glm::cross(e.angularVelocity, r);
    const glm::vec3 tangentVelocity = postNormalVelocity -
        normal * glm::dot(postNormalVelocity, normal);
    const float tangentLength = glm::length(tangentVelocity);
    if (tangentLength <= 1e-5f || normalMass <= 1e-6f)
        return;
    const glm::vec3 tangent = tangentVelocity / tangentLength;
    const glm::vec3 rCrossTangent = glm::cross(r, tangent);
    const float tangentMass = 1.0f / e.mass +
        glm::dot(glm::cross(inverseInertiaWorld(e, rCrossTangent), r), tangent);
    if (tangentMass <= 1e-6f)
        return;
    const float desiredTangentImpulse = -glm::dot(postNormalVelocity, tangent) /
                                        tangentMass;
    const float frictionLimit = e.friction * normalImpulse;
    const float tangentImpulse = normalImpulse > 0.0f
        ? glm::clamp(desiredTangentImpulse, -frictionLimit, frictionLimit)
        : desiredTangentImpulse;
    applyImpulseAtPoint(e, tangent * tangentImpulse, contact.point, false);
}

void rebuildTransformFromPose(PhysicalEntity& e)
{
    e.transform = glm::translate(glm::mat4(1.0f), glm::vec3(e.transform[3])) *
                  glm::mat4_cast(glm::normalize(e.orientation));
}

// Settling: an object that stays within sleepMoveThresholdMeters of its anchor
// for sleepRequiredTicks fixed ticks freezes there until disturbed (by an
// impulse, contact, or push). This replaces the previous special-case righting
// and velocity-kill so settling is a natural result of collisions plus this
// explicit, configurable "has not moved" rule. It never freezes a body that is
// still penetrating geometry.
void updateSettling(PhysicalEntity& e,
                    const MimitaImpact::DestructibleWorldConfig& config,
                    float maxPenetration)
{
    if (e.motion != PhysicalEntityMotion::Dynamic)
        return;
    const glm::vec3 position(e.transform[3]);
    const float threshold = std::max(0.0f, config.sleepMoveThresholdMeters());
    if (!e.sleepAnchorValid ||
        glm::length(position - e.sleepAnchorPos) > threshold)
    {
        e.sleepAnchorPos = position;
        e.sleepAnchorValid = true;
        e.sleepTicks = 0;
        return;
    }
    if (maxPenetration > 0.05f)
    {
        e.sleepTicks = 0;
        return;
    }
    if (e.sleepTicks < 0xFFFF)
        ++e.sleepTicks;
    if (e.sleepTicks >= config.sleepRequiredTicks())
    {
        e.velocity = glm::vec3(0.0f);
        e.angularVelocity = glm::vec3(0.0f);
        e.sleeping = true;
        e.sleepAnchorValid = false;
    }
}

} // namespace

void refreshEntityMassProperties(PhysicalEntity& e)
{
    refreshMassProperties(e);
}

void applyPhysicalEntityImpulse(PhysicalEntity& e, const glm::vec3& impulse,
                                const glm::vec3& worldPoint)
{
    applyImpulseAtPoint(e, impulse, worldPoint, true);
}

PhysicalEntitySystem& PhysicalEntitySystem::instance()
{
    static PhysicalEntitySystem system;
    return system;
}

void PhysicalEntitySystem::clear()
{
    for (PhysicalEntity& e : mEntities)
        MimitaImpact::DestructibleGeometrySystem::instance().release(e.destructible);
    mEntities.clear();
    mNextId = 1;
    mFixedAccumulator = 0.0;
    mSimulationTick = 0;
}

uint32_t PhysicalEntitySystem::add(
    const std::vector<CollisionTriangle>& localTriangles,
    const glm::mat4& transform,
    PhysicalEntityMotion motion,
    uint32_t materialId)
{
    PhysicalEntity e;
    e.id = mNextId++;
    e.localTriangles = localTriangles;
    e.transform = transform;
    e.previousTransform = transform;
    e.orientation = glm::normalize(glm::quat_cast(glm::mat3(transform)));
    e.motion = motion;
    e.shape = PhysicalEntityShape::TriangleMesh;
    e.materialId = materialId;
    e.persistenceId = "runtime-physical-" + std::to_string(e.id);
    e.networkId = e.id;
    // Default object material response comes from
    // config/destructible-world.json (physics) so bounce/friction are tunable.
    {
        const MimitaImpact::DestructibleWorldConfig& config =
            MimitaImpact::DestructibleWorldConfig::instance();
        e.restitution = config.objectRestitution();
        e.friction = config.objectFriction();
    }
    refreshMassProperties(e);
    recordCrashBreadcrumb("entity-add", "id=%u tris=%zu",
                          e.id, e.localTriangles.size());
    mEntities.push_back(std::move(e));
    return mEntities.back().id;
}

void PhysicalEntitySystem::beginTick()
{
    for (PhysicalEntity& e : mEntities)
        e.previousTransform = e.transform;
}

PhysicalEntity* PhysicalEntitySystem::moveKinematic(uint32_t id,
                                                    const glm::mat4& transform,
                                                    float dt)
{
    PhysicalEntity* e = find(id);
    if (!e)
        return nullptr;
    const glm::vec3 previous(e->transform[3]);
    const glm::vec3 next(transform[3]);
    if (dt > 0.0f)
        e->velocity = (next - previous) / dt;
    else
        e->velocity = glm::vec3(0.0f);
    e->transform = transform;
    e->orientation = glm::normalize(glm::quat_cast(glm::mat3(transform)));
    e->sleeping = false;
    e->sleepTicks = 0;
    e->sleepAnchorValid = false;
    return e;
}

PhysicalEntity* PhysicalEntitySystem::find(uint32_t id)
{
    for (PhysicalEntity& e : mEntities)
        if (e.id == id)
            return &e;
    return nullptr;
}

PhysicalEntity* PhysicalEntitySystem::findByNetworkId(uint32_t networkId)
{
    if (networkId == 0)
        return nullptr;
    for (PhysicalEntity& e : mEntities)
        if (e.serverDriven && e.networkId == networkId)
            return &e;
    return nullptr;
}

uint32_t PhysicalEntitySystem::addReplicated(
    uint32_t networkId,
    const std::vector<CollisionTriangle>& localTriangles,
    const glm::mat4& transform,
    PhysicalEntityMotion motion,
    uint32_t materialId)
{
    if (networkId == 0 || findByNetworkId(networkId))
        return 0;
    const uint32_t localId = add(localTriangles, transform, motion, materialId);
    if (PhysicalEntity* e = find(localId))
    {
        e->networkId = networkId;
        e->serverDriven = true;
        e->sleeping = false;
        e->sleepTicks = 0;
    }
    return localId;
}

bool PhysicalEntitySystem::removeByNetworkId(uint32_t networkId)
{
    PhysicalEntity* e = findByNetworkId(networkId);
    return e ? remove(e->id) : false;
}

bool PhysicalEntitySystem::remove(uint32_t id)
{
    for (auto it = mEntities.begin(); it != mEntities.end(); ++it)
    {
        if (it->id == id)
        {
            recordCrashBreadcrumb("entity-remove", "id=%u", id);
            MimitaImpact::DestructibleGeometrySystem::instance().release(it->destructible);
            releaseGeneratedEntityMesh(id);
            mEntities.erase(it);
            return true;
        }
    }
    return false;
}

void PhysicalEntitySystem::advanceKinematics(float dt, const World& world)
{
    if (dt <= 0.0f)
        return;
    const MimitaImpact::DestructibleWorldConfig& objectPhysics =
        MimitaImpact::DestructibleWorldConfig::instance();
    mFixedAccumulator = std::min(mFixedAccumulator + (double)dt, 0.25);
    constexpr double kFixedDt = 1.0 / 60.0;
    constexpr int kMaxSteps = 5;
    int steps = 0;
    while (mFixedAccumulator >= kFixedDt && steps++ < kMaxSteps)
    {
        mFixedAccumulator -= kFixedDt;
        ++mSimulationTick;
        for (PhysicalEntity& e : mEntities)
        {
            if (e.serverDriven ||
                ((e.motion != PhysicalEntityMotion::Kinematic &&
                  e.motion != PhysicalEntityMotion::Dynamic) || e.sleeping))
                continue;
            refreshMassProperties(e);
            if (e.motion == PhysicalEntityMotion::Dynamic)
            {
                // Object gravity/damping are config-driven
                // (config/destructible-world.json -> physics) so crate feel is
                // tweakable in game without a rebuild. Players keep PHYS.gravity.
                e.velocity.z = std::max(
                    e.velocity.z + objectPhysics.objectGravity() * e.gravityScale *
                                       (float)kFixedDt,
                    -MAX_FALL_SPEED);
                e.velocity *= std::max(
                    0.0f, 1.0f - objectPhysics.objectLinearDamping() * (float)kFixedDt);
                e.angularVelocity *= std::max(
                    0.0f, 1.0f - objectPhysics.objectAngularDamping() * (float)kFixedDt);
                const float angularSpeed = glm::length(e.angularVelocity);
                if (angularSpeed > 1e-5f)
                {
                    const glm::vec3 axis = e.angularVelocity / angularSpeed;
                    e.orientation = glm::normalize(
                        glm::angleAxis(angularSpeed * (float)kFixedDt, axis) *
                        e.orientation);
                }
                const float postDampingAngularSpeed = glm::length(e.angularVelocity);
                if (e.maxAngularSpeed > 0.0f &&
                    postDampingAngularSpeed > e.maxAngularSpeed)
                    e.angularVelocity *= e.maxAngularSpeed / postDampingAngularSpeed;
                // Configurable linear-speed cap so a collision glitch cannot
                // fling an object across the map.
                const float maxSpeed = objectPhysics.objectMaxSpeed();
                if (maxSpeed > 0.0f)
                {
                    const float speed = glm::length(e.velocity);
                    if (speed > maxSpeed)
                        e.velocity *= maxSpeed / speed;
                }
            }
            if (glm::dot(e.velocity, e.velocity) <= 1e-8f &&
                glm::dot(e.angularVelocity, e.angularVelocity) <= 1e-8f)
            {
                // Still run the freeze rule so a truly stationary body becomes
                // sleeping (and is then skipped) instead of staying awake.
                updateSettling(e, objectPhysics, 0.0f);
                continue;
            }
            e.previousTransform = e.transform;
            e.transform[3] += glm::vec4(e.velocity * (float)kFixedDt, 0.0f);
            rebuildTransformFromPose(e);

            // First Phase 3 slice: kinematic objects sweep their authored
            // local triangles against the static world and slide instead of
            // passing through it. Player/object contacts remain owned by the
            // actor manifold below.
            // Reused scratch: the object sweep must not allocate per entity per
            // collision pass inside the fixed tick (collision.md hard rule).
            static thread_local std::vector<ActorCollisionMesh> s_objectMeshes;
            static thread_local std::vector<int> s_objectCandidates;
            static thread_local std::vector<RecoveryContact> s_objectContacts;
            static thread_local AabbTree s_objectWorldTree;
            s_objectMeshes.resize(1);
            s_objectMeshes[0].label = "physicalEntity";
            s_objectMeshes[0].localTriangles = &e.localTriangles;
            s_objectMeshes[0].previousTransform = e.previousTransform;
            s_objectMeshes[0].desiredTransform = e.transform;
            s_objectContacts.clear();
            // The solver already does 6 internal relaxation passes; the outer
            // re-gather passes were the dominant multiplier for holey bodies
            // (thousands of triangles re-scanned per pass). One pass is enough
            // for the entity-vs-world correction and the self-tests.
            constexpr int kEntityCollisionPasses = 1;
            for (int collisionPass = 0;
                 collisionPass < kEntityCollisionPasses; ++collisionPass)
            {
                s_objectMeshes[0].previousTransform = e.previousTransform;
                s_objectMeshes[0].desiredTransform = e.transform;
                s_objectMeshes[0].localTriangles = &e.localTriangles;
                const AABB sweepBox = makeSweptActorMeshAABB(
                    s_objectMeshes, glm::vec3(0.0f));
                s_objectCandidates.clear();
                appendChunkTrianglesForAABB(world, sweepBox, 0.1f,
                                            s_objectCandidates,
                                            "physicalEntitySweep");
                if (s_objectCandidates.empty())
                    break;

                // Index this pass's candidates once so each body triangle only
                // tests the world triangles whose bounds it can touch, instead
                // of scanning the whole candidate list (the holey-crate cost).
                const AabbTree* worldTree = nullptr;
                if (world.collisionMesh.triangleAABBs.size() ==
                    world.collisionMesh.triangles.size())
                {
                    s_objectWorldTree.build(s_objectCandidates,
                                            world.collisionMesh.triangleAABBs);
                    worldTree = &s_objectWorldTree;
                }

                collectActorMeshContactsInto(
                    world, s_objectMeshes, s_objectCandidates,
                    glm::vec3(e.transform[3]), s_objectContacts, true, -1.0f,
                    worldTree);
                if (s_objectContacts.empty())
                    break;
                const glm::vec3 correction = solveBatchedCorrection(
                    s_objectContacts, 0.01f, nullptr, nullptr,
                    e.velocity * (float)kFixedDt, glm::vec3(e.transform[3]));
                e.transform[3] += glm::vec4(correction, 0.0f);
                for (const RecoveryContact& contact : s_objectContacts)
                {
                    if (e.motion == PhysicalEntityMotion::Dynamic)
                    {
                        resolveWorldContactVelocity(e, contact);
                        // The impulse solver handles angular and frictional
                        // response. Keep the final translational invariant as
                        // strict as the player solver: a supported body may
                        // never retain velocity into the supporting surface.
                        const float into = glm::dot(e.velocity, contact.normal);
                        if (into < 0.0f)
                            e.velocity -= contact.normal * into;
                        if (contact.normal.z > MAX_WALKABLE_SLOPE_DOT &&
                            std::fabs(e.velocity.z) < 0.25f)
                            e.velocity.z = 0.0f;
                    }
                    else
                    {
                        const float into = glm::dot(e.velocity, contact.normal);
                        if (into < 0.0f)
                            e.velocity -= contact.normal * into;
                    }
                }
                if (glm::dot(correction, correction) < 1e-8f)
                    break;
            }
            if (s_objectContacts.empty())
            {
                // No contacts this tick: the only remaining rule is the
                // configurable "has not moved" freeze (gravity keeps a falling
                // body moving, so it cannot freeze in mid-air).
                updateSettling(e, objectPhysics, 0.0f);
                continue;
            }

            // Collisions/gravity/contacts already produced the motion. The only
            // special rule is the configurable freeze when the body has not
            // moved beyond the threshold for N ticks. No artificial righting or
            // velocity damping.
            float maxPenetration = 0.0f;
            for (const RecoveryContact& contact : s_objectContacts)
                maxPenetration = std::max(maxPenetration, contact.penetration);
            updateSettling(e, objectPhysics, maxPenetration);
        }
        resolveEntityContacts(mEntities);
    }
    if (steps >= kMaxSteps && mFixedAccumulator >= kFixedDt)
        mFixedAccumulator = 0.0;

    // Fragment lifecycle: age detached pieces, then remove ones that are too
    // small, too old, or beyond the total cap. All thresholds are config-driven
    // (config/destructible-world.json -> fragments).
    {
        const float elapsed = (float)steps * (float)kFixedDt;
        uint32_t fragmentCount = 0;
        std::vector<uint32_t> toRemove;
        for (PhysicalEntity& e : mEntities)
        {
            if (!e.isFragment)
                continue;
            e.fragmentAge += elapsed;
            ++fragmentCount;
            if (e.destructible.remainingVolume < objectPhysics.minFragmentVolume() ||
                (objectPhysics.fragmentLifetimeSeconds() > 0.0f &&
                 e.fragmentAge >= objectPhysics.fragmentLifetimeSeconds()))
                toRemove.push_back(e.id);
        }
        if (fragmentCount > objectPhysics.maxTotalFragments())
        {
            std::vector<const PhysicalEntity*> oldest;
            for (const PhysicalEntity& e : mEntities)
                if (e.isFragment)
                    oldest.push_back(&e);
            std::sort(oldest.begin(), oldest.end(),
                [](const PhysicalEntity* x, const PhysicalEntity* y) {
                    return x->fragmentAge > y->fragmentAge;
                });
            const uint32_t over = fragmentCount - objectPhysics.maxTotalFragments();
            for (uint32_t i = 0; i < over && i < oldest.size(); ++i)
                toRemove.push_back(oldest[i]->id);
        }
        std::sort(toRemove.begin(), toRemove.end());
        toRemove.erase(std::unique(toRemove.begin(), toRemove.end()), toRemove.end());
        for (uint32_t id : toRemove)
            remove(id);
    }

    // Drain queued destruction cuts once per fixed tick, batched and budgeted so
    // a burst of shots cannot blow a frame. This is the 60 Hz destruction owner;
    // ImpactSystem still decides the cut and owns fracture. Budgets come from
    // config/destructible-world.json (hot-reloadable).
    const MimitaImpact::DestructibleWorldConfig& destruction =
        MimitaImpact::DestructibleWorldConfig::instance();
    MimitaImpact::ImpactSystem::instance().flushPendingCuts(
        destruction.maxCutsPerEntityPerTick(), destruction.cutBudgetMsPerTick());
}

float PhysicalEntitySystem::renderAlpha() const
{
    constexpr double kFixedDt = 1.0 / 60.0;
    return static_cast<float>(std::clamp(mFixedAccumulator / kFixedDt, 0.0, 1.0));
}

void PhysicalEntitySystem::applyPlayerPush(const Player& player, float dt)
{
    if (dt <= 0.0f)
        return;
    const glm::vec3 playerMin = player.pos + glm::vec3(-PLAYER_RADIUS, -PLAYER_RADIUS, 0.0f);
    const glm::vec3 playerMax = player.pos + glm::vec3(PLAYER_RADIUS, PLAYER_RADIUS, PLAYER_HEIGHT);
    const AABB playerBox{playerMin, playerMax};
    // Include the external impulse so a dash into an object pushes it with the
    // speed/force at the moment of contact, not just the base walk velocity.
    glm::vec3 horizontalVelocity(player.vel.x + player.externalImpulse.x,
                                  player.vel.y + player.externalImpulse.y, 0.0f);
    if (glm::dot(horizontalVelocity, horizontalVelocity) <= 1e-6f &&
        glm::dot(player.inputWishMove, player.inputWishMove) > 1e-4f)
        horizontalVelocity = glm::vec3(player.inputWishMove.x,
                                       player.inputWishMove.y, 0.0f) *
                             MAX_PLAYER_MOVE_SPEED;
    if (glm::dot(horizontalVelocity, horizontalVelocity) <= 1e-6f)
        return;

    for (PhysicalEntity& e : mEntities)
    {
        if (e.serverDriven || e.motion != PhysicalEntityMotion::Dynamic ||
            e.mass <= 0.0f)
            continue;
        if (e.lastPlayerPushTick == mSimulationTick)
            continue;
        const AABB objectBox = entityWorldAABB(e);
        if (!aabbOverlapsPadded(playerBox, objectBox, 0.05f))
            continue;

        refreshMassProperties(e);
        glm::vec3 pushNormal = player.pos - glm::vec3(e.transform[3]);
        pushNormal.z = 0.0f;
        if (glm::dot(pushNormal, pushNormal) <= 1e-6f)
            pushNormal = -horizontalVelocity;
        pushNormal = glm::normalize(pushNormal);
        const float into = glm::dot(horizontalVelocity, pushNormal);
        if (into >= 0.0f)
            continue;
        const glm::vec3 contactPoint = glm::clamp(
            player.pos, objectBox.min, objectBox.max);
        constexpr float kPlayerMass = 80.0f;
        applyImpulseAtPoint(e, -pushNormal * (-into) * kPlayerMass * 0.9f,
                            contactPoint);
        e.lastPlayerPushTick = mSimulationTick;
        e.velocity.x = std::clamp(e.velocity.x, -MAX_PLAYER_MOVE_SPEED, MAX_PLAYER_MOVE_SPEED);
        e.velocity.y = std::clamp(e.velocity.y, -MAX_PLAYER_MOVE_SPEED, MAX_PLAYER_MOVE_SPEED);
    }
}

void PhysicalEntitySystem::applyPlayerContactPush(
    uint32_t entityId, const Player& player, const glm::vec3& point,
    const glm::vec3& contactNormal)
{
    PhysicalEntity* e = find(entityId);
    if (!e || e->serverDriven || e->motion != PhysicalEntityMotion::Dynamic ||
        e->mass <= 0.0f || e->lastPlayerPushTick == mSimulationTick)
        return;

    glm::vec3 pushNormal(contactNormal.x, contactNormal.y, 0.0f);
    if (glm::dot(pushNormal, pushNormal) <= 1e-6f)
        return;
    pushNormal = glm::normalize(pushNormal);
    glm::vec3 incoming = player.vel + player.externalImpulse;
    float into = std::max(0.0f, -glm::dot(incoming, pushNormal));
    if (into <= 1e-4f && glm::dot(player.inputWishMove, player.inputWishMove) > 1e-4f)
    {
        const glm::vec3 wish(player.inputWishMove.x, player.inputWishMove.y, 0.0f);
        into = std::max(0.0f, -glm::dot(glm::normalize(wish) * MAX_PLAYER_MOVE_SPEED,
                                        pushNormal));
    }
    if (into <= 1e-4f)
        return;

    constexpr float kPlayerMass = 80.0f;
    applyImpulseAtPoint(*e, -pushNormal * into * kPlayerMass * 0.9f, point);
    e->lastPlayerPushTick = mSimulationTick;
}

void buildBoxCollisionTriangles(std::vector<CollisionTriangle>& out,
                                const glm::vec3& center,
                                const glm::vec3& half)
{
    const glm::vec3 mn = center - half;
    const glm::vec3 mx = center + half;
    const glm::vec3 v[8] = {
        mn,
        {mx.x, mn.y, mn.z},
        {mx.x, mx.y, mn.z},
        {mn.x, mx.y, mn.z},
        {mn.x, mn.y, mx.z},
        {mx.x, mn.y, mx.z},
        {mx.x, mx.y, mx.z},
        {mn.x, mx.y, mx.z},
    };
    const int quads[6][4] = {
        {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
        {2, 3, 7, 6}, {1, 2, 6, 5}, {3, 0, 4, 7}
    };
    for (int q = 0; q < 6; ++q)
    {
        const int* ix = quads[q];
        auto push = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
            glm::vec3 n = glm::cross(b - a, c - a);
            const float len = glm::length(n);
            if (len < 1e-6f)
                return;
            CollisionTriangle t;
            t.a = a; t.b = b; t.c = c; t.normal = n / len;
            out.push_back(t);
        };
        push(v[ix[0]], v[ix[1]], v[ix[2]]);
        push(v[ix[0]], v[ix[2]], v[ix[3]]);
    }
}

namespace {

AABB entityWorldAABB(const PhysicalEntity& e)
{
    AABB box;
    // Destructible geometry is always contained in the source box, so the world
    // bound is a transform of ±halfExtents — no per-triangle scan in a hot loop.
    if (e.destructible.enabled)
    {
        box.min = glm::vec3(1e30f);
        box.max = glm::vec3(-1e30f);
        const glm::vec3 he = e.halfExtents;
        for (int c = 0; c < 8; ++c)
        {
            const glm::vec3 corner((c & 1) ? he.x : -he.x,
                                   (c & 2) ? he.y : -he.y,
                                   (c & 4) ? he.z : -he.z);
            const glm::vec3 w = glm::vec3(e.transform * glm::vec4(corner, 1.0f));
            box.min = glm::min(box.min, w);
            box.max = glm::max(box.max, w);
        }
        return box;
    }
    box.min = glm::vec3(1e30f);
    box.max = glm::vec3(-1e30f);
    for (const CollisionTriangle& t : e.localTriangles)
        for (const glm::vec3& v : {t.a, t.b, t.c})
        {
            const glm::vec3 w = glm::vec3(e.transform * glm::vec4(v, 1.0f));
            box.min = glm::min(box.min, w);
            box.max = glm::max(box.max, w);
        }
    return box;
}

bool aabbOverlapsPadded(const AABB& a, const AABB& b, float pad)
{
    return a.min.x - pad <= b.max.x && a.max.x + pad >= b.min.x &&
           a.min.y - pad <= b.max.y && a.max.y + pad >= b.min.y &&
           a.min.z - pad <= b.max.z && a.max.z + pad >= b.min.z;
}

bool entityCanMove(const PhysicalEntity& e)
{
    // A client mirror follows server transforms and must never be pushed by
    // local entity-vs-entity resolution.
    return !e.serverDriven && e.collidesWithActors &&
           e.motion != PhysicalEntityMotion::Static &&
           !e.localTriangles.empty();
}

// Collects contacts for `actor` vs `other` using the shared actor-vs-mesh
// narrowphase (the same routine the actor/world path uses). `other`'s cached
// world surface is swapped into a scratch World so no triangle soup is copied.
void collectPairDirection(
    const PhysicalEntity& actor, const PhysicalEntity& other,
    World& scratchWorld, std::vector<int>& candidates,
    std::vector<ActorCollisionMesh>& meshes, std::vector<RecoveryContact>& out)
{
    // Fetch the tree inside this call: cachedEntitySurface may rebuild the map
    // entry and a pointer captured earlier could dangle.
    const EntitySurfaceCacheView view = cachedEntitySurface(other);
    const AabbTree* otherTree = view.tree;
    std::swap(scratchWorld.collisionMesh, *view.meshCache);

    meshes.resize(1);
    meshes[0].label = "physicalEntity";
    meshes[0].localTriangles = &actor.localTriangles;
    meshes[0].previousTransform = actor.previousTransform;
    meshes[0].desiredTransform = actor.transform;

    candidates.resize(scratchWorld.collisionMesh.triangles.size());
    for (size_t k = 0; k < candidates.size(); ++k)
        candidates[k] = (int)k;

    // contactSkin < 0 enables the rounded feature shell (point->sphere,
    // line->capsule, face->triangle), so edges/vertices are thickened and a
    // moving body cannot slip through a corner or a curved surface.
    collectActorMeshContactsInto(scratchWorld, meshes, candidates,
                                 glm::vec3(actor.transform[3]), out,
                                 false, -1.0f, otherTree);

    std::swap(scratchWorld.collisionMesh, *view.meshCache);
}

// Applies one two-body contact. `normal` points from `b` toward `a`. Splits the
// positional correction by inverse mass and applies a normal + Coulomb friction
// impulse at the real contact point so both bodies gain torque and translation.
void resolveEntityPairContact(PhysicalEntity& a, PhysicalEntity& b,
                              const RecoveryContact& contact)
{
    constexpr float kSlop = 0.005f;

    glm::vec3 normal = contact.responseNormal;
    if (glm::dot(normal, normal) <= 1e-8f)
        normal = contact.normal;
    const float normalLength = glm::length(normal);
    if (normalLength <= 1e-6f)
        return;
    normal /= normalLength;

    const float invMassA = (a.motion == PhysicalEntityMotion::Dynamic && a.mass > 0.0f)
        ? 1.0f / a.mass : 0.0f;
    const float invMassB = (b.motion == PhysicalEntityMotion::Dynamic && b.mass > 0.0f)
        ? 1.0f / b.mass : 0.0f;
    const float invMassTotal = invMassA + invMassB;
    if (invMassTotal <= 1e-8f)
        return;

    const float correction = std::max(0.0f, contact.penetration - kSlop);
    if (correction > 0.0f)
    {
        a.transform[3] += glm::vec4(normal * (correction * invMassA / invMassTotal), 0.0f);
        b.transform[3] -= glm::vec4(normal * (correction * invMassB / invMassTotal), 0.0f);
    }
    a.sleeping = false;
    b.sleeping = false;
    a.sleepTicks = b.sleepTicks = 0;
    a.sleepAnchorValid = false;
    b.sleepAnchorValid = false;
    a.supportGraceTicks = b.supportGraceTicks = 0;

    const glm::vec3 comA = glm::vec3(a.transform[3]) + a.orientation * a.centerOfMass;
    const glm::vec3 comB = glm::vec3(b.transform[3]) + b.orientation * b.centerOfMass;
    // Contact point on the contact plane nearest the line between the two
    // centers of mass. A flat, face-on pair then exchanges momentum without a
    // spurious angular-resistance term (which under-exchanged momentum when the
    // manifold triangles were asymmetric), while an off-center landing still
    // gains torque from the offset of this point from each center of mass.
    const glm::vec3 midpoint = 0.5f * (comA + comB);
    const glm::vec3 point = midpoint -
        normal * glm::dot(midpoint - contact.point, normal);
    const glm::vec3 rA = point - comA;
    const glm::vec3 rB = point - comB;
    const glm::vec3 velA = a.velocity + glm::cross(a.angularVelocity, rA);
    const glm::vec3 velB = b.velocity + glm::cross(b.angularVelocity, rB);
    const float normalSpeed = glm::dot(velA - velB, normal);
    if (normalSpeed >= 0.0f)
        return;

    auto effectiveMass = [&](const PhysicalEntity& e, const glm::vec3& r,
                             const glm::vec3& n) {
        const glm::vec3 rXn = glm::cross(r, n);
        return glm::dot(glm::cross(inverseInertiaWorld(e, rXn), r), n);
    };
    const float normalMass = invMassA + invMassB +
        effectiveMass(a, rA, normal) + effectiveMass(b, rB, normal);
    if (normalMass <= 1e-8f)
        return;

    const MimitaImpact::DestructibleWorldConfig& config =
        MimitaImpact::DestructibleWorldConfig::instance();
    const float restitution =
        std::fabs(normalSpeed) < config.objectMinBounceSpeed()
            ? 0.0f : std::max(a.restitution, b.restitution);
    const float normalImpulse = -(1.0f + restitution) * normalSpeed / normalMass;
    applyImpulseAtPoint(a, normal * normalImpulse, point, false);
    applyImpulseAtPoint(b, -normal * normalImpulse, point, false);

    const glm::vec3 postA = a.velocity + glm::cross(a.angularVelocity, rA);
    const glm::vec3 postB = b.velocity + glm::cross(b.angularVelocity, rB);
    glm::vec3 tangent = (postA - postB) -
                        normal * glm::dot(postA - postB, normal);
    const float tangentLength = glm::length(tangent);
    if (tangentLength <= 1e-5f)
        return;
    tangent /= tangentLength;

    const float tangentMass = invMassA + invMassB +
        effectiveMass(a, rA, tangent) + effectiveMass(b, rB, tangent);
    if (tangentMass <= 1e-8f)
        return;
    const float desired = -glm::dot(postA - postB, tangent) / tangentMass;
    const float limit = std::max(a.friction, b.friction) * normalImpulse;
    const float tangentImpulse = glm::clamp(desired, -limit, limit);
    applyImpulseAtPoint(a, tangent * tangentImpulse, point, false);
    applyImpulseAtPoint(b, -tangent * tangentImpulse, point, false);
}

void resolveEntityContacts(std::vector<PhysicalEntity>& entities)
{
    static thread_local World s_pairWorld;
    static thread_local std::vector<int> s_candidates;
    static thread_local std::vector<ActorCollisionMesh> s_meshes;
    static thread_local std::vector<RecoveryContact> s_forward;
    static thread_local std::vector<RecoveryContact> s_reverse;
    static thread_local std::vector<RecoveryContact> s_contacts;

    for (size_t i = 0; i < entities.size(); ++i)
    {
        PhysicalEntity& a = entities[i];
        if (!entityCanMove(a))
            continue;
        const AABB boxA = entityWorldAABB(a);
        for (size_t j = i + 1; j < entities.size(); ++j)
        {
            PhysicalEntity& b = entities[j];
            if (!entityCanMove(b))
                continue;
            const AABB boxB = entityWorldAABB(b);
            if (!aabbOverlapsPadded(boxA, boxB, 0.02f))
                continue;

            // Both directions use the shared triangle narrowphase so either
            // body's linear + angular motion is swept. Normals are oriented
            // toward `a`; duplicate reverse contacts at the same spot are
            // dropped so an impulse is never applied twice.
            s_contacts.clear();
            collectPairDirection(a, b, s_pairWorld, s_candidates,
                                 s_meshes, s_forward);
            for (const RecoveryContact& c : s_forward)
                s_contacts.push_back(c);

            collectPairDirection(b, a, s_pairWorld, s_candidates,
                                 s_meshes, s_reverse);
            for (RecoveryContact c : s_reverse)
            {
                c.normal = -c.normal;
                c.responseNormal = -c.responseNormal;
                c.surfaceNormal = -c.surfaceNormal;
                bool duplicate = false;
                for (const RecoveryContact& existing : s_contacts)
                {
                    const glm::vec3 d = existing.point - c.point;
                    if (glm::dot(d, d) < 0.0004f) { duplicate = true; break; }
                }
                if (!duplicate)
                    s_contacts.push_back(c);
            }
            if (s_contacts.empty())
                continue;

            // Deterministic order and a hard per-pair cap.
            std::sort(s_contacts.begin(), s_contacts.end(),
                [](const RecoveryContact& x, const RecoveryContact& y) {
                    if (x.triangleIndex != y.triangleIndex)
                        return x.triangleIndex < y.triangleIndex;
                    return glm::dot(x.point, x.point) < glm::dot(y.point, y.point);
                });
            const size_t count = std::min<size_t>(s_contacts.size(), 32);

            // Aggregate the manifold into one representative contact. A box
            // face pair touches through several triangles; applying a separate
            // impulse per triangle under-exchanges momentum (the previous
            // AABB path used a single axis impulse). One normal/point/penetration
            // keeps the exchange correct while still using the real triangle
            // geometry rather than a box axis.
            glm::vec3 normalSum(0.0f);
            glm::vec3 pointSum(0.0f);
            float maxPenetration = 0.0f;
            for (size_t c = 0; c < count; ++c)
            {
                glm::vec3 n = s_contacts[c].responseNormal;
                if (glm::dot(n, n) <= 1e-8f)
                    n = s_contacts[c].normal;
                normalSum += n;
                pointSum += s_contacts[c].point;
                maxPenetration = std::max(maxPenetration, s_contacts[c].penetration);
            }
            if (glm::dot(normalSum, normalSum) <= 1e-8f)
                continue;

            RecoveryContact aggregate = s_contacts[0];
            aggregate.normal = normalSum;
            aggregate.responseNormal = normalSum;
            aggregate.point = pointSum / (float)count;
            aggregate.penetration = maxPenetration;
            resolveEntityPairContact(a, b, aggregate);
        }
    }
}

} // namespace

EntitySurfaceCacheView cachedEntitySurface(const PhysicalEntity& entity)
{
    struct Entry
    {
        bool valid = false;
        uint64_t revision = 0;
        glm::mat4 transform{0.0f};
        CollisionMeshCache meshCache;
        // Spatial tree over the cached world triangles, so narrowphase queries
        // only test nearby crate triangles instead of scanning all of them.
        AabbTree tree;
    };
    static thread_local std::unordered_map<uint32_t, Entry> cache;
    if (cache.size() > 128)
        cache.clear();

    Entry& entry = cache[entity.id];
    const bool cacheHit =
        entry.valid &&
        entry.revision == entity.destructible.geometryRevision &&
        entry.transform == entity.transform &&
        entry.meshCache.triangles.size() == entity.localTriangles.size();
    if (!cacheHit)
    {
        entry.valid = true;
        entry.revision = entity.destructible.geometryRevision;
        entry.transform = entity.transform;
        CollisionMeshCache& mesh = entry.meshCache;
        mesh.triangles.clear();
        mesh.triangleAABBs.clear();
        mesh.triangles.reserve(entity.localTriangles.size());
        mesh.triangleAABBs.reserve(entity.localTriangles.size());
        for (const CollisionTriangle& lt : entity.localTriangles)
        {
            CollisionTriangle wt;
            wt.a = glm::vec3(entity.transform * glm::vec4(lt.a, 1.0f));
            wt.b = glm::vec3(entity.transform * glm::vec4(lt.b, 1.0f));
            wt.c = glm::vec3(entity.transform * glm::vec4(lt.c, 1.0f));
            const glm::vec3 n = glm::cross(wt.b - wt.a, wt.c - wt.a);
            const float len = glm::length(n);
            wt.normal = len > 1e-8f ? n / len : glm::vec3(0.0f, 0.0f, 1.0f);
            mesh.triangleAABBs.push_back(makeTriangleAABB(wt));
            mesh.triangles.push_back(wt);
        }
        std::vector<int> primitives(mesh.triangles.size());
        std::iota(primitives.begin(), primitives.end(), 0);
        entry.tree.build(primitives, mesh.triangleAABBs);
    }
    return EntitySurfaceCacheView{&entry.meshCache, &entry.tree};
}

std::vector<EntityActorContact> collectActorEntityContacts(
    const std::vector<ActorCollisionMesh>& meshes,
    const std::vector<PhysicalEntity>& entities,
    const glm::vec3& actorPos)
{
    std::vector<EntityActorContact> out;
    if (meshes.empty() || entities.empty())
        return out;

    const AABB actorBox = makeSweptActorMeshAABB(meshes, glm::vec3(0.0f));
    const float kPad = std::max(0.05f, MOVEMENT_FEATURE_SMOOTHNESS);

    // Reused per-thread bridge storage. The entity world-space triangles and
    // candidate list are refilled in place, so a touched crate no longer
    // allocates a World, a triangle vector, and a candidate vector per entity
    // per correction iteration. collectActorMeshContacts still owns the one
    // triangle-vs-triangle routine; the entity reuses it.
    static thread_local World s_entityWorld;
    static thread_local std::vector<int> s_entityCandidates;

    for (const PhysicalEntity& e : entities)
    {
        if (!e.collidesWithActors || e.localTriangles.empty())
            continue;
        if (!aabbOverlapsPadded(actorBox, entityWorldAABB(e), kPad))
            continue;

        // World-space expansion is stable while an entity rests; the shared
        // cache rebuilds it only when the pose or generated surface changed.
        const EntitySurfaceCacheView view = cachedEntitySurface(e);

        // Swap the cached expansion in for the query, then back out, so a cache
        // hit costs no per-triangle copy.
        std::swap(s_entityWorld.collisionMesh, *view.meshCache);
        s_entityCandidates.resize(s_entityWorld.collisionMesh.triangles.size());
        std::iota(s_entityCandidates.begin(), s_entityCandidates.end(), 0);

        std::vector<RecoveryContact> contacts;
        // The entity candidate list is complete for this entity; keep the exact
        // entity query intact instead of applying the static-world part filter.
        // The per-entity tree prunes which crate triangles the actor narrowphase
        // actually tests. NOTE: the actor-vs-entity adapter keeps the exact
        // triangle contract (skin 0) because the rounded shell changed the
        // player-carry support behavior; the rounded shell is used for
        // entity-vs-entity and entity-vs-world, which is where crate phasing
        // happens. Migrating the actor path is tracked in the regression record.
        collectActorMeshContactsInto(s_entityWorld, meshes, s_entityCandidates,
                                     actorPos, contacts, false, 0.0f, view.tree);
        std::swap(s_entityWorld.collisionMesh, *view.meshCache);

        for (RecoveryContact& c : contacts)
        {
            c.entityId = e.id;
            c.surfaceVelocity = e.velocity;
            c.surfaceMass = e.motion == PhysicalEntityMotion::Dynamic ? e.mass : 0.0f;
            c.surfaceRestitution = e.restitution;
            out.push_back(EntityActorContact{c, e.id});
        }
    }
    return out;
}

namespace {

GLuint gPhysicalBoxVao = 0;
GLuint gPhysicalBoxVbo = 0;

void drawTexturedPhysicalBox(const Camera& camera, const PhysicalEntity& e)
{
    if (!gRenderer || !gRenderer->shaderProgram)
        return;

    // Six independent faces keep UVs simple and make this renderer reusable
    // for imported box-shaped objects before a full GLB model renderer exists.
    const glm::vec3 h = e.halfExtents;
    const glm::vec3 p[8] = {
        {-h.x, -h.y, -h.z}, { h.x, -h.y, -h.z},
        { h.x,  h.y, -h.z}, {-h.x,  h.y, -h.z},
        {-h.x, -h.y,  h.z}, { h.x, -h.y,  h.z},
        { h.x,  h.y,  h.z}, {-h.x,  h.y,  h.z}
    };
    const int faces[6][4] = {
        {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
        {3, 7, 6, 2}, {1, 2, 6, 5}, {0, 4, 7, 3}
    };
    const glm::vec3 normals[6] = {
        {0, 0, -1}, {0, 0, 1}, {0, -1, 0},
        {0, 1, 0}, {1, 0, 0}, {-1, 0, 0}
    };
    std::vector<Vertex> verts;
    verts.reserve(36);
    for (int face = 0; face < 6; ++face)
    {
        const int* f = faces[face];
        const glm::vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        const int order[6] = {0, 1, 2, 0, 2, 3};
        for (int i : order)
            verts.push_back({p[f[i]], normals[face], uv[i]});
    }

    if (!gPhysicalBoxVao)
    {
        glGenVertexArrays(1, &gPhysicalBoxVao);
        glGenBuffers(1, &gPhysicalBoxVbo);
    }
    glUseProgram(gRenderer->shaderProgram);
    const glm::mat4 view = camera.getView();
    const glm::mat4 proj = camera.getProj((float)gRenderer->width,
                                          (float)gRenderer->height);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "model"),
                       1, GL_FALSE, [&]() {
                           static glm::mat4 renderTransform(1.0f);
                           const float alpha = PhysicalEntitySystem::instance().renderAlpha();
                           const glm::vec3 previousPosition(e.previousTransform[3]);
                           const glm::vec3 currentPosition(e.transform[3]);
                           const glm::quat previousOrientation = glm::normalize(
                               glm::quat_cast(glm::mat3(e.previousTransform)));
                           renderTransform = glm::translate(
                               glm::mat4(1.0f),
                               glm::mix(previousPosition, currentPosition, alpha)) *
                               glm::mat4_cast(glm::normalize(glm::slerp(
                                   previousOrientation, e.orientation, alpha)));
                           return &renderTransform[0][0];
                       }());
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "view"),
                       1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "projection"),
                       1, GL_FALSE, &proj[0][0]);
    setUniforms(gRenderer->shaderProgram, camera.pos);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextures.getPath(e.texturePath));

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    GLint cullModeWas = GL_BACK;
    glGetIntegerv(GL_CULL_FACE_MODE, &cullModeWas);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glBindVertexArray(gPhysicalBoxVao);
    glBindBuffer(GL_ARRAY_BUFFER, gPhysicalBoxVbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(),
                 GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, normal));
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)verts.size());
    if (!cullWasEnabled)
        glDisable(GL_CULL_FACE);
    glCullFace(cullModeWas);
    glBindVertexArray(0);
}

} // namespace

void drawPhysicalEntities(const Camera& camera)
{
    const std::vector<PhysicalEntity>& entities =
        PhysicalEntitySystem::instance().entities();
    for (const PhysicalEntity& e : entities)
    {
        if (e.localTriangles.empty())
            continue;
        // Prefer the generated destructible mesh; fall back to the box.
        if (drawGeneratedEntityMesh(e, camera))
            continue;
        drawTexturedPhysicalBox(camera, e);
    }
}

// ── Deterministic moving-crate proof ───────────────────────────────

namespace {

void addFloorQuad(World& world, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d)
{
    auto add = [&](glm::vec3 p0, glm::vec3 p1, glm::vec3 p2) {
        glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
        const float len = glm::length(n);
        if (len < 1e-6f)
            return;
        CollisionTriangle t;
        t.a = p0; t.b = p1; t.c = p2; t.normal = n / len;
        world.collisionMesh.triangles.push_back(t);
    };
    add(a, b, c);
    add(a, c, d);
}

// One rigid box body part whose bottom face sits at the actor root z.
void setupTestBoxActor(Player& p, glm::vec3 pos, float halfXY, float halfZ)
{
    p.pos = pos;
    p.yaw = 0.0f;

    PhysicalBodyPart part;
    part.name = "torso";
    part.nodeIndex = 0;
    Collider& col = part.collider;
    col.name = "torso";
    col.localMin = glm::vec3(0.0f, 0.0f, halfZ) - glm::vec3(halfXY, halfXY, halfZ);
    col.localMax = glm::vec3(0.0f, 0.0f, halfZ) + glm::vec3(halfXY, halfXY, halfZ);
    buildBoxCollisionTriangles(col.triangles, glm::vec3(0.0f, 0.0f, halfZ),
                               glm::vec3(halfXY, halfXY, halfZ));

    p.physicalBody.parts.clear();
    p.physicalBody.parts.push_back(part);
    p.nodes.resize(1);
    p.restLocalTransforms.assign(1, glm::mat4(1.0f));
    p.perfectPoseSkeleton.nodes.resize(1);
    p.perfectPoseSkeleton.nodes[0].name = "root";
    p.perfectPoseSkeleton.nodes[0].parent = -1;
    p.perfectPoseSkeleton.nodes[0].localTransform = glm::mat4(1.0f);
    p.perfectPoseSkeleton.restLocalTransforms.assign(1, glm::mat4(1.0f));
    p.updateModelWorldTransforms();
}

} // namespace

bool physicalEntitySelfTest(std::string* outSummary)
{
    std::string report;
    bool ok = true;
    auto check = [&](bool cond, const char* name) {
        report += cond ? "  PASS: " : "  FAIL: ";
        report += name;
        report += "\n";
        if (!cond) ok = false;
    };

    constexpr float dt = 1.0f / 60.0f;
    const float crateHalf = 0.5f;
    const float crateTop0 = crateHalf * 2.0f;   // local box z range [0, 1]

    World world;
    addFloorQuad(world, {-20, -20, 0}, {20, -20, 0}, {20, 20, 0}, {-20, 20, 0});
    buildCollisionChunks(world, nullptr);

    PhysicalEntitySystem& system = PhysicalEntitySystem::instance();

    // 1. Standing on a moving crate: the actor is carried and reports support.
    {
        system.clear();
        std::vector<CollisionTriangle> crate;
        buildBoxCollisionTriangles(crate, glm::vec3(0.0f, 0.0f, crateHalf),
                                   glm::vec3(crateHalf));
        const uint32_t crateId = system.add(
            crate, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f)),
            PhysicalEntityMotion::Kinematic);

        Player p(false);
        setupTestBoxActor(p, glm::vec3(0.0f, 0.0f, crateTop0 - 0.02f), 0.4f, 0.5f);

        const glm::vec3 crateVel(2.0f, 0.0f, 0.0f);
        glm::vec3 cratePos(0.0f);
        const float startXCrate = cratePos.x;
        bool grounded = false;
        constexpr int kTicks = 30;
        for (int i = 0; i < kTicks; ++i)
        {
            p.vel.z -= 58.0f * dt;   // gravity keeps the actor pressed onto the crate
            cratePos.x += crateVel.x * dt;
            system.moveKinematic(crateId, glm::translate(glm::mat4(1.0f), cratePos), dt);
            grounded = false;
            runActorTriangleCollisionStep(p, world, grounded, dt);
        }

        const float crateMoved = cratePos.x - startXCrate;
        check(grounded, "actor on moving crate stays grounded");
        check(std::fabs((p.pos.x - 0.0f) - crateMoved) < 0.05f,
              "actor world motion includes crate velocity");
        check(p.collision.supportEntityId == crateId,
              "canonical contact identifies the support entity");
        check(std::fabs(p.collision.supportVelocity.x - crateVel.x) < 1e-4f,
              "support surfaces exposes the entity velocity");
    }

    // 2. Walking adds to the support motion. A wide crate keeps the actor fully
    //    supported while it walks relative to the crate.
    {
        system.clear();
        const float wideHalf = 4.0f;
        const float wideTop = wideHalf * 2.0f;
        std::vector<CollisionTriangle> crate;
        buildBoxCollisionTriangles(crate, glm::vec3(0.0f, 0.0f, wideHalf),
                                   glm::vec3(wideHalf));
        const uint32_t crateId = system.add(
            crate, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f)),
            PhysicalEntityMotion::Kinematic);

        Player p(false);
        setupTestBoxActor(p, glm::vec3(0.0f, 0.0f, wideTop - 0.02f), 0.4f, 0.5f);

        const glm::vec3 crateVel(2.0f, 0.0f, 0.0f);
        const float walk = 1.0f;
        glm::vec3 cratePos(0.0f);
        const float startX = p.pos.x;
        bool grounded = false;
        constexpr int kTicks = 30;
        for (int i = 0; i < kTicks; ++i)
        {
            // Local walking, as the movement controller would assign it; gravity
            // keeps contact so the actor stays supported.
            p.vel.x = walk;
            p.vel.z -= 58.0f * dt;
            cratePos.x += crateVel.x * dt;
            system.moveKinematic(crateId, glm::translate(glm::mat4(1.0f), cratePos), dt);
            grounded = false;
            runActorTriangleCollisionStep(p, world, grounded, dt);
        }

        const float expected = (crateVel.x + walk) * dt * (float)kTicks;
        check(grounded, "walking actor stays supported");
        check(std::fabs((p.pos.x - startX) - expected) < 0.08f,
              "local walking adds to the support velocity");
    }

    // 3. Jumping preserves the inherited support velocity, and leaving the crate
    //    does not zero it.
    {
        system.clear();
        std::vector<CollisionTriangle> crate;
        buildBoxCollisionTriangles(crate, glm::vec3(0.0f, 0.0f, crateHalf),
                                   glm::vec3(crateHalf));
        const uint32_t crateId = system.add(
            crate, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f)),
            PhysicalEntityMotion::Kinematic);

        Player p(false);
        setupTestBoxActor(p, glm::vec3(0.0f, 0.0f, crateTop0 - 0.02f), 0.4f, 0.5f);

        const glm::vec3 crateVel(3.0f, 0.0f, 0.0f);
        glm::vec3 cratePos(0.0f);
        // Settle onto the moving crate.
        for (int i = 0; i < 10; ++i)
        {
            p.vel.z -= 58.0f * dt;
            cratePos.x += crateVel.x * dt;
            system.moveKinematic(crateId, glm::translate(glm::mat4(1.0f), cratePos), dt);
            bool g = false;
            runActorTriangleCollisionStep(p, world, g, dt);
        }
        check(p.collision.supportEntityId == crateId, "actor is settled on the crate");

        // Jump: set an upward velocity from rest (x/y zero to isolate inheritance)
        // and tick until the actor leaves the crate. The departure tick must add
        // the support velocity to the actor's own velocity.
        p.vel = glm::vec3(0.0f, 0.0f, 19.0f);
        float inheritedAfterJump = 0.0f;
        bool airborne = false;
        for (int i = 0; i < 10 && !airborne; ++i)
        {
            p.vel.z -= 58.0f * dt;
            cratePos.x += crateVel.x * dt;
            system.moveKinematic(crateId, glm::translate(glm::mat4(1.0f), cratePos), dt);
            bool g = false;
            runActorTriangleCollisionStep(p, world, g, dt);
            if (!g)
            {
                airborne = true;
                inheritedAfterJump = p.vel.x;
            }
        }
        check(airborne, "jump lifts the actor off the crate");
        check(std::fabs(inheritedAfterJump - crateVel.x) < 0.05f,
              "jumping preserves the inherited support velocity");

        // Leave: remove the crate entirely and tick once. Support must clear but
        // the inherited velocity must survive.
        system.clear();
        const float velBeforeLeave = p.vel.x;
        bool g = false;
        runActorTriangleCollisionStep(p, world, g, dt);
        check(p.collision.supportEntityId == 0, "leaving the crate clears support");
        check(std::fabs(p.vel.x - velBeforeLeave) < 1e-4f,
              "leaving the crate does not zero inherited velocity");
        check(std::fabs(p.vel.x) > 0.1f, "inherited velocity remains after departure");
    }

    // 4. A Dynamic crate falls under the shared gravity value and settles on
    // the same world triangle manifold instead of remaining a frozen kinematic
    // debug object.
    {
        system.clear();
        std::vector<CollisionTriangle> crate;
        buildBoxCollisionTriangles(crate, glm::vec3(0.0f), glm::vec3(0.5f));
        const uint32_t crateId = system.add(
            crate, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 5.0f)),
            PhysicalEntityMotion::Dynamic);
        PhysicalEntity* dynamicCrate = system.find(crateId);
        dynamicCrate->mass = 20.0f;
        for (int i = 0; i < 180; ++i)
            system.advanceKinematics(1.0f / 60.0f, world);
        check(dynamicCrate->transform[3].z > 0.45f &&
                  dynamicCrate->transform[3].z < 0.65f,
              "dynamic crate falls onto the floor");
        check(std::fabs(dynamicCrate->velocity.z) < 0.01f,
              "dynamic crate removes inward floor velocity");
        check(dynamicCrate->sleeping, "resting dynamic crate enters sleep");

        Player p(false);
        p.pos = glm::vec3(0.9f, 0.9f, 0.2f);
        p.vel = glm::vec3(-10.0f, -10.0f, 0.0f);
        system.applyPlayerPush(p, 1.0f / 60.0f);
        check(glm::length(dynamicCrate->angularVelocity) > 0.001f,
              "off-center player push creates angular velocity");
        system.advanceKinematics(1.0f / 60.0f, world);
        check(glm::length(glm::eulerAngles(dynamicCrate->orientation)) > 0.0f,
              "angular velocity changes crate orientation");
    }

    // 5. Dynamic crates exchange momentum through the same fixed-step owner.
    {
        system.clear();
        std::vector<CollisionTriangle> crate;
        buildBoxCollisionTriangles(crate, glm::vec3(0.0f), glm::vec3(0.5f));
        const uint32_t leftId = system.add(
            crate, glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 0.0f, 0.5f)),
            PhysicalEntityMotion::Dynamic);
        const uint32_t rightId = system.add(
            crate, glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.5f)),
            PhysicalEntityMotion::Dynamic);
        PhysicalEntity* left = system.find(leftId);
        PhysicalEntity* right = system.find(rightId);
        left->gravityScale = 0.0f;
        right->gravityScale = 0.0f;
        left->velocity.x = 12.0f;
        right->velocity.x = -12.0f;
        left->restitution = 0.5f;
        right->restitution = 0.5f;
        for (int i = 0; i < 30; ++i)
            system.advanceKinematics(1.0f / 60.0f, world);
        check(left->velocity.x < 0.0f && right->velocity.x > 0.0f,
              "dynamic crates exchange momentum");
        check(glm::length(glm::vec3(right->transform[3]) -
                          glm::vec3(left->transform[3])) >= 0.99f,
              "dynamic crates do not remain overlapped");
    }

    // 6. A destructible dynamic crate keeps the full rigid-body behavior: it
    // falls under gravity and rests on the floor while using the generated
    // planar collision surface (built lazily on the first cut).
    {
        system.clear();
        std::vector<CollisionTriangle> box;
        buildBoxCollisionTriangles(box, glm::vec3(0.0f), glm::vec3(0.5f));
        const uint32_t id = system.add(
            box, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 5.0f)),
            PhysicalEntityMotion::Dynamic);
        PhysicalEntity* destructible = system.find(id);
        destructible->halfExtents = glm::vec3(0.5f);
        destructible->density = 700.0f;
        destructible->mass = 700.0f;
        destructible->friction = 0.7f;
        destructible->linearDamping = 0.15f;
        destructible->angularDamping = 2.5f;
        MimitaImpact::ImpactSystem::instance().initializeEntity(
            *destructible, MimitaImpact::materialIdForName("wood"),
            glm::vec3(0.5f));
        // No geometry until the first cut; then the planar surface replaces the box.
        const bool lazyMesh = destructible->localTriangles.size() == 12;
        MimitaImpact::DestructionCut cut;
        cut.cutter.type = MimitaImpact::BooleanCutterType::Sphere;
        cut.cutter.localCenter = glm::vec3(0.0f, 0.0f, 0.5f);
        cut.cutter.radius = 0.2f;
        MimitaImpact::DestructibleGeometrySystem::instance().addCut(
            destructible->destructible, cut);
        destructible->localTriangles = destructible->destructible.collisionTriangles;
        const bool generatedMesh = destructible->localTriangles.size() > 12;
        for (int i = 0; i < 180; ++i)
            system.advanceKinematics(1.0f / 60.0f, world);
        check(lazyMesh, "destructible crate keeps the box mesh until the first cut");
        check(generatedMesh, "destructible crate uses the generated collision mesh");
        check(destructible->transform[3].z > 0.45f &&
                  destructible->transform[3].z < 0.65f,
              "destructible dynamic crate falls onto the floor");
    }

    // 7. A projectile impact transfers momentum to the crate at the hit point:
    //    the crate gains translation and torque instead of only losing geometry,
    //    and it is never teleported.
    {
        system.clear();
        std::vector<CollisionTriangle> box;
        buildBoxCollisionTriangles(box, glm::vec3(0.0f), glm::vec3(1.0f));
        const uint32_t id = system.add(
            box, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 5.0f)),
            PhysicalEntityMotion::Dynamic);
        PhysicalEntity* crate = system.find(id);
        crate->gravityScale = 0.0f;
        crate->density = 1.0f;
        crate->mass = 8.0f;
        crate->halfExtents = glm::vec3(1.0f);
        if (MimitaImpact::MaterialConfig::instance().revision() == 0)
            MimitaImpact::MaterialConfig::instance().load();
        MimitaImpact::ImpactSystem::instance().initializeEntity(
            *crate, MimitaImpact::materialIdForName("wood"), glm::vec3(1.0f));

        const glm::vec3 before = glm::vec3(crate->transform[3]);
        MimitaImpact::ImpactEvent ev;
        ev.source = MimitaImpact::ImpactSource::Projectile;
        ev.target = MimitaImpact::ImpactTarget::PhysicalEntity;
        ev.targetEntityId = id;
        ev.worldPoint = glm::vec3(-1.0f, 0.0f, 5.6f); // off-center => torque
        ev.worldNormal = glm::vec3(-1.0f, 0.0f, 0.0f);
        ev.worldDirection = glm::vec3(1.0f, 0.0f, 0.0f);
        ev.mass = 0.05f;
        ev.speed = 400.0f;
        ev.radius = 0.1f;
        ev.sizeScale = 1.0f;
        ev.cutScale = 1.0f;
        ev.energy = MimitaImpact::ImpactSystem::kineticEnergy(ev.mass, ev.speed);
        MimitaImpact::ImpactSystem::instance().submit(ev);

        check(crate->velocity.x > 0.0f,
              "projectile impact pushes the crate along the shot");
        check(glm::length(crate->angularVelocity) > 0.001f,
              "off-center projectile impact spins the crate");
        check(glm::length(glm::vec3(crate->transform[3]) - before) < 1e-4f,
              "projectile impact does not teleport the crate");
    }

    system.clear();

    if (outSummary)
        *outSummary = report;
    return ok;
}

// Headless performance probe for the entity/destruction physics path. Builds a
// world with a floor, spawns several crates each riddled with holes, lets them
// settle, then measures a single fixed-tick cost. Runs without a window so it
// is usable as an automated guard for the <4 ms frame budget work.
bool physicalEntityPerfSelfTest(std::string* outSummary)
{
    std::string report;
    bool ok = true;
    auto check = [&](bool cond, const char* name) {
        report += cond ? "  PASS: " : "  FAIL: ";
        report += name;
        report += "\n";
        if (!cond) ok = false;
    };

    if (MimitaImpact::MaterialConfig::instance().revision() == 0)
        MimitaImpact::MaterialConfig::instance().load();
    MimitaImpact::DestructibleWorldConfig::instance().load();

    World world;
    addFloorQuad(world, {-80, -80, 0}, {80, -80, 0}, {80, 80, 0}, {-80, 80, 0});
    buildCollisionChunks(world, nullptr);

    PhysicalEntitySystem& system = PhysicalEntitySystem::instance();
    system.clear();

    constexpr int kCrates = 10;
    constexpr int kHolesPerCrate = 48;
    const float half = 2.0f;
    const uint32_t wood = MimitaImpact::materialIdForName("wood");

    size_t totalTriangles = 0;
    double buildMs = 0.0;
    {
        const auto buildStart = std::chrono::steady_clock::now();
        for (int i = 0; i < kCrates; ++i)
        {
            std::vector<CollisionTriangle> box;
            buildBoxCollisionTriangles(box, glm::vec3(0.0f), glm::vec3(half));
            const float x = (float)(i - kCrates / 2) * 6.0f;
            const uint32_t id = system.add(
                box, glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, half + 0.01f)),
                PhysicalEntityMotion::Dynamic, wood);
            PhysicalEntity* e = system.find(id);
            if (!e)
                continue;
            MimitaImpact::ImpactSystem::instance().initializeEntity(
                *e, wood, glm::vec3(half));

            // Punch many small holes through one face, then rebuild once.
            for (int h = 0; h < kHolesPerCrate; ++h)
            {
                MimitaImpact::DestructionCut cut;
                cut.cutter.type = MimitaImpact::BooleanCutterType::Sphere;
                const float u = (float)((h % 8) - 4) * 0.4f;
                const float v = (float)((h / 8) - 3) * 0.4f;
                cut.cutter.localCenter = glm::vec3(u, v, half);
                cut.cutter.radius = 0.35f;
                MimitaImpact::DestructibleGeometrySystem::instance().enqueueCut(
                    e->destructible, cut);
            }
            MimitaImpact::DestructibleGeometrySystem::instance().flushQueuedCuts(
                e->destructible, 0);
            e->localTriangles = e->destructible.collisionTriangles;
            totalTriangles += e->localTriangles.size();
        }
        buildMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - buildStart).count();
    }

    constexpr float dt = 1.0f / 60.0f;
    for (int t = 0; t < 30; ++t) // warm up: let them settle onto the floor
        system.advanceKinematics(dt, world);

    constexpr int kTicks = 300;
    const auto measureStart = std::chrono::steady_clock::now();
    for (int t = 0; t < kTicks; ++t)
        system.advanceKinematics(dt, world);
    const double totalMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - measureStart).count();
    const double perTickMs = totalMs / (double)kTicks;

    // Moving phase: keep disturbing the crates so they never settle, which is
    // the real "holey crate moving" cost (the settled phase is nearly free
    // because settled bodies sleep).
    const auto moveStart = std::chrono::steady_clock::now();
    for (int t = 0; t < kTicks; ++t)
    {
        if (t % 30 == 0)
        {
            for (PhysicalEntity& e : system.entities())
            {
                if (e.motion != PhysicalEntityMotion::Dynamic)
                    continue;
                e.velocity += glm::vec3(2.0f, 0.0f, 3.0f);
                e.sleeping = false;
                e.sleepAnchorValid = false;
            }
        }
        system.advanceKinematics(dt, world);
    }
    const double moveTotalMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - moveStart).count();
    const double movePerTickMs = moveTotalMs / (double)kTicks;

    char info[288];
    std::snprintf(info, sizeof(info),
        "  INFO: crates=%d holes/crate=%d tris=%zu build=%.1fms ticks=%d "
        "settled=%.3fms/tick moving=%.3fms/tick\n",
        kCrates, kHolesPerCrate, totalTriangles, buildMs, kTicks, perTickMs,
        movePerTickMs);
    report += info;

    check(totalTriangles > 0, "holey crates have generated collision triangles");
    check(perTickMs < 4.0,
          "settled holey crates stay under 4ms per fixed tick");
    // Moving-crate cost is the active target for the local-space collision
    // refactor. Report it as a measurement (MET/MISS) without failing the suite
    // so the number stays visible and trackable.
    {
        char target[96];
        std::snprintf(target, sizeof(target),
            "  TARGET(4ms) moving holey crates: %s (%.3fms/tick)\n",
            movePerTickMs < 4.0 ? "MET" : "MISS", movePerTickMs);
        report += target;
    }

    system.clear();
    if (outSummary)
        *outSummary = report;
    return ok;
}
