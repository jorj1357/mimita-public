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
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glad/glad.h>

#include "physics/config.h"
#include "physics/movement/actor-triangle-solver.h"
#include "physics/movement/physics-collision-shared.h"
#include "config/material-config.h"
#include "impact/destructible-geometry.h"
#include "impact/destructible-render.h"
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

void refreshBoxMassProperties(PhysicalEntity& e)
{
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
        normalImpulse = -(1.0f + e.restitution) * normalSpeed / normalMass;
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

bool isBoxRestingUpright(const PhysicalEntity& e)
{
    const glm::vec3 worldUp(0.0f, 0.0f, 1.0f);
    const glm::vec3 localAxes[] = {
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f)
    };
    float best = 0.0f;
    for (const glm::vec3& localAxis : localAxes)
        best = std::max(best, std::fabs(glm::dot(e.orientation * localAxis, worldUp)));
    return best >= e.restingUprightDot;
}

void applyRestingRightingTorque(PhysicalEntity& e, float dt)
{
    const glm::vec3 worldUp(0.0f, 0.0f, 1.0f);
    const glm::vec3 localAxes[] = {
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f)
    };
    glm::vec3 selectedWorldAxis(0.0f);
    float best = -1.0f;
    for (const glm::vec3& localAxis : localAxes)
    {
        glm::vec3 axis = e.orientation * localAxis;
        const float alignment = std::fabs(glm::dot(axis, worldUp));
        if (alignment > best)
        {
            best = alignment;
            selectedWorldAxis = glm::dot(axis, worldUp) >= 0.0f ? axis : -axis;
        }
    }
    const float axisLength = glm::length(selectedWorldAxis);
    if (axisLength <= 1e-5f)
        return;
    selectedWorldAxis /= axisLength;
    const glm::vec3 errorAxis = glm::cross(selectedWorldAxis, worldUp);
    const float errorLength = glm::length(errorAxis);
    if (errorLength <= 1e-5f)
        return;
    const float errorAngle = std::atan2(errorLength,
                                        glm::dot(selectedWorldAxis, worldUp));
    e.angularVelocity += (errorAxis / errorLength) *
                         (errorAngle * e.rightingStrength * dt);
}

} // namespace

PhysicalEntitySystem& PhysicalEntitySystem::instance()
{
    static PhysicalEntitySystem system;
    return system;
}

void PhysicalEntitySystem::clear()
{
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
    refreshBoxMassProperties(e);
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
    return e;
}

PhysicalEntity* PhysicalEntitySystem::find(uint32_t id)
{
    for (PhysicalEntity& e : mEntities)
        if (e.id == id)
            return &e;
    return nullptr;
}

bool PhysicalEntitySystem::remove(uint32_t id)
{
    for (auto it = mEntities.begin(); it != mEntities.end(); ++it)
    {
        if (it->id == id)
        {
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
            if ((e.motion != PhysicalEntityMotion::Kinematic &&
                 e.motion != PhysicalEntityMotion::Dynamic) || e.sleeping)
                continue;
            refreshBoxMassProperties(e);
            if (e.motion == PhysicalEntityMotion::Dynamic)
            {
                e.velocity.z = std::max(
                    e.velocity.z + PHYS.gravity * e.gravityScale * (float)kFixedDt,
                    -MAX_FALL_SPEED);
                e.velocity *= std::max(0.0f, 1.0f - e.linearDamping * (float)kFixedDt);
                e.angularVelocity *= std::max(0.0f, 1.0f - e.angularDamping * (float)kFixedDt);
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
            }
            if (glm::dot(e.velocity, e.velocity) <= 1e-8f &&
                glm::dot(e.angularVelocity, e.angularVelocity) <= 1e-8f)
                continue;
            e.previousTransform = e.transform;
            e.transform[3] += glm::vec4(e.velocity * (float)kFixedDt, 0.0f);
            rebuildTransformFromPose(e);

            // First Phase 3 slice: kinematic objects sweep their authored
            // local triangles against the static world and slide instead of
            // passing through it. Player/object contacts remain owned by the
            // actor manifold below.
            ActorCollisionMesh objectMesh;
            objectMesh.label = "physicalEntity";
            objectMesh.localTriangles = &e.localTriangles;
            std::vector<ActorCollisionMesh> meshes{objectMesh};
            std::vector<int> candidates;
            std::vector<RecoveryContact> contacts;
            for (int collisionPass = 0; collisionPass < 3; ++collisionPass)
            {
                meshes[0].previousTransform = e.previousTransform;
                meshes[0].desiredTransform = e.transform;
                meshes[0].localTriangles = &e.localTriangles;
                const AABB sweepBox = makeSweptActorMeshAABB(
                    meshes, glm::vec3(0.0f));
                candidates.clear();
                appendChunkTrianglesForAABB(world, sweepBox, 0.1f, candidates,
                                            "physicalEntitySweep");
                if (candidates.empty())
                    break;

                // The destructible mesh is intentionally low-poly (a box plus a
                // bounded number of hole triangles), so the body sweeps its full
                // local mesh directly; no per-entity broadphase is needed.
                std::vector<RecoveryContact> passContacts = collectActorMeshContacts(
                    world, meshes, candidates, glm::vec3(e.transform[3]),
                    true, -1.0f);
                if (passContacts.empty())
                    break;
                contacts = std::move(passContacts);
                const glm::vec3 correction = solveBatchedCorrection(
                    contacts, 0.01f, nullptr, nullptr,
                    e.velocity * (float)kFixedDt, glm::vec3(e.transform[3]));
                e.transform[3] += glm::vec4(correction, 0.0f);
                for (const RecoveryContact& contact : contacts)
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
            if (contacts.empty())
            {
                // Preserve a short support window across a one-tick rounded
                // feature gap. Gravity still runs, so a body that genuinely
                // leaves the surface cannot remain asleep on this grace path.
                if (e.motion == PhysicalEntityMotion::Dynamic &&
                    e.supportGraceTicks > 0 && glm::length(e.velocity) < 0.5f)
                {
                    --e.supportGraceTicks;
                    e.velocity.x *= 0.65f;
                    e.velocity.y *= 0.65f;
                    e.angularVelocity *= 0.15f;
                    if (glm::length(e.velocity) < std::max(0.5f, e.sleepLinearThreshold) &&
                        glm::length(e.angularVelocity) < std::max(0.5f, e.sleepAngularThreshold))
                        ++e.sleepTicks;
                    if (e.sleepTicks >= e.sleepRequiredTicks)
                    {
                        e.velocity = glm::vec3(0.0f);
                        e.angularVelocity = glm::vec3(0.0f);
                        e.sleeping = true;
                    }
                }
                continue;
            }

            if (e.motion == PhysicalEntityMotion::Dynamic)
            {
                bool supported = false;
                for (const RecoveryContact& contact : contacts)
                {
                    if (contact.normal.z > MAX_WALKABLE_SLOPE_DOT)
                    {
                        supported = true;
                        break;
                    }
                }
                if (supported)
                    e.supportGraceTicks = 3;
                if (supported && glm::length(e.velocity) < 0.5f)
                {
                    // Static contact friction absorbs residual roll/spin once
                    // the body is no longer meaningfully translating. This is
                    // the rigid-body equivalent of the player's grounded
                    // velocity projection, not a free rotation override.
                    e.velocity.x *= 0.65f;
                    e.velocity.y *= 0.65f;
                    applyRestingRightingTorque(e, (float)kFixedDt);
                    e.angularVelocity *= 0.55f;
                }
                const bool upright = isBoxRestingUpright(e);
                const bool stableCandidate = supported && upright &&
                    glm::length(e.velocity) < std::max(0.5f, e.sleepLinearThreshold) &&
                    glm::length(e.angularVelocity) < std::max(0.5f, e.sleepAngularThreshold);
                if (stableCandidate)
                    ++e.sleepTicks;
                else if (glm::length(e.velocity) > 1.0f ||
                         glm::length(e.angularVelocity) > 1.0f)
                    e.sleepTicks = 0;
                if (e.sleepTicks >= e.sleepRequiredTicks)
                {
                    e.velocity = glm::vec3(0.0f);
                    e.angularVelocity = glm::vec3(0.0f);
                    e.sleeping = true;
                }
                if (!supported && contacts.size() >= 2 && glm::length(e.velocity) < 0.25f)
                {
                    glm::vec3 escape(0.0f);
                    for (const RecoveryContact& contact : contacts)
                        escape += contact.responseNormal;
                    if (glm::dot(escape, escape) > 1e-6f)
                    {
                        e.velocity += glm::normalize(escape) * 0.15f;
                        e.sleeping = false;
                        e.sleepTicks = 0;
                    }
                }
            }
        }
        resolveEntityContacts(mEntities);
    }
    if (steps >= kMaxSteps && mFixedAccumulator >= kFixedDt)
        mFixedAccumulator = 0.0;
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
    glm::vec3 horizontalVelocity(player.vel.x, player.vel.y, 0.0f);
    if (glm::dot(horizontalVelocity, horizontalVelocity) <= 1e-6f &&
        glm::dot(player.inputWishMove, player.inputWishMove) > 1e-4f)
        horizontalVelocity = glm::vec3(player.inputWishMove.x,
                                       player.inputWishMove.y, 0.0f) *
                             MAX_PLAYER_MOVE_SPEED;
    if (glm::dot(horizontalVelocity, horizontalVelocity) <= 1e-6f)
        return;

    for (PhysicalEntity& e : mEntities)
    {
        if (e.motion != PhysicalEntityMotion::Dynamic || e.mass <= 0.0f)
            continue;
        if (e.lastPlayerPushTick == mSimulationTick)
            continue;
        const AABB objectBox = entityWorldAABB(e);
        if (!aabbOverlapsPadded(playerBox, objectBox, 0.05f))
            continue;

        refreshBoxMassProperties(e);
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
    if (!e || e->motion != PhysicalEntityMotion::Dynamic || e->mass <= 0.0f ||
        e->lastPlayerPushTick == mSimulationTick)
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

void resolveEntityContacts(std::vector<PhysicalEntity>& entities)
{
    constexpr float kSlop = 0.005f;
    for (size_t i = 0; i < entities.size(); ++i)
    {
        PhysicalEntity& a = entities[i];
        if (a.motion == PhysicalEntityMotion::Static || !a.collidesWithActors)
            continue;
        for (size_t j = i + 1; j < entities.size(); ++j)
        {
            PhysicalEntity& b = entities[j];
            if (b.motion == PhysicalEntityMotion::Static || !b.collidesWithActors)
                continue;
            if (a.localTriangles.empty() || b.localTriangles.empty())
                continue;

            const AABB boxA = entityWorldAABB(a);
            const AABB boxB = entityWorldAABB(b);
            const glm::vec3 overlap(
                std::min(boxA.max.x, boxB.max.x) - std::max(boxA.min.x, boxB.min.x),
                std::min(boxA.max.y, boxB.max.y) - std::max(boxA.min.y, boxB.min.y),
                std::min(boxA.max.z, boxB.max.z) - std::max(boxA.min.z, boxB.min.z));
            if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f)
                continue;

            const glm::vec3 centerA = (boxA.min + boxA.max) * 0.5f;
            const glm::vec3 centerB = (boxB.min + boxB.max) * 0.5f;
            const glm::vec3 delta = centerB - centerA;
            int axis = 0;
            if (overlap.y < overlap.x && overlap.y <= overlap.z) axis = 1;
            else if (overlap.z < overlap.x && overlap.z < overlap.y) axis = 2;
            glm::vec3 normal(0.0f);
            normal[axis] = delta[axis] >= 0.0f ? 1.0f : -1.0f;

            const float invMassA = a.motion == PhysicalEntityMotion::Dynamic && a.mass > 0.0f
                ? 1.0f / a.mass : 0.0f;
            const float invMassB = b.motion == PhysicalEntityMotion::Dynamic && b.mass > 0.0f
                ? 1.0f / b.mass : 0.0f;
            const float invMassTotal = invMassA + invMassB;
            if (invMassTotal <= 1e-8f)
                continue;

            const float correction = std::max(0.0f, overlap[axis] - kSlop);
            a.transform[3] -= glm::vec4(normal * correction * invMassA / invMassTotal, 0.0f);
            b.transform[3] += glm::vec4(normal * correction * invMassB / invMassTotal, 0.0f);
            a.sleeping = false;
            b.sleeping = false;
            a.sleepTicks = b.sleepTicks = 0;
            a.supportGraceTicks = b.supportGraceTicks = 0;

            const glm::vec3 point = (glm::max(boxA.min, boxB.min) +
                                     glm::min(boxA.max, boxB.max)) * 0.5f;
            const glm::vec3 centerOfMassA = glm::vec3(a.transform[3]) +
                                            a.orientation * a.centerOfMass;
            const glm::vec3 centerOfMassB = glm::vec3(b.transform[3]) +
                                            b.orientation * b.centerOfMass;
            const glm::vec3 relativeVelocity =
                (b.velocity + glm::cross(b.angularVelocity, point - centerOfMassB)) -
                (a.velocity + glm::cross(a.angularVelocity, point - centerOfMassA));
            const float normalSpeed = glm::dot(relativeVelocity, normal);
            if (normalSpeed >= 0.0f)
                continue;

            const float restitution = std::max(a.restitution, b.restitution);
            const float impulseMagnitude = -(1.0f + restitution) * normalSpeed /
                                           invMassTotal;
            const glm::vec3 impulse = normal * impulseMagnitude;
            if (invMassA > 0.0f)
                applyImpulseAtPoint(a, -impulse, point);
            if (invMassB > 0.0f)
                applyImpulseAtPoint(b, impulse, point);
        }
    }
}

} // namespace

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

        CollisionMeshCache& cache = s_entityWorld.collisionMesh;
        cache.triangles.clear();
        cache.triangleAABBs.clear();
        cache.triangles.reserve(e.localTriangles.size());
        cache.triangleAABBs.reserve(e.localTriangles.size());
        for (const CollisionTriangle& lt : e.localTriangles)
        {
            CollisionTriangle wt;
            wt.a = glm::vec3(e.transform * glm::vec4(lt.a, 1.0f));
            wt.b = glm::vec3(e.transform * glm::vec4(lt.b, 1.0f));
            wt.c = glm::vec3(e.transform * glm::vec4(lt.c, 1.0f));
            const glm::vec3 n = glm::cross(wt.b - wt.a, wt.c - wt.a);
            const float len = glm::length(n);
            wt.normal = len > 1e-8f ? n / len : glm::vec3(0.0f, 0.0f, 1.0f);
            cache.triangleAABBs.push_back(makeTriangleAABB(wt));
            cache.triangles.push_back(wt);
        }

        s_entityCandidates.resize(cache.triangles.size());
        std::iota(s_entityCandidates.begin(), s_entityCandidates.end(), 0);

        std::vector<RecoveryContact> contacts =
            // The entity candidate list is complete for this entity; keep the
            // exact entity query intact instead of applying the static-world
            // part filter. Cached triangle AABBs still accelerate the overlap
            // rejection inside the narrowphase.
            collectActorMeshContacts(s_entityWorld, meshes, s_entityCandidates,
                                     actorPos, false, 0.0f);
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
        MimitaImpact::DestructionCutSphere cut;
        cut.localCenter = glm::vec3(0.0f, 0.0f, 0.5f);
        cut.radius = 0.2f;
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

    system.clear();

    if (outSummary)
        *outSummary = report;
    return ok;
}
