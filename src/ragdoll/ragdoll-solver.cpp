// 09 12 2026
/* purpose
* Implements the component-driven ragdoll solve core.
* Does NOT own input, networking, rendering, or the domain clock.
*/
#include "ragdoll/ragdoll-solver.h"

#include <algorithm>
#include <cmath>

#include <glm/gtx/quaternion.hpp>

#include "physics/physical-body.h"
#include "ragdoll/ragdoll-mode-config.h"

namespace Ragdoll {
namespace Solver {

namespace {
constexpr float kGravity = 9.81f;

glm::vec3 quatToRotationVector(const glm::quat& q)
{
    glm::quat n = glm::normalize(q);
    float w = glm::clamp(n.w, -1.0f, 1.0f);
    float angle = 2.0f * std::acos(w);
    float s = std::sqrt(std::max(0.0f, 1.0f - w * w));
    if (s < 1e-5f) return glm::vec3(0.0f);
    glm::vec3 axis(n.x, n.y, n.z);
    axis /= s;
    if (angle > 3.14159265f) angle -= 2.0f * 3.14159265f;
    return axis * angle;
}

// Higher live stiffness tightens the position correction; 1.0 is neutral.
float effectiveBeta(float base, float stiffness)
{
    return glm::clamp(base * glm::max(stiffness, 0.0f), 0.0f, 1.0f);
}
} // namespace

void solveJoints(RagdollBody& b, const RagdollModeConfigData& cfg,
                 int iterations, bool positionPass, const SolveParams& params)
{
    for (int iter = 0; iter < iterations; ++iter) {
        for (int pi = 0; pi < (int)b.parts.size(); ++pi) {
            RagdollModePart& part = b.parts[pi];
            if (part.parentIndex < 0) continue;
            if (part.parentIndex >= (int)b.parts.size()) continue;

            RigidBody& child = part.body;
            RigidBody& parent = b.parts[part.parentIndex].body;

            glm::vec3 parentAnchor = parent.position
                + parent.orientation * part.parentLocalAnchor;
            glm::vec3 childAnchor = child.position
                + child.orientation * part.childLocalAnchor;

            const float beta = effectiveBeta(cfg.jointPositionBeta, params.stiffness);

            // Arms may stretch only while reaching (extending) and not holding a
            // grab. While grabbing the shoulder stays rigid so the pinned hand
            // pivots the arm at the shoulder instead of letting it float/spin
            // about its center. Otherwise the joint is rigid too.
            float stretch = 0.0f;
            if (part.maxStretch > 0.0f) {
                const bool isLeft = (pi == b.leftArmIndex);
                const bool isRight = (pi == b.rightArmIndex);
                const bool grabbing = (isLeft && b.leftGrab.active) || (isRight && b.rightGrab.active);
                const bool extending = (isLeft && b.leftArmExtending) || (isRight && b.rightArmExtending);
                if (extending && !grabbing)
                    stretch = part.maxStretch;
            }

            if (stretch > 0.0f) {
                // Anchors coincide at bind, so the allowed separation is the
                // stretch amount itself (base 0), not restLength.
                solvePointJointMaxDistanceVelocity(parent, parentAnchor, child, childAnchor, stretch);
                if (positionPass)
                    solvePointJointMaxDistance(parent, parentAnchor, child, childAnchor, stretch, beta);
            } else {
                solvePointJointVelocity(parent, parentAnchor, child, childAnchor);
                if (positionPass)
                    solvePointJointPosition(parent, parentAnchor, child, childAnchor, beta);
            }
        }

        if (positionPass)
            solveRotationLimits(b, -1.0f, cfg);
    }
}

void solveRotationLimits(RagdollBody& b, float betaOverride,
                         const RagdollModeConfigData& cfg)
{
    const float kBeta = (betaOverride >= 0.0f)
        ? glm::clamp(betaOverride, 0.0f, 1.0f)
        : glm::clamp(cfg.limitPositionBeta, 0.0f, 1.0f);

    for (int pi = 0; pi < (int)b.parts.size(); ++pi) {
        RagdollModePart& part = b.parts[pi];
        if (!part.hasRotationLimits) continue;
        if (part.parentIndex < 0 || part.parentIndex >= (int)b.parts.size()) continue;

        // An actively extending arm must be free to point along camera-forward.
        if ((pi == b.leftArmIndex && b.leftArmExtending) ||
            (pi == b.rightArmIndex && b.rightArmExtending))
            continue;

        RigidBody& child = part.body;
        RigidBody& parent = b.parts[part.parentIndex].body;

        glm::quat currentRelative = glm::normalize(
            glm::inverse(parent.orientation) * child.orientation);
        glm::quat delta = glm::normalize(
            currentRelative * glm::inverse(part.bindRelativeRotation));

        glm::vec3 rv = quatToRotationVector(delta);
        glm::vec3 clamped(
            glm::clamp(rv.x, part.rotMinDeg.x, part.rotMaxDeg.x),
            glm::clamp(rv.y, part.rotMinDeg.y, part.rotMaxDeg.y),
            glm::clamp(rv.z, part.rotMinDeg.z, part.rotMaxDeg.z));

        glm::vec3 rejected = rv - clamped;
        if (glm::length(rejected) < 1e-6f) continue;

        float invA = child.invInertia;
        float invB = parent.invInertia;
        float total = invA + invB;
        if (total < 1e-8f) continue;

        glm::vec3 worldCorrection = parent.orientation * (-rejected);
        rotateBody(child,  worldCorrection * (kBeta * invA / total));
        rotateBody(parent, -worldCorrection * (kBeta * invB / total));
    }
}

void solveGrabs(RagdollBody& b, const RagdollModeConfigData& cfg, int iterations)
{
    float beta = glm::clamp(0.9f - cfg.grabCompliance * 20.0f, 0.1f, 0.95f);

    auto solve = [&](RagdollGrabState& grab) {
        if (!grab.active || grab.partIndex < 0
            || grab.partIndex >= (int)b.parts.size()) return;
        RigidBody& body = b.parts[grab.partIndex].body;
        const float grip = glm::clamp(grab.strength, 0.0f, 1.0f);
        const float b2 = glm::clamp(beta * grip + 1.0f * (1.0f - grip), 0.0f, 1.0f);
        for (int i = 0; i < iterations; ++i) {
            glm::vec3 hand = body.position + body.orientation * grab.handLocalAnchor;
            if (grab.targetPart >= 0 && grab.targetPart != grab.partIndex
                && grab.targetPart < (int)b.parts.size()) {
                // Entity-to-entity: a two-body point constraint. Cancel the
                // relative velocity at the shared point, then pull the anchors
                // together so momentum is exchanged instead of injected.
                RigidBody& target = b.parts[grab.targetPart].body;
                glm::vec3 targetAnchor = target.position
                    + target.orientation * grab.targetLocalAnchor;
                solvePointJointVelocity(body, hand, target, targetAnchor);
                solvePointJointPosition(body, hand, target, targetAnchor, b2);
            } else {
                // Cancel the anchor velocity first so gravity cannot accumulate,
                // then project the position so the grip stays effectively rigid.
                solvePointToWorldVelocity(body, hand);
                solvePointToWorld(body, hand, grab.grabPoint, b2);
            }
        }
    };

    solve(b.leftGrab);
    solve(b.rightGrab);
}

void selfCollision(RagdollBody& b, const RagdollModeConfigData& cfg)
{
    for (int i = 0; i < (int)b.parts.size(); ++i) {
        for (int j = i + 1; j < (int)b.parts.size(); ++j) {
            // Directly-jointed parts overlap at their shared joint by design, so
            // they collide everywhere except a small sphere around that joint.
            const bool connected = (b.parts[i].parentIndex == j || b.parts[j].parentIndex == i);
            glm::vec3 excludePoint(0.0f);
            float excludeRadius = 0.0f;
            if (connected) {
                int childIdx = (b.parts[i].parentIndex == j) ? i : j;
                int parentIdx = b.parts[childIdx].parentIndex;
                if (parentIdx < 0 || parentIdx >= (int)b.parts.size()) continue;
                const RagdollModePart& c = b.parts[childIdx];
                const RigidBody& p = b.parts[parentIdx].body;
                excludePoint = p.position + p.orientation * c.parentLocalAnchor;
                excludeRadius = c.body.capsuleRadius + p.capsuleRadius;
            }
            collideBodies(b.parts[i].body, b.parts[j].body, excludePoint, excludeRadius,
                          cfg.selfCollisionBeta, cfg.selfCollisionSkin,
                          cfg.selfCollisionMaxCorrection);
        }
    }
}

void solveSubstep(RagdollBody& b, const World& world, float dt,
                  const RagdollModeConfigData& cfg, const SolveParams& params)
{
    const int iterations = std::max(1, params.iterations);

    glm::vec3 gravity(0.0f, 0.0f, -kGravity * params.gravityScale);
    for (auto& part : b.parts)
        integrate(part.body, gravity, dt);

    solveJoints(b, cfg, iterations, true, params);
    solveGrabs(b, cfg, iterations);

    if (cfg.worldCollision) {
        for (auto& part : b.parts)
            collideWithWorld(part.body, world, dt);
    }

    if (cfg.selfCollision)
        selfCollision(b, cfg);

    solveJoints(b, cfg, std::max(1, iterations / 2), true, params);
    solveGrabs(b, cfg, std::max(1, iterations / 2));

    if (cfg.selfCollision) {
        for (int i = 0; i < cfg.selfCollisionIterations; ++i) selfCollision(b, cfg);
    }

    if (cfg.worldCollision) {
        for (auto& part : b.parts)
            depenetrateWorld(part.body, world, 2);
    }

    for (auto& part : b.parts) {
        if (part.body.stopLinearSpeed > 0.0f &&
            glm::length(part.body.linearVelocity) < part.body.stopLinearSpeed)
            part.body.linearVelocity = glm::vec3(0.0f);
        if (part.body.stopAngularSpeed > 0.0f &&
            glm::length(part.body.angularVelocity) < part.body.stopAngularSpeed)
            part.body.angularVelocity = glm::vec3(0.0f);
        // Live damping relaxes velocity each substep; 1.0 is neutral.
        if (params.damping > 1.0f) {
            const float relax = glm::clamp(1.0f - (params.damping - 1.0f) * dt, 0.0f, 1.0f);
            part.body.linearVelocity *= relax;
            part.body.angularVelocity *= relax;
        }
    }

    solveRotationLimits(b, 1.0f, cfg);
}

} // namespace Solver
} // namespace Ragdoll
