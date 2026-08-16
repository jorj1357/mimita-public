// 07 21 2026, 17 25
/* purpose
* Implements core shared helpers for Player collision recovery and contact response.
* Emits neutral movement contact facts while preserving existing collision projection behavior.
* Keeps local GLB/block collision files using one contact adapter.
* Does NOT own movement reset formulas, network packets, rendering, audio, or damage.
* Does NOT change projectile authority, weapon firing, server reconciliation, or input polling.
* Does NOT replace specialized GLB, block, body, or safety collision passes.
*/

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
#include <cmath>
#include <limits>
#include <string>
#include <glm/glm.hpp>

#include "physics/config.h"
#include "world/world.h"
#include "entities/player.h"
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "config/player-settings.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/movement-step.h"

#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)
#define LOG_COLLISION(K, ...) Debug::logThrottled(Debug::Category::Collision, K, 0.25f, __VA_ARGS__)

void recoverInvalidPlayerCollisionState(Player& p, const glm::vec3& frameStart, const char* phase)
{
    if (isFiniteVec3(p.pos) && isFiniteVec3(p.vel) && isFiniteVec3(p.externalImpulse))
        return;

    glm::vec3 recovery = isFiniteVec3(frameStart) ? frameStart : p.respawnPosition;
    if (!isFiniteVec3(recovery))
        recovery = glm::vec3(0.0f);

    Debug::log(Debug::Category::Collision,
        "[COLLISION RECOVER] invalid state phase=%s pos=(%.3f %.3f %.3f) vel=(%.3f %.3f %.3f) external=(%.3f %.3f %.3f)\n",
        phase ? phase : "unknown",
        p.pos.x, p.pos.y, p.pos.z,
        p.vel.x, p.vel.y, p.vel.z,
        p.externalImpulse.x, p.externalImpulse.y, p.externalImpulse.z);

    p.pos = recovery;
    p.vel = glm::vec3(0.0f);
    p.externalImpulse = glm::vec3(0.0f);
    p.syncLegacyStateToLayers();
    p.updateModelWorldTransforms();
}

static uint32_t movementSurfaceIdFromTriangle(int triangleIndex)
{
    return triangleIndex >= 0 ? static_cast<uint32_t>(triangleIndex + 1) : 0;
}

MovementContactKind classifyCollisionMovementContactKind(const glm::vec3& normal,
                                                         bool grounded,
                                                         bool step)
{
    MovementConfig config;
    config.walkableSlopeDot = MAX_WALKABLE_SLOPE_DOT;
    return classifyMovementContactKindFromNormal(normal, config, grounded, step);
}

void appendPlayerMovementContact(Player& p,
                                 MovementContactKind kind,
                                 const glm::vec3& normal,
                                 glm::vec3 point,
                                 float penetration,
                                 int triangleIndex,
                                 MovementContactSource source)
{
    MovementContact contact;
    if (source == MovementContactSource::StaticWorld) {
        contact = makeStaticWorldMovementContact(
            kind,
            p.movementSimulationTick,
            MovementLifecycleIdentity{p.spawnGeneration, 0},
            point,
            normal,
            movementSurfaceIdFromTriangle(triangleIndex),
            penetration);
    } else {
        contact = makeEntityMovementContact(
            kind,
            source,
            0,
            0,
            0,
            p.movementSimulationTick,
            MovementLifecycleIdentity{p.spawnGeneration, 0},
            point,
            normal,
            penetration);
        contact.surfaceId = movementSurfaceIdFromTriangle(triangleIndex);
        contact.penetrationDepth = penetration;
    }
    p.movementContacts.addDeduplicated(contact);
}

void appendPlayerMovementContactForNormal(Player& p,
                                          bool grounded,
                                          bool step,
                                          const glm::vec3& normal,
                                          glm::vec3 point,
                                          float penetration,
                                          int triangleIndex,
                                          MovementContactSource source)
{
    appendPlayerMovementContact(
        p,
        classifyCollisionMovementContactKind(normal, grounded, step),
        normal,
        point,
        penetration,
        triangleIndex,
        source);
}

void applyCollisionContact(
    Player& p,
    bool& groundedThisFrame,
    const glm::vec3& normal,
    glm::vec3 point,
    float penetration,
    int triangleIndex,
    const char* label
) {
    if (DebugConfig::COLLISION_VERBOSE)
        Debug::log(Debug::Category::Collision,
                   "[CONTACT] label=%s tri=%d normal=(%.3f %.3f %.3f) penetration=%.4f\n",
                   label, triangleIndex, normal.x, normal.y, normal.z, penetration);

    const PlayerSettings& cfg = GetPlayerSettings();

    p.ground.realWorldContactThisFrame = true;
    p.ground.hasWorldContact = true;
    p.ground.worldContactLostTimer = 0.033f;

    if (normal.z > MAX_WALKABLE_SLOPE_DOT)
    {
        Capsule cap = p.getCapsule();
        float feetZ = cap.a.z - cap.r;
        if (point.z <= feetZ + 0.15f)
        {
            LOG_COLLISION("contact_ground", "[CONTACT] GROUND label=%s tri=%d normal=(%.3f,%.3f,%.3f)"
                          " pointZ=%.3f feetZ=%.3f pen=%.4f velZ=%.3f",
                          label ? label : "?", triangleIndex, normal.x, normal.y, normal.z,
                          point.z, feetZ, penetration, p.vel.z);

            groundedThisFrame = true;
            appendPlayerMovementContact(
                p, MovementContactKind::Ground, normal, point, penetration, triangleIndex);

            respondVelocityAgainstNormal(p, normal);

            if (p.externalImpulse.z > 0.0f)
                p.externalImpulse.z = 0.0f;

            DebugVis::recordGroundNormal(point, normal, label);
        }
        else
        {
            PHYS_LOG("[GROUND REJECT] source=%s tri=%d normal=(%.3f %.3f %.3f) point=(%.3f %.3f %.3f) feetZ=%.3f dist=%.3f pos=(%.3f %.3f %.3f) reason=contact_above_feet\n",
                label, triangleIndex, normal.x, normal.y, normal.z,
                point.x, point.y, point.z, feetZ, point.z - feetZ,
                p.pos.x, p.pos.y, p.pos.z);
            appendPlayerMovementContact(
                p, MovementContactKind::StaticWorld, normal, point, penetration, triangleIndex);
            projectVelocityAgainstNormal(p, normal);
        }
    }
    else if (normal.z > 0.0f)
    {
        LOG_COLLISION("contact_slope", "[CONTACT] SLOPE label=%s normal=(%.3f,%.3f,%.3f) pen=%.4f vel projected",
                      label ? label : "?", normal.x, normal.y, normal.z, penetration);
        appendPlayerMovementContact(
            p, MovementContactKind::Slope, normal, point, penetration, triangleIndex);
        respondVelocityAgainstNormal(p, normal);
    }
    else if (normal.z < -MAX_WALKABLE_SLOPE_DOT)
    {
        LOG_COLLISION("contact_ceiling", "[CONTACT] CEILING label=%s normal=(%.3f,%.3f,%.3f) velZ=%.3f",
                      label ? label : "?", normal.x, normal.y, normal.z, p.vel.z);
        appendPlayerMovementContact(
            p, MovementContactKind::Ceiling, normal, point, penetration, triangleIndex);
        respondVelocityAgainstNormal(p, normal);
    }
    else
    {
        LOG_COLLISION("contact_wall", "[CONTACT] WALL label=%s normal=(%.3f,%.3f,%.3f) velZ=%.3f",
                      label ? label : "?", normal.x, normal.y, normal.z, p.vel.z);
        respondVelocityAgainstNormal(p, normal);
        appendPlayerMovementContact(
            p, MovementContactKind::Wall, normal, point, penetration, triangleIndex);
    }

    DebugVis::recordContact(point, normal, penetration, triangleIndex, label);
}

glm::vec3 solveBatchedCorrection(
    const std::vector<RecoveryContact>& contacts,
    float slop,
    float* outMaxPenetration,
    glm::vec3* outWeightedNormal,
    glm::vec3 intendedMove,
    glm::vec3 debugPosition
) {
    auto t0 = std::chrono::steady_clock::now();
    const PlayerSettings& cfg = GetPlayerSettings();
    std::vector<RecoveryContact> manifold;
    for (const RecoveryContact& contact : contacts) {
        RecoveryContact merged = contact;
        bool found = false;
        for (RecoveryContact& existing : manifold) {
            float alignment = glm::dot(existing.normal, contact.normal);
            if (alignment >= 0.95f) {
                existing.normal = glm::normalize(existing.normal + contact.normal);
                existing.point = (existing.point + contact.point) * 0.5f;
                existing.penetration = std::max(existing.penetration, contact.penetration);
                found = true;
                break;
            }
        }
        if (!found)
            manifold.push_back(merged);
    }

    int manifoldIn = (int)manifold.size();

    std::sort(manifold.begin(), manifold.end(),
        [](const RecoveryContact& a, const RecoveryContact& b) {
            return a.penetration > b.penetration;
        });

    if (glm::dot(intendedMove, intendedMove) > 0.000001f && manifold.size() > 1) {
        glm::vec3 moveDir = glm::normalize(intendedMove);
        size_t preferred = 0;
        float preferredScore = -std::numeric_limits<float>::max();
        for (size_t i = 0; i < manifold.size(); ++i) {
            float blocks = std::max(0.0f, -glm::dot(moveDir, manifold[i].normal));
            float score = manifold[i].penetration + blocks * cfg.collisionMovementBias;
            if (score > preferredScore) {
                preferredScore = score;
                preferred = i;
            }
        }
        std::vector<RecoveryContact> filtered;
        for (size_t i = 0; i < manifold.size(); ++i) {
            bool shallowSeam = i != preferred &&
                manifold[i].penetration <= slop &&
                std::fabs(manifold[i].normal.z) < 0.45f;
            if (!shallowSeam) {
                filtered.push_back(manifold[i]);
            }
        }
        if (!filtered.empty())
            manifold.swap(filtered);
    }

    glm::vec3 correction(0.0f);
    glm::vec3 weightedNormal(0.0f);
    float maxPenetration = 0.0f;

    for (const RecoveryContact& c : manifold)
    {
        maxPenetration = std::max(maxPenetration, c.penetration);
        weightedNormal += c.normal * std::max(c.penetration + slop, slop);
    }

    constexpr int SOLVER_PASSES = 6;
    constexpr float RELAXATION = 0.8f;
    for (int pass = 0; pass < SOLVER_PASSES; ++pass)
    {
        for (const RecoveryContact& c : manifold)
        {
            float required = c.penetration + slop;
            float satisfied = glm::dot(correction, c.normal);
            if (satisfied < required)
                correction += c.normal * (required - satisfied) * RELAXATION;
        }
    }

    constexpr float MAX_AXIS_CORRECTION = 0.5f;
    correction.x = glm::clamp(correction.x, -MAX_AXIS_CORRECTION, MAX_AXIS_CORRECTION);
    correction.y = glm::clamp(correction.y, -MAX_AXIS_CORRECTION, MAX_AXIS_CORRECTION);
    correction.z = glm::clamp(correction.z, -MAX_AXIS_CORRECTION, MAX_AXIS_CORRECTION);

    auto t1 = std::chrono::steady_clock::now();
    float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

    LOG_COLLISION("solver_summary", "[SOLVER] contacts=%zu manifoldIn=%d manifoldOut=%zu passes=%d "
                  "maxPen=%.4f corr=(%.4f %.4f %.4f) elapsedMs=%.2f\n",
                  contacts.size(), manifoldIn, manifold.size(), SOLVER_PASSES,
                  maxPenetration, correction.x, correction.y, correction.z, elapsedMs);

    if (manifold.size() > 20) {
        Debug::warn(Debug::Category::Collision,
            "[SOLVER WARNING] large manifold: contacts=%zu manifold=%zu maxPen=%.4f\n",
            contacts.size(), manifold.size(), maxPenetration);
    }

    if (outMaxPenetration)
        *outMaxPenetration = maxPenetration;
    if (outWeightedNormal)
        *outWeightedNormal = weightedNormal;

    return correction;
}
