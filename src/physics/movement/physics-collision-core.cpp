#include <algorithm>
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

#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

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
            PHYS_LOG("[GROUND SET] source=%s tri=%d normal=(%.3f %.3f %.3f) point=(%.3f %.3f %.3f) feetZ=%.3f dist=%.3f pos=(%.3f %.3f %.3f) reason=walkable_contact\n",
                label, triangleIndex, normal.x, normal.y, normal.z,
                point.x, point.y, point.z, feetZ, feetZ - point.z,
                p.pos.x, p.pos.y, p.pos.z);

            groundedThisFrame = true;
            applyTouchResets(p);

            if (p.vel.z < 0.0f)
                p.vel.z = 0.0f;

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
            projectVelocityAgainstNormal(p, normal);
        }
    }
    else if (normal.z > 0.0f)
    {
        applyTouchResets(p);
        projectVelocityAgainstNormal(p, normal);
    }
    else if (normal.z < -MAX_WALKABLE_SLOPE_DOT)
    {
        applyTouchResets(p);
        if (p.vel.z > 0.0f)
            p.vel.z = 0.0f;
    }
    else
    {
        projectVelocityAgainstNormal(p, normal);
        applyTouchResets(p);
    }

    DebugVis::recordContact(point, normal, penetration, triangleIndex, label);
}

void applyTouchResets(Player& p)
{
    p.jump.airJumpsLeft = AIR_JUMPS_MAX;
    p.jump.airJumpArmed = true;
    p.jump.airJumpLocked = false;
    p.dash.dashAvailable = true;
    p.groundReturn.available = true;
    p.dash.downDashAvailable = true;
    p.freeze.freezeAvailable = true;
}

glm::vec3 solveBatchedCorrection(
    const std::vector<RecoveryContact>& contacts,
    float slop,
    float* outMaxPenetration,
    glm::vec3* outWeightedNormal,
    glm::vec3 intendedMove,
    glm::vec3 debugPosition
) {
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

    if (outMaxPenetration)
        *outMaxPenetration = maxPenetration;
    if (outWeightedNormal)
        *outWeightedNormal = weightedNormal;

    return correction;
}
