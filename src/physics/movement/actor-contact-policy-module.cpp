// 2026-11-07
/* purpose
* live collision policy for actor seam/contact response, streamed-surface
* merge selection, and shared bounded depenetration correction
* this file DOES NOT include World, RecoveryContact, the actor solver, or STL
* types; the host owns contact gathering, correction application, and the
* static union/face-removal execution
*/
#include "modules/api/actor-contact-policy-api.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <module-identity.h>

namespace mimita_collision_policy_module {

using mimita_collision_policy::ContactPolicyV1;
using mimita_collision_policy::SurfacePolicyV1;
using mimita_collision_policy::ContactSolveV1;
using mimita_collision_policy::SolverContactV1;

int solveContacts(ContactSolveV1* request)
{
    if (!request || request->version != mimita_collision_policy::ABI_VERSION ||
        request->structSize != sizeof(*request) || !request->contacts ||
        !request->scratch || request->count > request->capacity ||
        request->capacity > mimita_collision_policy::MAX_SOLVE_CONTACTS)
        return 0;
    auto dot = [](const float a[3], const float b[3]) {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    };
    std::uint32_t count = 0;
    for (std::uint32_t i = 0; i < request->count; ++i) {
        const SolverContactV1& contact = request->contacts[i];
        bool found = false;
        for (std::uint32_t j = 0; j < count; ++j) {
            SolverContactV1& existing = request->scratch[j];
            if (dot(existing.normal, contact.normal) < 0.95f) continue;
            float sum[3];
            for (int axis = 0; axis < 3; ++axis) {
                sum[axis] = existing.normal[axis] + contact.normal[axis];
                existing.point[axis] =
                    (existing.point[axis] + contact.point[axis]) * 0.5f;
            }
            const float length = std::sqrt(dot(sum, sum));
            if (length > 0.00001f)
                for (int axis = 0; axis < 3; ++axis)
                    existing.normal[axis] = sum[axis] / length;
            existing.penetration = std::max(existing.penetration,
                                             contact.penetration);
            found = true;
            break;
        }
        if (!found) request->scratch[count++] = contact;
    }
    std::sort(request->scratch, request->scratch + count,
        [](const SolverContactV1& a, const SolverContactV1& b) {
            return a.penetration > b.penetration;
        });
    const float moveLength = std::sqrt(dot(request->intendedMove,
                                            request->intendedMove));
    if (moveLength > 0.001f && count > 1) {
        float moveDir[3];
        for (int axis = 0; axis < 3; ++axis)
            moveDir[axis] = request->intendedMove[axis] / moveLength;
        std::uint32_t preferred = 0;
        float preferredScore = -std::numeric_limits<float>::max();
        for (std::uint32_t i = 0; i < count; ++i) {
            const SolverContactV1& c = request->scratch[i];
            const float blocks = std::max(0.0f, -dot(moveDir, c.normal));
            const float score = c.penetration + blocks * request->movementBias;
            if (score > preferredScore) {
                preferredScore = score;
                preferred = i;
            }
        }
        std::uint32_t write = 0;
        for (std::uint32_t i = 0; i < count; ++i) {
            const SolverContactV1& c = request->scratch[i];
            const bool shallowSeam = i != preferred &&
                c.penetration <= request->slop &&
                std::fabs(c.normal[2]) < 0.45f;
            if (!shallowSeam) request->scratch[write++] = c;
        }
        if (write) count = write;
    }
    request->maxPenetration = 0.0f;
    for (int axis = 0; axis < 3; ++axis) {
        request->correction[axis] = 0.0f;
        request->weightedNormal[axis] = 0.0f;
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        const SolverContactV1& c = request->scratch[i];
        request->maxPenetration = std::max(request->maxPenetration,
                                            c.penetration);
        const float weight = std::max(c.penetration + request->slop,
                                      request->slop);
        for (int axis = 0; axis < 3; ++axis)
            request->weightedNormal[axis] += c.normal[axis] * weight;
    }
    for (int pass = 0; pass < 6; ++pass) {
        for (std::uint32_t i = 0; i < count; ++i) {
            const SolverContactV1& c = request->scratch[i];
            const float required = c.penetration + request->slop;
            const float satisfied = dot(request->correction, c.normal);
            if (satisfied < required)
                for (int axis = 0; axis < 3; ++axis)
                    request->correction[axis] +=
                        c.normal[axis] * (required - satisfied) * 0.8f;
        }
    }
    const float limit = std::max(0.5f, moveLength + request->slop);
    for (float& component : request->correction)
        component = std::clamp(component, -limit, limit);
    request->manifoldCount = count;
    request->generation = MIMITA_MODULE_GENERATION;
    return 1;
}

int solveActorWorld(mimita_collision_policy::ActorWorldSolveV1* request)
{
    if (!request || request->version != mimita_collision_policy::ABI_VERSION ||
        request->structSize != sizeof(*request) || !request->contacts ||
        !request->scratch || request->count > request->capacity ||
        request->capacity > mimita_collision_policy::MAX_ACTOR_WORLD_CONTACTS)
        return 0;

    ContactSolveV1 solve{};
    solve.version = mimita_collision_policy::ABI_VERSION;
    solve.structSize = sizeof(solve);
    solve.count = request->count;
    solve.capacity = request->capacity;
    solve.contacts = request->contacts;
    solve.scratch = request->scratch;
    solve.slop = request->slop;
    solve.movementBias = request->movementBias;
    for (int axis = 0; axis < 3; ++axis)
        solve.intendedMove[axis] = request->intendedMove[axis];
    if (!solveContacts(&solve))
        return 0;

    request->maxPenetration = solve.maxPenetration;
    request->manifoldCount = solve.manifoldCount;
    for (int axis = 0; axis < 3; ++axis) {
        request->correction[axis] = solve.correction[axis];
        request->weightedNormal[axis] = solve.weightedNormal[axis];
    }

    request->grounded = 0;
    request->blocked = 0;
    for (std::uint32_t i = 0; i < request->count; ++i) {
        const SolverContactV1& contact = request->contacts[i];
        if (contact.normal[2] > 0.7f && contact.penetration > 0.0f)
            request->grounded = 1;
        const float moveDotNormal =
            request->intendedMove[0] * contact.normal[0] +
            request->intendedMove[1] * contact.normal[1] +
            request->intendedMove[2] * contact.normal[2];
        if (moveDotNormal < 0.0f && contact.penetration > 0.0f)
            request->blocked = 1;
    }
    request->generation = MIMITA_MODULE_GENERATION;
    return 1;
}

void fillContactPolicy(ContactPolicyV1* policy)
{
    if (!policy)
        return;
    policy->version = mimita_collision_policy::ABI_VERSION;
    policy->structSize = sizeof(ContactPolicyV1);
    policy->enabled = 1;
    policy->removeTouchingFaceSeams = 1;
    policy->collapseFeatureContacts = 1;
    policy->suppressLowFlushSeams = 1;
    policy->collapseSamePartFeatures = 1;
    policy->opposingFaceSeamDistance = 0.4f;
    policy->opposingFaceNormalDot = -0.98f;
    policy->featureCollapseDistance = 0.1f;
    policy->flushSeamHorizontalDistance = 0.2f;
    policy->flushWallTopHeight = 0.2f;
    policy->flushSupportZTolerance = 0.2f;
    policy->flushSupportHorizontalDistance = 1.2f;
    policy->result = 1;
}

void fillSurfacePolicy(SurfacePolicyV1* policy)
{
    if (!policy)
        return;
    policy->version = mimita_collision_policy::ABI_VERSION;
    policy->structSize = sizeof(SurfacePolicyV1);
    // Face removal is the always-on seam pass. Bounded union is opt-in because
    // Manifold cost on a moving streamed set is not acceptable per tick.
    policy->mergeMode = mimita_collision_policy::SURFACE_FACE_REMOVAL;
    policy->contactTolerance = 0.02f;
    policy->result = 1;
}

void decideActorPair(mimita_collision_policy::ActorPairPolicyV1* policy)
{
    if (!policy)
        return;
    const auto flags = policy->flags;
    const bool staticCollider =
        (flags & mimita_collision_policy::ACTOR_PAIR_STATIC) != 0;
    const bool dead = (flags & mimita_collision_policy::ACTOR_PAIR_DEAD) != 0;
    const bool onGround =
        (flags & mimita_collision_policy::ACTOR_PAIR_UPPER_ON_GROUND) != 0;
    const bool continuing =
        (flags & mimita_collision_policy::ACTOR_PAIR_CONTINUING_SUPPORT) != 0;

    policy->outPush = 0;
    policy->outPushSpeed = 0.0f;
    if (!staticCollider && !dead) {
        const float weakSpeed = policy->weakHitDistanceM /
            std::max(0.01f, policy->weakHitMotionSeconds);
        const float denominator = std::max(
            1e-6f, policy->maxImpactSpeedMps - policy->minImpactSpeedMps);
        const float u = std::clamp(
            (policy->impactSpeed - policy->minImpactSpeedMps) / denominator,
            0.0f, 1.0f);
        const float speedCurve = std::pow(
            u, std::max(0.01f, policy->impactSpeedCurveExponent));
        const float angleScale = policy->impactAngleMinScale +
            (1.0f - policy->impactAngleMinScale) *
                std::pow(std::clamp(policy->directness, 0.0f, 1.0f),
                         std::max(0.01f, policy->impactAngleCurveExponent));
        const float knockback =
            std::max(weakSpeed, policy->maxKnockbackSpeedMps * speedCurve) *
            angleScale;
        const float speed = std::min(knockback, policy->maxKnockbackSpeedMps);
        if (speed > 1e-5f) {
            policy->outPush = 1;
            policy->outPushSpeed = speed;
        }
    }

    policy->outSupport = 0;
    policy->outSupportDeltaZ = 0.0f;
    if (!staticCollider && !dead && (!onGround || continuing)) {
        const float delta = policy->lowerTop - policy->upperBottom;
        const bool withinTopBand =
            policy->upperBottom >= policy->lowerTop - policy->supportTopToleranceM &&
            policy->upperBottom <= policy->lowerTop + policy->supportSnapMaxM;
        const bool notRising =
            policy->relativeVertical <= 0.5f &&
            policy->approachSpeed <= policy->supportMaxImpactSpeedMps;
        if (withinTopBand && notRising) {
            policy->outSupport = 1;
            policy->outSupportDeltaZ = delta;
        }
    }
    policy->outGeneration = MIMITA_MODULE_GENERATION;
}

void fillFinalPosePolicy(mimita_collision_policy::FinalPosePolicyV1* policy)
{
    if (!policy)
        return;
    policy->version = mimita_collision_policy::ABI_VERSION;
    policy->structSize = sizeof(*policy);
    policy->maxSolidPushOut = 2.0f;
    policy->pushSkin = 0.02f;
    policy->contactEpsilon = 0.001f;
    policy->clearanceEpsilon = 0.0005f;
    policy->maxContainmentIterations = 6;
    policy->maxCorrectionPerIteration = 1.0f;
    policy->maxTotalCorrection = 3.0f;
    policy->outGeneration = MIMITA_MODULE_GENERATION;
}

} // namespace mimita_collision_policy_module
